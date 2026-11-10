// Copyright (c) 2026 The QtDMM developers
// SPDX-License-Identifier: GPL-3.0-or-later
#include "recording/readingsmodel.h"
#include "core/reading.h"
#include "recording/recordingstore.h"

#include <QCoreApplication>
#include <QFile>
#include <QRegularExpression>
#include <QTextStream>

#include <cmath>

#include "core/limittest.h"
#include "core/siprefix.h"
#include "recording/spreadsheet.h"

ReadingsModel::ReadingsModel(QObject *parent) :
  QAbstractTableModel(parent), m_own(new RecordingStore(this)), m_store(m_own)
{
  connectStore();
}

// The store's signals come in pairs around each change, as the model wants them.
void ReadingsModel::connectStore()
{
  connect(m_store, &RecordingStore::readingsAboutToInsert, this, [this](int first, int last)
  {
    m_inserting = { first, last };
    beginInsertRows(QModelIndex(), first, last);
  });
  connect(m_store, &RecordingStore::readingsInserted, this, [this]
  {
    endInsertRows();
    updateSingleDay();
    noteValues(m_inserting.first, m_inserting.second);
  });
  connect(m_store, &RecordingStore::readingsAboutToRemove, this,
          [this](int first, int last) { beginRemoveRows(QModelIndex(), first, last); });
  connect(m_store, &RecordingStore::readingsRemoved, this, [this] { endRemoveRows(); updateSingleDay(); });
  connect(m_store, &RecordingStore::readingsAboutToClear, this, [this] { beginResetModel(); });
  connect(m_store, &RecordingStore::readingsCleared, this, [this]
  {
    m_singleDay = true;
    endResetModel();
  });
  connect(m_store, &RecordingStore::readingMarked, this, [this](int row)
  {
    Q_EMIT dataChanged(index(row, 0), index(row, ColumnCount - 1), {Qt::BackgroundRole, Qt::ToolTipRole});
  });
  // a store from outside may go first (the main window's views are deleted
  // one after the other): the table falls back to its own, empty one
  if (m_store != m_own)
    connect(m_store, &QObject::destroyed, this, [this]
    {
      beginResetModel();
      m_store = m_own;
      m_own->clearReadings();   // not connected yet: rows from before setStore()
      connectStore();
      m_singleDay = allOneDay();
      endResetModel();
    });
}

void ReadingsModel::setStore(RecordingStore *store)
{
  if (!store)
  {
    store = m_own;
    m_own->clearReadings();
  }
  if (store == m_store)
    return;
  beginResetModel();
  m_store->disconnect(this);
  m_store = store;
  connectStore();
  m_singleDay = allOneDay();
  endResetModel();
  noteValues(0, m_store->readingCount() - 1);
}

bool ReadingsModel::isPaused() const
{
  return m_store->readingsPaused();
}

void ReadingsModel::setPaused(bool paused)
{
  m_store->setReadingsPaused(paused);
}

int ReadingsModel::maxRows() const
{
  return m_store->readingCapacity();
}

ReadingsModel::Entry ReadingsModel::entry(int row) const
{
  const LoggedReading &r = m_store->readingAt(row);
  Entry e;
  e.when = QDateTime::fromMSecsSinceEpoch(r.when);
  e.dval = r.value;
  e.val = r.text;
  e.unit = r.unit;
  e.port = r.port;
  e.flags = r.flags;
  e.range = r.range;
  e.hold = r.hold();
  e.id = r.id;
  if (r.alarmArgb)
    e.alarmColor = QColor::fromRgba(r.alarmArgb);
  e.alarmName = r.alarmName;
  return e;
}

int ReadingsModel::rowCount(const QModelIndex &parent) const
{
  return parent.isValid() ? 0 : m_store->readingCount();
}

int ReadingsModel::columnCount(const QModelIndex &parent) const
{
  return parent.isValid() ? 0 : ColumnCount;
}

QString ReadingsModel::formatTime(const QDateTime &when)
{
  return when.toString("yyyy-MM-dd HH:mm:ss.zzz");
}

QString ReadingsModel::modeColumn(int id, const PortKey &port, quint32 flags)
{
  // the other values of the meter by their place: 2nd, 3rd, 4th
  static const char *const ordinals[] = { "2nd", "3rd", "4th" };
  const QString mode = modeText(port, flags);
  if (id <= 0)
    return mode;
  return (QString::fromLatin1(ordinals[qMin(id, kMaxValues - 1) - 1]) + ' ' + mode).trimmed();
}

