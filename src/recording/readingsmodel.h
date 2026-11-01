// Copyright (c) 2026 The QtDMM developers
// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <functional>

#include <QAbstractTableModel>
#include <QColor>
#include <QDateTime>
#include <QSet>
#include <QStringList>
#include <QVector>

#include "core/valueref.h"
#include "core/sampletypes.h"
#include "core/reading.h"   // complete for moc: a slot takes it (GCC 16 -Wsfinae-incomplete)

/// Every reading the meter sent, one row each, as a table model: the raw
/// protocol of a session next to the recorder's time-gridded graph. A thin
/// model over the readings series of a RecordingStore (the newest maxRows()
/// readings, a ring): it holds no rows of its own, knows min/max/mean of
/// what the store holds and writes itself as CSV. Without setStore() it uses
/// a store of its own. No widgets here, so it can be tested; ReadingsWidget
/// shows it.
class RecordingStore;

class ReadingsModel : public QAbstractTableModel
{
  Q_OBJECT
public:
  /// One reading as MeterConnection::value() delivered it.
  struct Entry
  {
    QDateTime when;
    double dval = 0;      ///< SI base units, for statistics and sorting
    QString val;          ///< as displayed ("1.234", "OL")
    QString unit;         ///< with prefix ("mV")
    PortKey port;         ///< what was measured: voltage.dc, resistance, ...
    quint32 flags = 0;    ///< SampleFlag: AC, DC, Diode, Hold, ...
    QString range;        ///< "AUTO", "MANU" or empty
    bool hold = false;
    int id = 0;           ///< 0 = main display, 1+ = secondary values
    QColor alarmColor;    ///< set when an alarm raised on this reading
    QString alarmName;
  };

  /// Number: which value of the meter (1 the main one), only shown for a
  /// meter with more than one
  enum Column { Time, Device, Number, Value, Unit, Mode, Range, Hold, Limit, ColumnCount };
  /// Qt::UserRole on any cell gives the row's dval as double.
  static constexpr int DvalRole = Qt::UserRole;

  /// Statistics over the numeric (non-overload) readings of one value in the log.
  struct Stats
  {
    int id = 0;           ///< the value they are of
    QString source;       ///< of another meter than the window's; empty: the window's
    int count = 0;        ///< its rows in the log, overloads included
    int numeric = 0;      ///< rows min/max/mean are computed from: its readings of the newest one's
                          ///< function (port and unit), without overloads
    double min = 0, max = 0, mean = 0;
    QString unit;         ///< unit of the newest main reading, prefix stripped
  };

  explicit ReadingsModel(QObject *parent = nullptr);

  /// Shows the readings of @p store (the MeterController's recorder). The
  /// row limit and the pause are the store's: set maxRows() after this. Not
  /// owned; should it be deleted first, the log shows its own empty store again.
  /// nullptr: its own store, emptied (the readings of several meters, which
  /// the caller logs into it).
  void setStore(RecordingStore *store);
  /// The Device column: the rows' meters (LoggedReading::source) by @p name,
  /// shown while @p on (readings of more than one meter). @p own is a meter
  /// whose values go by an empty source (ValueRef); none since 26.4.
  void setDevices(bool on, const QString &own, std::function<QString(const QString &source)> name);
  bool hasDevices() const { return m_devices; }
  /// The value of row @p row; @p own's (setDevices()) by an empty source.
  ValueRef valueOf(int row) const;
  /// The table shows row @p row: its value is not hidden.
  bool rowShown(int row) const { return !m_hidden.contains(valueOf(row)); }
  RecordingStore *store() const { return m_store; }

  int rowCount(const QModelIndex &parent = QModelIndex()) const override;
  int columnCount(const QModelIndex &parent = QModelIndex()) const override;
  QVariant data(const QModelIndex &index, int role = Qt::DisplayRole) const override;
  QVariant headerData(int section, Qt::Orientation orientation, int role = Qt::DisplayRole) const override;

