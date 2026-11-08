// Copyright (c) 2026 The QtDMM developers
// SPDX-License-Identifier: GPL-3.0-or-later
#include "recording/recordingfile.h"

#include <QBuffer>
#include <QCoreApplication>
#include <QFile>
#include <QFileInfo>
#include <QJsonDocument>
#include <QRegularExpression>
#include <QSaveFile>
#include <QTemporaryDir>
#include <QTextStream>

#include <cmath>

#include "core/limittest.h"
#include "core/siprefix.h"
#include "recording/spreadsheet.h"
#include "recording/zipfile.h"

namespace
{
QString tr(const char *text)
{
  return QCoreApplication::translate("RecordingFile", text);
}

void setError(QString *error, const QString &text)
{
  if (error)
    *error = text;
}
}

QStringList RecordingFile::devices(const Recording &recording)
{
  QStringList out;
  for (const RecordingColumn &col : recording.extra)
    if (!col.device.isEmpty() && col.device != recording.device && !out.contains(col.device))
      out << col.device;
  return out;
}

Recording RecordingFile::devicePart(const Recording &recording, const QString &device)
{
  Recording part;
  part.start = recording.start;
  part.sampleTimeTenths = recording.sampleTimeTenths;
  part.times = recording.times;
  part.meta = recording.meta;   // the marks are the recording's
  bool haveMain = device == recording.device;
  if (haveMain)
  {
    part.unit = recording.unit;
    part.values = recording.values;
    part.states = recording.states;
    part.limits = recording.limits;
  }
  for (const RecordingColumn &col : recording.extra)
  {
    if (col.device != device)
      continue;
    if (!haveMain && col.id == 0)
    {
      part.unit = col.unit;
      part.values = col.values;
      part.states = col.states;
      part.limits = col.limits;
      haveMain = true;
      continue;
    }
    RecordingColumn own = col;
    own.device.clear();
    part.extra << own;
  }
  // only other values of it: no main value in any row
  if (!haveMain)
  {
    part.values.fill(qQNaN(), recording.values.size());
    part.states.fill(CellState::None, recording.values.size());
  }
  return part;
}

double RecordingFile::unitScaleFactor(const QString &unit, const QString &baseUnit)
{
  if (unit != baseUnit && unit.endsWith(baseUnit))
    return SiPrefix::factor(unit.left(unit.size() - baseUnit.size()));
  return 1.0;
}

namespace
{
const char *stateText(CellState state)
{
  switch (state)
  {
    case CellState::Ok:               return "ok";
    case CellState::Overload:         return "OL";
    case CellState::NegativeOverload: return "-OL";
    case CellState::Gap:              return "gap";
    case CellState::NoValue:          return "none";
    case CellState::Fault:            return "fault";
    case CellState::None:             break;
  }
  return "";
}

// a column's name read back: "Sim B: value 2" is the meter "Sim B" (empty:
// this one), the word "value" and the number 2 (0: none)
struct ColumnName
{
  QString device;
  QString word;
  int number = 0;
};

ColumnName columnName(const QString &text)
{
  ColumnName name;
  QString rest = text;
  const qsizetype colon = text.lastIndexOf(QLatin1String(": "));
  if (colon >= 0)
  {
    name.device = text.left(colon);
    rest = text.mid(colon + 2);
  }
  const QStringList parts = rest.split(' ');
  name.word = parts.value(0);
  name.number = parts.size() == 2 ? parts[1].toInt() : 0;
  return name;
}

std::optional<CellState> stateFrom(const QString &text)
{
  for (CellState s : { CellState::Ok, CellState::Overload, CellState::NegativeOverload, CellState::Gap,
                       CellState::NoValue, CellState::Fault })
    if (text.compare(QLatin1String(stateText(s)), Qt::CaseInsensitive) == 0)
      return s;
  if (text.isEmpty())
    return CellState::None;
  return std::nullopt;
}

// the value of a state: an overload's NaN carries its sign
double stateValue(CellState state, double value)
{
  switch (state)
  {
    case CellState::Ok:               return value;
    case CellState::NegativeOverload: return std::copysign(qQNaN(), -1.0);
    default:                          return qQNaN();
  }
}

// one value, unit and status of a row: what the column group of the file holds
struct Cell
{
  double value = qQNaN();
  QString unit;
  CellState state = CellState::None;
};

// "1.25", "-3", "nan" (a gap or an overload), empty (not there)
std::optional<Cell> cellFrom(const QString &value, const QString &unit, const QString *status)
{
  static const QRegularExpression number(R"(^-?\d+(?:\.\d+)?(?:[eE][-+]?\d+)?$)");
  Cell c;
  c.unit = unit;
  if (value.isEmpty())
  {
    if (status && !status->isEmpty())
      return std::nullopt;
    return c;
  }
  const bool nan = value.compare("nan", Qt::CaseInsensitive) == 0;
  if (!nan && !number.match(value).hasMatch())
    return std::nullopt;
  if (status)
  {
    const std::optional<CellState> st = stateFrom(*status);
    if (!st || *st == CellState::None || (*st == CellState::Ok) == nan)
      return std::nullopt;
    c.state = *st;
  }
  else
    c.state = nan ? CellState::Gap : CellState::Ok;
  c.value = nan ? qQNaN() : value.toDouble();
  return c;
}
}