void ReadingsModel::setValueHidden(const ValueRef &value, bool hidden)
{
  if (hidden == m_hidden.contains(value))
    return;
  if (hidden)
    m_hidden.insert(value);
  else
    m_hidden.remove(value);
  Q_EMIT hiddenValuesChanged();
}

void ReadingsModel::setDevices(bool on, const QString &own, std::function<QString(const QString &)> name)
{
  m_ownSource = own;
  m_deviceName = std::move(name);
  if (on == m_devices)
    return;
  m_devices = on;
  Q_EMIT devicesChanged(on);
}

ValueRef ReadingsModel::valueOf(int row) const
{
  const LoggedReading &e = m_store->readingAt(row);
  return ValueRef(e.id, e.source == m_ownSource ? QString() : e.source);
}

void ReadingsModel::setValueNames(const QStringList &names)
{
  if (names == m_names)
    return;
  m_names = names;
  Q_EMIT valueNamesChanged();
}

void ReadingsModel::noteValues(int first, int last)
{
  for (int r = qMax(0, first); r <= last && r < m_store->readingCount() && !(m_otherValues && m_verdicts); ++r)
  {
    const LoggedReading &e = m_store->readingAt(r);
    if (!m_otherValues && e.id > 0)
    {
      m_otherValues = true;
      Q_EMIT otherValuesChanged(true);
    }
    if (!m_verdicts && (e.flags & SampleFlag::Limit))
    {
      m_verdicts = true;
      Q_EMIT verdictsChanged(true);
    }
  }
}

QString ReadingsModel::modeText(const PortKey &port, quint32 flags)
{
  if (flags & SampleFlag::Diode)
    return tr("Diode");
  const QString coupling = couplingText(flags);
  if (!coupling.isEmpty())
    return coupling == QLatin1String("AC+DC") ? tr("AC+DC") : coupling;
  switch (port.quantity)
  {
    case Quantity::Unknown:
    case Quantity::Voltage:
    case Quantity::Current:     return QString();
    case Quantity::Continuity:  return tr("Continuity");
    case Quantity::Resistance:  return tr("Resistance");
    case Quantity::Capacitance: return tr("Capacitance");
    case Quantity::Frequency:   return tr("Frequency");
    case Quantity::Temperature: return tr("Temperature");
    default:                    return Quantities::name(port.quantity);
  }
}

QVariant ReadingsModel::data(const QModelIndex &index, int role) const
{
  if (!index.isValid() || index.row() >= m_store->readingCount())
    return QVariant();
  const LoggedReading &e = m_store->readingAt(index.row());

  if (role == DvalRole)
    return e.value;
  if (role == Qt::BackgroundRole && e.alarmArgb)
  {
    QColor c = QColor::fromRgba(e.alarmArgb);
    c.setAlpha(70);
    return c;
  }
  if (role == Qt::ToolTipRole && !e.alarmName.isEmpty())
    return e.alarmName;   // the row is marked in the alarm's colour; "Alarm 1", not "Alarm Alarm 1"
  // the verdict in its colour, as on the display
  if (role == Qt::ForegroundRole && index.column() == Limit && (e.flags & SampleFlag::Limit))
    return e.flags & SampleFlag::LimitPass ? QColor(0x1f, 0x8f, 0x3a) : QColor(0xc6, 0x28, 0x28);
  if (role == Qt::TextAlignmentRole)
    return int(index.column() == Value || index.column() == Number ? Qt::AlignRight | Qt::AlignVCenter
                                                                   : Qt::AlignLeft | Qt::AlignVCenter);
  if (role != Qt::DisplayRole)
    return QVariant();

  switch (index.column())
  {
    case Time:
    {
      const QDateTime when = QDateTime::fromMSecsSinceEpoch(e.when);
      return m_singleDay ? when.toString("HH:mm:ss.zzz") : formatTime(when);
    }
    case Device: return m_deviceName ? m_deviceName(e.source) : e.source;
    case Number: return e.id + 1;
    case Value: return SiPrefix::withoutLeadingZeros(e.text);   // "000.00" -> "0.00", as the meter shows it
    case Unit:  return SiPrefix::displayText(e.unit);   // "°C", "kΩ" as on the display
    case Mode:  return modeText(e.port, e.flags);
    case Range: return e.range;
    case Hold:  return e.hold() ? tr("HOLD") : QString();
    case Limit: return LimitTest::verdictText(LimitTest::fromFlags(e.flags));
  }
  return QVariant();
}