  /// Appends a row; the oldest one goes when the log is full. Dropped
  /// while paused.
  void append(const Entry &entry);
  /// Paused: readings pass by without being logged (the pause button).
  bool isPaused() const;
  void setPaused(bool paused);
  void clear();
  /// An alarm raised on the newest reading: the row gets the colour.
  void markLast(const QColor &color, const QString &name);

public Q_SLOTS:
  /// A reading from the MeterController.
  void appendReading(const Reading &reading);

public:
  Entry entry(int row) const;

  int maxRows() const;
  /// Rows to keep, at least 1; trims the log when it shrinks.
  void setMaxRows(int rows);

  /// Over value @p id; the first one shown in the table when -1.
  Stats stats(int id = -1) const;
  /// The name of the meter of @p source, as the column Device shows it.
  QString deviceName(const QString &source) const { return m_deviceName ? m_deviceName(source) : source; }

  /// The values the table leaves out (by Reading::id); copying leaves them
  /// out too, the export and the store keep them.
  QSet<ValueRef> hiddenValues() const { return m_hidden; }
  void setValueHidden(const ValueRef &value, bool hidden);
  /// Value @p id with an empty source.
  void setValueHidden(int id, bool hidden) { setValueHidden(ValueRef(id), hidden); }
  /// Whether a value other than the main one has come: the Number column.
  bool hasOtherValues() const { return m_otherValues; }
  /// Whether a reading with a limit test's verdict has come: the Limit column.
  bool hasVerdicts() const { return m_verdicts; }
  /// The names of the values, by id, for the statistics line.
  QStringList valueNames() const { return m_names; }
  void setValueNames(const QStringList &names);

Q_SIGNALS:
  void hiddenValuesChanged();
  /// The Device column came or went (setDevices()).
  void devicesChanged(bool on);
  void otherValuesChanged(bool);
  void verdictsChanged(bool);
  void valueNamesChanged();

public:
  /// Text of the rows @p rows (all shown ones when empty) - tab separated
  /// with a header, as a spreadsheet pastes it.
  QString toText(const QList<int> &rows = {}) const;
  /// CSV export: `timestamp;value;unit;mode;range;hold`, and `;limit` when a
  /// reading has the limit test's verdict (PASS, FAIL HI, FAIL LO), ISO timestamps
  /// with milliseconds, the value as displayed (with the SI prefix on the
  /// unit). Returns false with @p error on failure or when empty.
  bool write(const QString &path, QString *error = nullptr) const;
  /// write() for .csv, an Excel/OpenDocument sheet for .xlsx/.ods (real
  /// date and number cells, plus an Alarm column).
  bool writeAny(const QString &path, QString *error = nullptr) const;

  /// "2026-09-21 14:03:05.250", the time with its date (clipboard, and
  /// the Time column once the rows span more than one day).
  static QString formatTime(const QDateTime &when);
  /// True while all rows are from the same day: the Time column then shows
  /// only the time of day ("14:03:05.250").
  bool isSingleDay() const { return m_singleDay; }
  /// The Mode column: the decoders' codes as words ("OH" -> "Resistance").
  /// The Mode column: "DC", "AC+DC", "Diode", "Resistance", ...
  static QString modeText(const PortKey &port, quint32 flags);
  /// The export's mode of value @p id: modeText(), for another value than
  /// the main one after its place ("2nd AC", "3rd", "4th"), in English.
  static QString modeColumn(int id, const PortKey &port, quint32 flags);

private:
  RecordingStore *m_own;     ///< used until setStore(), and after that store is gone
  RecordingStore *m_store;
  bool m_singleDay = true;
  QSet<ValueRef> m_hidden;
  bool m_devices = false;   ///< setDevices()
  QString m_ownSource;
  std::function<QString(const QString &)> m_deviceName;
  bool m_otherValues = false;
  bool m_verdicts = false;
  QStringList m_names;
  std::pair<int, int> m_inserting;   ///< rows being inserted, for noteValues()
  /// Rows @p first..@p last came: whether one of them is another value.
  void noteValues(int first, int last);
  void connectStore();
  bool allOneDay() const;
  /// Recomputes m_singleDay after rows came or went; when it flips, the
  /// whole Time column changes its text.
  void updateSingleDay();
};