std::optional<Recording> RecordingFile::read(const QString &path, QString *error)
{
  QFile file(path);
  if (!file.open(QIODevice::ReadOnly))
  {
    setError(error, tr("Cannot open file."));
    return std::nullopt;
  }
  // a QtDMM recording: the CSV and meta.json in a ZIP
  QBuffer unpacked;
  QJsonObject meta;
  QIODevice *in = &file;
  if (file.peek(4) == QByteArray("PK\x03\x04", 4))
  {
    const QByteArray zip = file.readAll();
    const std::optional<QByteArray> csv = zipEntry(zip, QStringLiteral("recording.csv"));
    if (!csv)
    {
      setError(error, tr("No recording in the file (recording.csv is missing)."));
      return std::nullopt;
    }
    if (const std::optional<QByteArray> json = zipEntry(zip, QStringLiteral("meta.json")))
      meta = QJsonDocument::fromJson(*json).object();
    unpacked.setData(*csv);
    unpacked.open(QIODevice::ReadOnly);
    in = &unpacked;
  }
  QTextStream ts(in);

  QString line = ts.readLine();
  if (line.isNull())
  {
    setError(error, tr("Oops! Seems not to be a valid file"));
    return std::nullopt;
  }
  // the columns of each value: value;unit, since 26.3 value;unit;status
  bool withStatus = false;
  int groups = 1;
  bool header = false;
  QStringList headerNames;
  QStringList limitNames;   // the limit columns at the end: "limit", "limit 2" ...
  if (line.startsWith("timestamp"))   // the CSV header
  {
    headerNames = line.split(';');
    while (headerNames.size() > 5 && columnName(headerNames.last()).word == QLatin1String("limit"))
      limitNames.prepend(headerNames.takeLast());
    withStatus = columnName(headerNames.value(4)).word == QLatin1String("status");
    groups = withStatus ? qMax(1, int(headerNames.size() - 2) / 3) : 1;
    header = true;
    line = ts.readLine();
    if (line.isNull())
    {
      setError(error, tr("File contains only header"));
      return std::nullopt;
    }
  }

  static const QRegularExpression reLegacy(
    R"(^(?<day>\d{2})\.(?<month>\d{2})\.(?<year>\d{4})\t(?<hour>\d{2}):(?<minute>\d{2}):(?<second>\d{2})(?::(?<ms>\d{1,3}))?\t(?<value>-?\d+(?:\.\d+)?(?:[eE][-+]?\d+)?|nan)\t(?<unit>.*)$)",
    QRegularExpression::CaseInsensitiveOption);
  static const QRegularExpression reTime(
    R"(^(?<year>\d{4})-(?<month>\d{2})-(?<day>\d{2})T(?<hour>\d{2}):(?<minute>\d{2}):(?<second>\d{2})[,\.](?<ms>\d{1,3})$)");
  static const QRegularExpression reDelta(R"(^(?:-?\d+(?:\.\d+)?(?:[eE][-+]?\d+)?|nan)$)", QRegularExpression::CaseInsensitiveOption);

  const bool isLegacy = reLegacy.match(line).hasMatch();
  // a CSV without the header: the columns say which format
  if (!isLegacy && !header && line.count(';') >= 4)
  {
    withStatus = true;
    groups = qMax(1, int(line.count(';') - 1) / 3);
  }
  Recording rec;
  rec.extra.resize(groups - 1);
  // "Sim A: value": the main value's meter (since 26.4)
  if (header)
    rec.device = columnName(headerNames.value(2)).device;
  // "value 3": the column group is value 3 (id 2); "Sim B: value" another
  // meter's main value
  for (int g = 1; g < groups; ++g)
  {
    const ColumnName name = columnName(headerNames.value(2 + 3 * g));
    rec.extra[g - 1].device = name.device;
    rec.extra[g - 1].id = name.word == QLatin1String("value") ? qMax(0, name.number - 1) : 0;
  }
  // each limit column's group: "limit" the main value, "limit 3" value 3,
  // "Sim B: limit" another meter's main value
  QList<int> limitGroup;
  for (const QString &text : std::as_const(limitNames))
  {
    const ColumnName name = columnName(text);
    int g = name.device == rec.device && name.number <= 1 ? 0 : -1;
    for (int e = 0; g < 0 && e < rec.extra.size(); ++e)
    {
      const RecordingColumn &col = rec.extra[e];
      if (col.device == name.device
          && (col.device.isEmpty() ? (col.id > 0 ? col.id : e + 1) + 1 == name.number : col.id == qMax(0, name.number - 1)))
        g = e + 1;
    }
    limitGroup << g;
  }
  QDateTime end;
  auto when = [](const QRegularExpressionMatch &m)
  {
    return QDateTime(QDate(m.captured("year").toInt(), m.captured("month").toInt(), m.captured("day").toInt()),
                     QTime(m.captured("hour").toInt(), m.captured("minute").toInt(), m.captured("second").toInt(),
                           m.captured("ms").toInt()));
  };
  do
  {
    if (!line.trimmed().isEmpty())
    {
      QDateTime at;
      QList<Cell> cells;
      bool valid = true;
      if (isLegacy)
      {
        const QRegularExpressionMatch match = reLegacy.match(line);
        const std::optional<Cell> c = match.hasMatch() ? cellFrom(match.captured("value"), match.captured("unit"), nullptr)
                                                       : std::nullopt;
        valid = c.has_value();
        if (valid)
        {
          at = when(match);
          cells << *c;
        }
      }
      else
      {
        const QStringList f = line.split(';');
        const QRegularExpressionMatch match = reTime.match(f.value(0));
        valid = match.hasMatch() && reDelta.match(f.value(1)).hasMatch()
                && (withStatus ? f.size() == 2 + 3 * groups + limitNames.size() : f.size() >= 4);
        for (int g = 0; valid && g < groups; ++g)
        {
          // before the status the unit was all the rest of the row
          const QString unit = withStatus ? f.value(3 + 3 * g) : f.mid(3).join(';');
          const QString status = f.value(4 + 3 * g);
          const std::optional<Cell> c = cellFrom(f.value(2 + 3 * g), unit, withStatus ? &status : nullptr);
          valid = c && (g > 0 || c->state != CellState::None);
          if (valid)
            cells << *c;
        }
        if (valid)
          at = when(match);
      }
      if (!valid)
      {
        setError(error, tr("Oops! Seems not to be a valid file"));
        return std::nullopt;
      }
      if (rec.values.isEmpty())
      {
        // the recording's unit is the first row's without its SI prefix;
        // every row is scaled to that base unit
        rec.unit = SiPrefix::split(cells[0].unit).baseUnit;
        rec.start = at;
      }
      end = at;
      // the old tab-separated export was always on the grid, often to the
      // second only
      if (!isLegacy)
        rec.times << rec.start.msecsTo(at);
      rec.values << stateValue(cells[0].state, cells[0].value * unitScaleFactor(cells[0].unit, rec.unit));
      if (withStatus)
        rec.states << cells[0].state;
      for (int g = 1; g < groups; ++g)
      {
        RecordingColumn &col = rec.extra[g - 1];
        const Cell &c = cells[g];
        if (col.unit.isEmpty() && c.state != CellState::None)
          col.unit = SiPrefix::split(c.unit).baseUnit;
        col.values << stateValue(c.state, c.value * unitScaleFactor(c.unit, col.unit));
        col.states << c.state;
      }
      if (!isLegacy)
        for (int k = 0; k < limitGroup.size(); ++k)
        {
          const quint32 bits = LimitTest::flags(LimitTest::fromText(line.section(';', 2 + 3 * groups + k, 2 + 3 * groups + k)));
          if (limitGroup[k] == 0)
            rec.limits << bits;
          else if (limitGroup[k] > 0)
            rec.extra[limitGroup[k] - 1].limits << bits;
        }
    }
    line = ts.readLine();
  }
  while (!line.isNull());

  // Sample time from the span and the row count. (Integer seconds divided by
  // the row count and then by ten, the old formula, came out as 0 -> 1 for
  // anything sampled slower than every 0.1 s.)
  const int count = rec.values.size();
  const qint64 spanMs = rec.start.msecsTo(end);
  rec.sampleTimeTenths = count > 1 ? qMax(1, qRound(spanMs / 100.0 / (count - 1))) : 1;
  rec.meta = meta;
  return rec;
}