QVariant ReadingsModel::headerData(int section, Qt::Orientation orientation, int role) const
{
  if (role != Qt::DisplayRole)
    return QVariant();
  if (orientation == Qt::Vertical)
    return section + 1;
  switch (section)
  {
    case Time:  return tr("Time");
    case Device: return tr("Device");
    case Number: return tr("No.");
    case Value: return tr("Value");
    case Unit:  return tr("Unit");
    case Mode:  return tr("Mode");
    case Range: return tr("Range");
    case Hold:  return tr("Hold");
    case Limit: return tr("Limit");
  }
  return QVariant();
}

void ReadingsModel::append(const Entry &entry)
{
  static const QRegularExpression letters("[A-Za-z]");
  Reading r;
  r.value = entry.dval;
  r.text = entry.val;
  r.unit = entry.unit;
  r.port = entry.port;
  r.flags = entry.flags | (entry.hold ? SampleFlag::Hold : 0u);
  r.range = entry.range;
  r.hold = entry.hold;
  r.id = entry.id;
  r.overload = entry.val.contains(letters);
  r.msecs = entry.when.toMSecsSinceEpoch();
  m_store->logReading(r);
}

bool ReadingsModel::allOneDay() const
{
  // the rows are in time order: first and last tell whether a day changed
  const int count = m_store->readingCount();
  return count == 0 || entry(0).when.date() == entry(count - 1).when.date();
}

void ReadingsModel::updateSingleDay()
{
  const bool single = allOneDay();
  if (single == m_singleDay)
    return;
  m_singleDay = single;
  const int count = m_store->readingCount();
  if (count)
    Q_EMIT dataChanged(index(0, Time), index(count - 1, Time), {Qt::DisplayRole});
}

void ReadingsModel::markLast(const QColor &color, const QString &name)
{
  m_store->markLastReading(color.rgba(), name);
}

void ReadingsModel::markAt(const QDateTime &when, const QColor &color, const QString &name)
{
  m_store->markReadingAt(when.toMSecsSinceEpoch(), color.rgba(), name);
}

void ReadingsModel::moveMark(const QDateTime &from, const QDateTime &to, const QColor &color, const QString &name)
{
  m_store->unmarkReadingAt(from.toMSecsSinceEpoch(), name);
  m_store->markReadingAt(to.toMSecsSinceEpoch(), color.rgba(), name);
}

void ReadingsModel::clear()
{
  m_store->clearReadings();
}

void ReadingsModel::setMaxRows(int rows)
{
  m_store->setReadingCapacity(rows);
}

ReadingsModel::Stats ReadingsModel::stats(int id) const
{
  static const QRegularExpression letters("[A-Za-z]");
  Stats s;
  ValueRef target(id);
  if (id < 0)
  {
    // the window meter's first value shown; a table without its rows: the
    // first value another meter's rows show
    target = ValueRef(0);
    while (m_hidden.contains(target) && target.id < kMaxValues - 1)
      target.id++;
    bool any = false;
    for (int i = 0; i < m_store->readingCount() && !any; ++i)
      any = valueOf(i) == target;
    for (int i = 0; i < m_store->readingCount() && !any; ++i)
      if (!m_hidden.contains(valueOf(i)))
      {
        target = valueOf(i);
        any = true;
      }
  }
  s.id = target.id;
  s.source = target.source;
  for (int i = 0; i < m_store->readingCount(); ++i)
    if (valueOf(i) == target)
      ++s.count;
  // the function of the value's newest reading: the numbers are over its
  // readings only - a table that holds volts and ohms has no minimum of both
  int newest = m_store->readingCount() - 1;
  while (newest >= 0 && valueOf(newest) != target)
    --newest;
  if (newest < 0)
    return s;
  const PortKey port = m_store->readingAt(newest).port;
  s.unit = SiPrefix::split(m_store->readingAt(newest).unit).baseUnit;
  double sum = 0;
  for (int i = 0; i <= newest; ++i)
  {
    const LoggedReading &e = m_store->readingAt(i);
    if (valueOf(i) != target || e.text.contains(letters) || !std::isfinite(e.value))   // other values, OL
      continue;
    if (e.port != port || SiPrefix::split(e.unit).baseUnit != s.unit)
      continue;
    if (s.numeric == 0)
      s.min = s.max = e.value;
    s.min = qMin(s.min, e.value);
    s.max = qMax(s.max, e.value);
    sum += e.value;
    ++s.numeric;
  }
  if (s.numeric)
    s.mean = sum / s.numeric;
  return s;
}

