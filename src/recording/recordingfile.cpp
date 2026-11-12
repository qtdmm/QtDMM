// Copyright (c) 2026 The QtDMM developers
// SPDX-License-Identifier: GPL-3.0-or-later
#include "recording/recordingfile.h"

#include <QBuffer>
#include <QCoreApplication>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QRegularExpression>
#include <QSaveFile>
#include <QTemporaryDir>
#include <QTextStream>

#include <cmath>

#include "core/limittest.h"
#include "core/siprefix.h"
#include "recording/labplotproject.h"
#include "recording/spreadsheet.h"
#include "recording/zipfile.h"

namespace
{
// Recordings keep at most two million points; these allowances cover verbose
// CSV rows and metadata while bounding archive input and decompression.
constexpr qint64 kMaxRecordingArchiveBytes = qint64(256) << 20;
constexpr qint64 kMaxRecordingCsvBytes = qint64(512) << 20;
constexpr qint64 kMaxRecordingMetaBytes = qint64(16) << 20;

QString tr(const char *text)
{
  return QCoreApplication::translate("RecordingFile", text);
}

// the fields of a CSV line, quotes as RFC 4180
QStringList csvFields(const QString &line)
{
  QStringList out;
  QString field;
  bool quoted = false;
  for (int i = 0; i < line.size(); ++i)
  {
    const QChar c = line[i];
    if (quoted)
    {
      if (c == '"' && i + 1 < line.size() && line[i + 1] == '"')
        field += line[++i];
      else if (c == '"')
        quoted = false;
      else
        field += c;
    }
    else if (c == '"')
      quoted = true;
    else if (c == ';')
    {
      out << field;
      field.clear();
    }
    else
      field += c;
  }
  out << field;
  return out;
}

// several marks on one row of a CSV
const char *const kMarkSeparator = " | ";

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

// the meter's states of a reading as words, as the graph's strip names them
const std::pair<quint32, const char *> kStateWords[] = { { SampleFlag::Hold, "HOLD" }, { SampleFlag::Min, "MIN" },
                                                          { SampleFlag::Max, "MAX" }, { SampleFlag::Peak, "PEAK" },
                                                          { SampleFlag::Avg, "AVG" }, { SampleFlag::Relative, "REL" },
                                                          { SampleFlag::GapLost, "LOST" }, { SampleFlag::GapPause, "PAUSE" } };

QString stateFlagsText(quint32 flags)
{
  QStringList words;
  for (const auto &[bit, word] : kStateWords)
    if (flags & bit)
      words << QLatin1String(word);
  return words.join(' ');
}

quint32 stateFlagsFrom(const QString &text)
{
  quint32 flags = 0;
  for (const QString &w : text.split(' ', Qt::SkipEmptyParts))
    for (const auto &[bit, word] : kStateWords)
      if (w.compare(QLatin1String(word), Qt::CaseInsensitive) == 0)
        flags |= bit;
  return flags;
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
    if (file.size() < 0 || file.size() > kMaxRecordingArchiveBytes)
    {
      setError(error, tr("The recording archive is too large to import."));
      return std::nullopt;
    }
    const QByteArray zip = file.read(kMaxRecordingArchiveBytes + 1);
    if (zip.size() > kMaxRecordingArchiveBytes || !file.atEnd())
    {
      setError(error, tr("The recording archive is too large to import."));
      return std::nullopt;
    }
    const std::optional<QByteArray> csv = zipEntry(zip, QStringLiteral("recording.csv"), kMaxRecordingCsvBytes);
    if (!csv)
    {
      setError(error, tr("No usable recording data found (recording.csv is missing, invalid, or too large)."));
      return std::nullopt;
    }
    if (const std::optional<QByteArray> json = zipEntry(zip, QStringLiteral("meta.json"), kMaxRecordingMetaBytes))
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
  QStringList limitNames;   // the limit and flags columns at the end: "limit", "limit 2", "flags" ...
  bool withMarks = false;   // mark;note at the very end (since 27.1)
  if (line.startsWith("timestamp"))   // the CSV header
  {
    headerNames = line.split(';');
    if (headerNames.size() > 4 && headerNames.last() == QLatin1String("note")
        && headerNames.value(headerNames.size() - 2) == QLatin1String("mark"))
    {
      withMarks = true;
      headerNames.resize(headerNames.size() - 2);
    }
    while (headerNames.size() > 5 && (columnName(headerNames.last()).word == QLatin1String("limit")
                                      || columnName(headerNames.last()).word == QLatin1String("flags")))
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
  QList<bool> limitIsFlags;   // the meter's states ("flags", since 27.1) instead of a verdict
  for (const QString &text : std::as_const(limitNames))
  {
    const ColumnName name = columnName(text);
    limitIsFlags << (name.word == QLatin1String("flags"));
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
  QJsonArray marks;   // from the mark;note columns
  auto when = [](const QRegularExpressionMatch &m)
  {
    return QDateTime(QDate(m.captured("year").toInt(), m.captured("month").toInt(), m.captured("day").toInt()),
                     QTime(m.captured("hour").toInt(), m.captured("minute").toInt(), m.captured("second").toInt(),
                           m.captured("ms").toInt()));
  };
  do
  {
    // a quoted note may hold line breaks: the line goes on until its quotes close
    while (withMarks && line.count('"') % 2 == 1)
    {
      const QString more = ts.readLine();
      if (more.isNull())
        break;
      line += '\n' + more;
    }
    if (!line.trimmed().isEmpty())
    {
      QDateTime at;
      QList<Cell> cells;
      QStringList f;
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
        f = withMarks ? csvFields(line) : line.split(';');
        const QRegularExpressionMatch match = reTime.match(f.value(0));
        valid = match.hasMatch() && reDelta.match(f.value(1)).hasMatch()
                && (withStatus ? f.size() == 2 + 3 * groups + limitNames.size() + (withMarks ? 2 : 0) : f.size() >= 4);
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
      {
        // a value's verdict and its states are one entry of the row
        QMap<int, quint32> rowBits;
        for (int k = 0; k < limitGroup.size(); ++k)
          if (limitGroup[k] >= 0)
            rowBits[limitGroup[k]] |= limitIsFlags[k] ? stateFlagsFrom(f.value(2 + 3 * groups + k))
                                                      : LimitTest::flags(LimitTest::fromText(f.value(2 + 3 * groups + k)));
        for (auto it = rowBits.cbegin(); it != rowBits.cend(); ++it)
          (it.key() == 0 ? rec.limits : rec.extra[it.key() - 1].limits) << it.value();
      }
      if (withMarks && !f.value(f.size() - 2).isEmpty())
      {
        // back as notes at the row's time; several on one row were joined
        const QStringList titles = f.value(f.size() - 2).split(QLatin1String(kMarkSeparator));
        const QStringList texts = f.value(f.size() - 1).split(QLatin1String(kMarkSeparator));
        for (int k = 0; k < titles.size(); ++k)
          marks.append(QJsonObject{ { "t", double(rec.start.msecsTo(at)) },
                                    { "color", QString("#%1").arg(RecordingFile::kNoteColor, 8, 16, QChar('0')) },
                                    { "name", titles[k] },
                                    { "note", titles.size() == texts.size() ? texts[k] : (k == 0 ? f.last() : QString()) },
                                    { "kind", "note" } });
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
  if (!marks.isEmpty() && !rec.meta.contains("marks"))
    rec.meta["marks"] = marks;
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

// a CSV field: quoted where it holds the separator, a quote or a line break
QString csvField(const QString &text)
{
  static const QRegularExpression special(QStringLiteral("[;\"\r\n]"));
  if (!text.contains(special))
    return text;
  return '"' + QString(text).replace('"', QLatin1String("\"\"")) + '"';
}

const QString kMarkJoin = QString::fromLatin1(kMarkSeparator);

// the marks of meta.json by the row nearest to their time: title and text
// per row, several joined; empty when the recording has none
QList<QPair<QString, QString>> rowMarks(const Recording &rec)
{
  const QJsonArray marks = rec.meta.value("marks").toArray();
  QList<QPair<QString, QString>> out;
  if (marks.isEmpty() || rec.values.isEmpty())
    return out;
  out.resize(rec.values.size());
  for (const QJsonValue &v : marks)
  {
    const QJsonObject m = v.toObject();
    const qint64 t = qint64(m.value("t").toDouble());
    // the rows come in the order of their time
    int lo = 0;
    int hi = int(rec.values.size()) - 1;
    while (lo < hi)
    {
      const int mid = (lo + hi) / 2;
      if (rec.timeAt(mid) < t)
        lo = mid + 1;
      else
        hi = mid;
    }
    if (lo > 0 && t - rec.timeAt(lo - 1) <= rec.timeAt(lo) - t)
      --lo;
    QPair<QString, QString> &cell = out[lo];
    const bool first = cell.first.isEmpty() && cell.second.isEmpty();
    cell.first += (first ? QString() : kMarkJoin) + m.value("name").toString();
    cell.second += (first ? QString() : kMarkJoin) + m.value("note").toString();
  }
  return out;
}

bool hasVerdict(const QVector<quint32> &limits, quint32 mask = SampleFlag::Limit)
{
  for (quint32 bits : limits)
    if (bits & mask)
      return true;
  return false;
}

// the values the meter reported a state of (HOLD, REL ...): -1 the main value, else the column's index
QList<int> flagColumns(const Recording &rec)
{
  QList<int> out;
  if (hasVerdict(rec.limits, SampleFlag::State))
    out << -1;
  for (int g = 0; g < rec.extra.size(); ++g)
    if (hasVerdict(rec.extra[g].limits, SampleFlag::State))
      out << g;
  return out;
}

QString flagCell(const Recording &rec, int column, int row)
{
  const QVector<quint32> &limits = column < 0 ? rec.limits : rec.extra[column].limits;
  return stateFlagsText(limits.value(row));
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

static bool writeCsv(const Recording &recording, const QString &path, QString *error, bool withMarks)
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
  const QList<int> flags = flagColumns(recording);
  ts << "timestamp;time (s);" << columnNames(recording, { "value", "unit", "status" }).join(';');
  for (int c : limits)
    ts << ';' << limitName(recording, c, QStringLiteral("limit"));
  for (int c : flags)
    ts << ';' << limitName(recording, c, QStringLiteral("flags"));
  const QList<QPair<QString, QString>> marks = withMarks ? rowMarks(recording) : QList<QPair<QString, QString>>();
  if (!marks.isEmpty())
    ts << ";mark;note";
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
    for (int c : flags)
      ts << ';' << flagCell(recording, c, i);
    if (!marks.isEmpty())
      ts << ';' << csvField(marks[i].first) << ';' << csvField(marks[i].second);
    ts << '\n';
  }
  return true;
}

bool RecordingFile::write(const Recording &recording, const QString &path, QString *error)
{
  return writeCsv(recording, path, error, true);
}

static bool writeQtdmm(const Recording &recording, const QString &path, QString *error)
{
  // the CSV as it is, beside it what it does not keep
  QTemporaryDir tmp;
  const QString csvPath = tmp.filePath("recording.csv");
  // the marks are in meta.json, to the ms and with their kind
  if (!tmp.isValid() || !writeCsv(recording, csvPath, error, false))
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
  // what the CSV says too, at a glance
  meta["start"] = recording.start.toString(Qt::ISODateWithMs);
  if (!recording.values.isEmpty())
    meta["end"] = recording.start.addMSecs(recording.timeAt(int(recording.values.size()) - 1)).toString(Qt::ISODateWithMs);
  meta["sample-time"] = recording.sampleTimeTenths / 10.0;
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

QList<RecordingFile::NumberColumn> RecordingFile::numberColumns(const Recording &recording, QVector<double> *seconds)
{
  auto numbers = [&recording](auto value, auto state)
  {
    QVector<double> out(recording.values.size());
    for (int i = 0; i < out.size(); ++i)
      out[i] = state(i) == CellState::Ok ? value(i) : qQNaN();
    return out;
  };
  QList<NumberColumn> out{ { mainName(QStringLiteral("value"), recording), recording.unit,
                             numbers([&](int i) { return recording.values[i]; },
                                     [&](int i) { return recording.stateAt(i); }) } };
  for (int g = 0; g < recording.extra.size(); ++g)
  {
    const RecordingColumn &col = recording.extra[g];
    out << NumberColumn{ columnName(QStringLiteral("value"), col, g), col.unit,
                         numbers([&](int i) { return col.values.value(i, qQNaN()); },
                                 [&](int i) { return col.states.value(i, CellState::None); }) };
  }
  if (seconds)
  {
    seconds->resize(recording.values.size());
    for (int i = 0; i < seconds->size(); ++i)
      (*seconds)[i] = recording.timeAt(i) / 1000.0;
  }
  return out;
}

// numbers only: time, then each value in its unit; what is no number is empty
static bool writeLabPlot(const Recording &recording, const QString &path, QString *error)
{
  if (recording.values.isEmpty())
  {
    setError(error, tr("Nothing to export."));
    return false;
  }
  QSaveFile file(path);
  if (!file.open(QIODevice::WriteOnly | QIODevice::Text))
  {
    setError(error, tr("Cannot open file."));
    return false;
  }
  QVector<double> seconds;
  const QList<RecordingFile::NumberColumn> columns = RecordingFile::numberColumns(recording, &seconds);
  // "time[s]" without a space: LabPlot's automatic import guesses the
  // separator from the first header field, "time (s)" fails it (2.12)
  QStringList head{ QStringLiteral("time[s]") };
  for (const RecordingFile::NumberColumn &col : columns)
    head << (col.unit.isEmpty() ? col.name : QString("%1 [%2]").arg(col.name, col.unit));
  QTextStream ts(&file);
  ts << head.join(';') << '\n';
  for (int i = 0; i < seconds.size(); ++i)
  {
    ts << QString::number(seconds[i], 'g', 12);
    for (const RecordingFile::NumberColumn &col : columns)
      ts << ';' << (std::isnan(col.values[i]) ? QString() : QString::number(col.values[i], 'g', 12));
    ts << '\n';
  }
  ts.flush();
  if (!file.commit())
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
  if (path.endsWith(QString(".") + kLabPlotSuffix, Qt::CaseInsensitive))
    return writeLabPlot(recording, path, error);
  if (QFileInfo(path).suffix().compare(kLabPlotProjectSuffix, Qt::CaseInsensitive) == 0)
    return LabPlotProject::write(recording, path, error);
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
  const QList<int> flags = flagColumns(recording);
  for (int c : flags)
    head << limitName(recording, c, tr("flags"));
  const QList<QPair<QString, QString>> marks = rowMarks(recording);
  if (!marks.isEmpty())
    head << tr("mark") << tr("note");
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
    for (int c : flags)
      row << flagCell(recording, c, i);
    if (!marks.isEmpty())
      row << marks[i].first << marks[i].second;
    sheet.addRow(row);
  }
  return sheet.write(path, *format, error);
}