namespace
{
// the name of a column of the other value @p col (group @p g): "value 2"
// ..., numbered by the value, so that a file with only value 3 says so;
// another meter's after its name, "Sim B: value", "Sim B: value 2"
QString columnName(const QString &word, const RecordingColumn &col, int g)
{
  if (col.device.isEmpty())
    return QString("%1 %2").arg(word).arg((col.id > 0 ? col.id : g + 1) + 1);
  QString device = col.device;
  device.replace(';', ',');   // the CSV's separator
  return col.id > 0 ? QString("%1: %2 %3").arg(device, word).arg(col.id + 1) : QString("%1: %2").arg(device, word);
}

// the name of a column of the main value: "value", "Sim A: value"
QString mainName(const QString &word, const Recording &rec)
{
  QString device = rec.device;
  device.replace(';', ',');   // the CSV's separator
  return device.isEmpty() ? word : QString("%1: %2").arg(device, word);
}

// the header of the columns of each value: value, unit, status, then value 2 ...
QStringList columnNames(const Recording &rec, const QStringList &names)
{
  const QList<RecordingColumn> &extra = rec.extra;
  QStringList out;
  for (const QString &name : names)
    out << mainName(name, rec);
  for (int g = 0; g < extra.size(); ++g)
    for (const QString &name : names)
      out << columnName(name, extra[g], g);
  return out;
}

bool hasVerdict(const QVector<quint32> &limits)
{
  for (quint32 bits : limits)
    if (bits)
      return true;
  return false;
}

// the values with a limit test: -1 the main value, else the column's index
QList<int> limitColumns(const Recording &rec)
{
  QList<int> out;
  if (hasVerdict(rec.limits))
    out << -1;
  for (int g = 0; g < rec.extra.size(); ++g)
    if (hasVerdict(rec.extra[g].limits))
      out << g;
  return out;
}

QString limitName(const Recording &rec, int column, const QString &word)
{
  return column < 0 ? mainName(word, rec) : columnName(word, rec.extra[column], column);
}

QString limitCell(const Recording &rec, int column, int row)
{
  const QVector<quint32> &limits = column < 0 ? rec.limits : rec.extra[column].limits;
  return LimitTest::verdictText(LimitTest::fromFlags(limits.value(row)));
}
}