QString ReadingsModel::toText(const QList<int> &rows) const
{
  QStringList lines;
  QStringList header;
  // the number only when the table shows it
  auto copied = [this](int c)
  {
    return (c != Number || m_otherValues) && (c != Limit || m_verdicts) && (c != Device || m_devices);
  };
  for (int c = 0; c < ColumnCount; ++c)
    if (copied(c))
      header << headerData(c, Qt::Horizontal, Qt::DisplayRole).toString();
  lines << header.join('\t');

  QList<int> which = rows;
  if (which.isEmpty())
    for (int r = 0; r < m_store->readingCount(); ++r)
      if (rowShown(r))
        which << r;
  std::sort(which.begin(), which.end());
  for (int r : which)
  {
    if (r < 0 || r >= m_store->readingCount())
      continue;
    QStringList cells;
    // the date stays in the copy: pasted elsewhere the rows lose their context
    cells << formatTime(QDateTime::fromMSecsSinceEpoch(m_store->readingAt(r).when));
    for (int c = Time + 1; c < ColumnCount; ++c)
      if (copied(c))
        cells << data(index(r, c), Qt::DisplayRole).toString();
    lines << cells.join('\t');
  }
  return lines.join('\n') + '\n';
}

bool ReadingsModel::write(const QString &path, QString *error) const
{
  auto fail = [&](const QString &text)
  {
    if (error)
      *error = text;
    return false;
  };
  if (m_store->readingCount() == 0)
    return fail(tr("Nothing to export."));
  QFile file(path);
  if (!file.open(QIODevice::WriteOnly | QIODevice::Text))
    return fail(tr("Cannot open file."));
  QTextStream ts(&file);
  ts << (m_devices ? "device;" : "") << "timestamp;value;unit;mode;range;hold" << (m_verdicts ? ";limit\n" : "\n");
  for (int i = 0; i < m_store->readingCount(); ++i)
  {
    const Entry e = entry(i);
    if (m_devices)
      ts << data(index(i, Device)).toString() << ';';
    ts << QString("%1;%2;%3;%4;%5;%6")
            .arg(e.when.toString("yyyy-MM-ddTHH:mm:ss,zzz"), SiPrefix::withoutLeadingZeros(e.val), e.unit,
                 modeColumn(e.id, e.port, e.flags), e.range, e.hold ? "1" : "0");
    if (m_verdicts)
      ts << ';' << LimitTest::verdictText(LimitTest::fromFlags(e.flags));
    ts << '\n';
  }
  return true;
}

bool ReadingsModel::writeAny(const QString &path, QString *error) const
{
  const auto format = SpreadsheetWriter::formatForFile(path);
  if (!format)
    return write(path, error);
  if (m_store->readingCount() == 0)
  {
    if (error)
      *error = tr("Nothing to export.");
    return false;
  }
  SpreadsheetWriter sheet(tr("Readings"));
  QStringList head{tr("Time"), tr("Value"), tr("Unit"), tr("Mode"), tr("Range"), tr("Hold"), tr("Alarm")};
  if (m_devices)
    head.prepend(tr("Device"));
  if (m_verdicts)
    head << tr("Limit");
  sheet.setHeader(head);
  for (int i = 0; i < m_store->readingCount(); ++i)
  {
    const Entry e = entry(i);
    const QString val = SiPrefix::withoutLeadingZeros(e.val);
    bool numeric = false;
    const double number = val.toDouble(&numeric);
    QList<QVariant> row{e.when, numeric ? QVariant(number) : QVariant(val), e.unit,
                        modeColumn(e.id, e.port, e.flags), e.range, e.hold ? tr("HOLD") : QString(), e.alarmName};
    if (m_devices)
      row.prepend(data(index(i, Device)));
    if (m_verdicts)
      row << LimitTest::verdictText(LimitTest::fromFlags(e.flags));
    sheet.addRow(row);
  }
  return sheet.write(path, *format, error);
}

void ReadingsModel::appendReading(const Reading &r)
{
  m_store->logReading(r);
}
