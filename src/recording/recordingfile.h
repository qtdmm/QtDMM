// Copyright (c) 2026 The QtDMM developers
// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <QDateTime>
#include <QJsonObject>
#include <QList>
#include <QString>
#include <QStringList>
#include <QVector>
#include <optional>

/// What a value is in a row of a recording file (the column `status`).
enum class CellState : quint8
{
  Ok,                 ///< a number (`ok`)
  Overload,           ///< the meter showed OL (`OL`)
  NegativeOverload,   ///< the meter showed -OL (`-OL`)
  Gap,                ///< no value: stale, off or ended (`gap`)
  NoValue,            ///< the meter showed no number, "----" (`none`)
  Fault,              ///< the meter showed a fault word, "FUSE" (`fault`)
  None                ///< the value is not there yet in this row (empty cells)
};

/// Another value in a recording: of the meter (a second display) or of
/// another meter recorded with it (device); its unit and, row by row as
/// Recording::values, its values and states.
struct RecordingColumn
{
  /// The value (1 = value 2 ...); of the meter 0 = by its place, of
  /// another meter 0 = its main value.
  int id = 0;
  QString device;             ///< the other meter's name; empty: this meter
  QString unit;               ///< without SI prefix
  QVector<double> values;     ///< NaN for anything but Ok
  QVector<CellState> states;  ///< one per row
  /// The limit test's verdict per row as SampleFlag::Limit* bits (LimitTest);
  /// empty: the value had no test.
  QVector<quint32> limits;
};

/// A recording as it goes to and comes from a CSV file: when it started, how
/// often it sampled, its unit and the values in that unit - on the grid of
/// the sample time, or each at its own time (times) - and the meter's other
/// values on the same rows.
struct Recording
{
  QDateTime start;
  int sampleTimeTenths = 1;   ///< sample interval in tenths of a second (>= 1)
  QString unit;               ///< the graph's unit, without SI prefix ("V")
  /// The meter of the main value ("Sim A"), its columns named after it like
  /// those of the others; empty: unnamed (files before 26.4).
  QString device;
  QVector<double> values;
  /// The state of each value; empty = Ok where the value is a number, else
  /// Gap. An overload's NaN carries the sign of -OL.
  QVector<CellState> states;
  /// ms since start for each value; empty = value i at i x the sample time
  QVector<qint64> times;
  /// The limit test's verdict per row, as RecordingColumn::limits.
  QVector<quint32> limits;
  /// The other values, a column group each (value 2, value 3 ...).
  QList<RecordingColumn> extra;
  /// What a QtDMM recording (*.qtdmm-rec) keeps beyond the columns, its
  /// meta.json: "marks" [{t ms since start, color "#aarrggbb", name}],
  /// "devices" [{name, model, protocol, values [{id, unit, quantity, name}], alarms,
  /// limit-test}] (MeterController::recordingInfo()) and what the writer
  /// adds (format, qtdmm, written, start, end, sample-time in s). Empty for
  /// a CSV.
  QJsonObject meta;

  /// The time of value @p i, ms since start.
  qint64 timeAt(int i) const { return times.isEmpty() ? qint64(i) * sampleTimeTenths * 100 : times[i]; }
  CellState stateAt(int i) const
  {
    if (!states.isEmpty())
      return states[i];
    return values[i] == values[i] ? CellState::Ok : CellState::Gap;   // NaN, without <cmath>
  }
  /// Whether the values lie on the grid of the sample time (to the ms the
  /// files keep).
  bool onGrid() const
  {
    for (int i = 0; i < times.size(); ++i)
      if (qAbs(times[i] - qint64(i) * sampleTimeTenths * 100) > 1)
        return false;
    return true;
  }
};