bool RecordingFile::write(const Recording &recording, const QString &path, QString *error)
{
  if (recording.values.isEmpty())
  {
    setError(error, tr("Nothing to export."));
    return false;
  }
  QFile file(path);
  if (!file.open(QIODevice::WriteOnly | QIODevice::Text))
  {
    setError(error, tr("Cannot open file."));
    return false;
  }

  QTextStream ts(&file);
  const QList<int> limits = limitColumns(recording);
  ts << "timestamp;time (s);" << columnNames(recording, { "value", "unit", "status" }).join(';');
  for (int c : limits)
    ts << ';' << limitName(recording, c, QStringLiteral("limit"));
  ts << '\n';
  auto cell = [&](double v, CellState state, const QString &unit)
  {
    if (state == CellState::None)
      return QString(";;");
    QString prefix;
    const QString value = state == CellState::Ok ? SiPrefix::format(v, unit, &prefix) : QString("nan");
    return QString("%1;%2;%3").arg(value, prefix + unit, QLatin1String(stateText(state)));
  };
  for (int i = 0; i < recording.values.size(); ++i)
  {
    const qint64 ms = recording.timeAt(i);
    const QDateTime dt = recording.start.addMSecs(ms);
    ts << dt.toString("yyyy-MM-ddTHH:mm:ss,zzz") << ';' << QString::number(ms / 1000.0, 'g', 12) << ';'
       << cell(recording.values[i], recording.stateAt(i), recording.unit);
    for (const RecordingColumn &col : recording.extra)
      ts << ';' << cell(col.values.value(i, qQNaN()), col.states.value(i, CellState::None), col.unit);
    for (int c : limits)
      ts << ';' << limitCell(recording, c, i);
    ts << '\n';
  }
  return true;
}

static bool writeQtdmm(const Recording &recording, const QString &path, QString *error)
{
  // the CSV as it is, beside it what it does not keep
  QTemporaryDir tmp;
  const QString csvPath = tmp.filePath("recording.csv");
  if (!tmp.isValid() || !RecordingFile::write(recording, csvPath, error))
    return false;
  QFile csv(csvPath);
  if (!csv.open(QIODevice::ReadOnly))
  {
    setError(error, tr("Cannot open file."));
    return false;
  }
  QJsonObject meta = recording.meta;
  meta["format"] = 1;
  meta["qtdmm"] = QCoreApplication::applicationVersion();
  meta["written"] = QDateTime::currentDateTime().toString(Qt::ISODate);
  ZipWriter zip;
  QSaveFile out(path);
  if (!zip.add("meta.json", QJsonDocument(meta).toJson(), false) || !zip.add("recording.csv", csv.readAll(), false)
      || !out.open(QIODevice::WriteOnly) || out.write(zip.finish()) < 0 || !out.commit())
  {
    setError(error, tr("Cannot write the file."));
    return false;
  }
  return true;
}

bool RecordingFile::writeAny(const Recording &recording, const QString &path, QString *error)
{
  if (QFileInfo(path).suffix().compare(kSuffix, Qt::CaseInsensitive) == 0)
    return writeQtdmm(recording, path, error);
  const auto format = SpreadsheetWriter::formatForFile(path);
  if (!format)
    return write(recording, path, error);
  if (recording.values.isEmpty())
  {
    setError(error, tr("Nothing to export."));
    return false;
  }
  SpreadsheetWriter sheet(tr("Recording"));
  const QList<int> limits = limitColumns(recording);
  QStringList head = QStringList{ tr("timestamp"), tr("time (s)") } + columnNames(recording, { tr("value"), tr("unit"), tr("status") });
  for (int c : limits)
    head << limitName(recording, c, tr("limit"));
  sheet.setHeader(head);
  // a value that is no number is an empty cell: a spreadsheet has no NaN
  auto cells = [](QList<QVariant> &row, double v, CellState state, const QString &unit)
  {
    const bool there = state != CellState::None;
    row << (state == CellState::Ok ? QVariant(v) : QVariant()) << (there ? QVariant(unit) : QVariant())
        << (there ? QVariant(QString::fromLatin1(stateText(state))) : QVariant());
  };
  for (int i = 0; i < recording.values.size(); ++i)
  {
    const qint64 ms = recording.timeAt(i);
    QList<QVariant> row{ recording.start.addMSecs(ms), ms / 1000.0 };
    cells(row, recording.values[i], recording.stateAt(i), recording.unit);
    for (const RecordingColumn &col : recording.extra)
      cells(row, col.values.value(i, qQNaN()), col.states.value(i, CellState::None), col.unit);
    for (int c : limits)
      row << limitCell(recording, c, i);
    sheet.addRow(row);
  }
  return sheet.write(path, *format, error);
}