/// The CSV file formats of the recorder, free of widgets and settings so they
/// can be tested on their own. GraphWidget hands a Recording in and out.
///
/// Written: `timestamp;time (s);value;unit;status` with ISO 8601 timestamps
/// (`2026-09-21T14:03:05,250`), the value with an SI prefix on the unit, the
/// status `ok`, `OL`, `-OL` or `gap` (the value then `nan`). Each other value
/// adds `;value 2;unit 2;status 2` and so on; its cells are empty in the rows
/// before it began. A value with a limit test adds a column `limit` (the
/// main value) or `limit 2` ... at the end of the row: `PASS`, `FAIL HI`,
/// `FAIL LO` or empty. The values of other meters recorded with it follow,
/// their columns named after the meter: `Sim B: value;Sim B: unit;Sim B:
/// status`, `Sim B: value 2` ..., `Sim B: limit`.
/// Read: that format, the one before it without status
/// (`timestamp;time (s);value;unit`, `nan` a gap), and the legacy
/// tab-separated export of QtDMM < 0.9
/// (`21.09.2026<TAB>14:03:05[:250]<TAB>value<TAB>unit`). The sample time is
/// derived from the time span and the row count; a CSV row keeps its time
/// (Recording::times).
///
/// A QtDMM recording (`*.qtdmm-rec`) is a ZIP of that CSV as `recording.csv`
/// and `meta.json` (Recording::meta): unpacked, the CSV reads anywhere.
///
/// A CSV for LabPlot (`*.labplot.csv`, written only) has numbers alone:
/// `time[s];value [V];value 2 [A];Sim B: value [V]`, each value in its unit
/// without SI prefix, an empty cell where it is no number. No timestamp,
/// status or limit columns: LabPlot's import takes every column as a number.
/// A LabPlot project (`*.lml`, LabPlotProject) has the same columns and a
/// graph of them, it opens without the import.
namespace RecordingFile
{
/// The suffix of a QtDMM recording.
inline constexpr const char *kSuffix = "qtdmm-rec";
/// The suffix of a CSV for LabPlot, after the dot ("x.labplot.csv").
inline constexpr const char *kLabPlotSuffix = "labplot.csv";
/// The suffix of a LabPlot project.
inline constexpr const char *kLabPlotProjectSuffix = "lml";
/// Reads @p path; on failure returns nothing and sets @p error (translated).
std::optional<Recording> read(const QString &path, QString *error = nullptr);
/// Writes @p recording to @p path; false with @p error on failure or when
/// there is nothing to write.
bool write(const Recording &recording, const QString &path, QString *error = nullptr);
/// The same columns as an Excel (.xlsx) or OpenDocument (.ods) sheet, with
/// real date and number cells (SpreadsheetWriter); the format follows the
/// suffix; `.qtdmm-rec` writes a QtDMM recording, `.labplot.csv` a CSV for
/// LabPlot, `.lml` a LabPlot project. Falls back to CSV for any other suffix.
bool writeAny(const Recording &recording, const QString &path, QString *error = nullptr);

/// A value of a recording as numbers alone, for LabPlot: its name ("value",
/// "Sim B: value 2"), its unit without SI prefix and a number per row, NaN
/// where it is none (OL, a gap, before it began).
struct NumberColumn
{
  QString name;
  QString unit;
  QVector<double> values;
};
/// The values of @p recording as NumberColumn, the main value first; the
/// times of the rows in s.
QList<NumberColumn> numberColumns(const Recording &recording, QVector<double> *seconds = nullptr);

/// The other meters in @p recording (RecordingColumn::device, not
/// Recording::device), in order.
QStringList devices(const Recording &recording);
/// The part of @p recording of the meter @p device (Recording::device: the
/// main value's meter):
/// its main value as the values, its other values as the extra columns,
/// on the same rows and times.
Recording devicePart(const Recording &recording, const QString &device);

/// Factor that brings a value in @p unit to @p baseUnit: 1e-3 for "mV" and
/// "V", 1.0 when the units are the same or unrelated. Older exports wrote the
/// ASCII "u" for micro; SiPrefix::factor() takes both.
double unitScaleFactor(const QString &unit, const QString &baseUnit);
}
