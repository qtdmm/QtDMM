// Copyright (c) 2026 The QtDMM developers
// SPDX-License-Identifier: GPL-3.0-or-later
//
// ReadingsModel: the readings table model. Ring behaviour, model signals
// (QAbstractItemModelTester watches the contracts), the statistics line, tab
// text for the clipboard and the CSV export; at the end the widget's value
// filter.

#include <QtCore>
#include <QJsonArray>
#include <QJsonObject>
#include <QAbstractItemModelTester>
#include <QApplication>
#include <QLabel>
#include <QSignalSpy>
#include <QTableView>
#include <QTemporaryDir>

#include "recording/readingsmodel.h"
#include "core/reading.h"
#include "recording/recordingfile.h"
#include "recording/recordingstore.h"
#include "ui/views/readingswidget.h"
#include "testutil.h"

static ReadingsModel::Entry entry(double dval, const QString &val, const QString &unit, int secs, int id = 0)
{
  ReadingsModel::Entry e;
  e.when = QDateTime(QDate(2026, 9, 21), QTime(14, 3, 5, 250)).addSecs(secs);
  e.dval = dval;
  e.val = val;
  e.unit = unit;
  e.port = PortKey::fromString("voltage.dc");
  e.flags = SampleFlag::DC;
  e.range = "AUTO";
  e.id = id;
  return e;
}

int main(int argc, char **argv)
{
  if (qEnvironmentVariableIsEmpty("QT_QPA_PLATFORM"))
    qputenv("QT_QPA_PLATFORM", "offscreen");
  QApplication app(argc, argv);
  ReadingsModel log;
  QAbstractItemModelTester tester(&log, QAbstractItemModelTester::FailureReportingMode::Warning);

  int inserted = 0, removed = 0, resets = 0;
  QObject::connect(&log, &QAbstractItemModel::rowsInserted, [&] { ++inserted; });
  QObject::connect(&log, &QAbstractItemModel::rowsRemoved, [&] { ++removed; });
  QObject::connect(&log, &QAbstractItemModel::modelReset, [&] { ++resets; });

  // --- 1. rows and cells ---
  check(log.rowCount() == 0 && log.columnCount() == ReadingsModel::ColumnCount, "empty model");
  log.append(entry(1.234, "1.234", "V", 0));
  log.append(entry(0.0122, "12.2", "mV", 1));
  log.append(entry(0, " OL ", "V", 2));
  log.append(entry(50.0, "50.0", "Hz", 3, 1));   // a secondary value
  check(log.rowCount() == 4 && inserted == 4, "four rows appended");
  check(log.isSingleDay() && log.data(log.index(0, ReadingsModel::Time)).toString() == "14:03:05.250",
        "time cell without the date while all rows are from one day: " + log.data(log.index(0, ReadingsModel::Time)).toString());
  check(log.data(log.index(1, ReadingsModel::Value)).toString() == "12.2", "value cell");
  check(log.data(log.index(1, ReadingsModel::Unit)).toString() == "mV", "unit cell");
  check(log.data(log.index(2, ReadingsModel::Value)).toString() == "OL", "overload cell is trimmed");
  check(log.data(log.index(3, ReadingsModel::Number)).toInt() == 2 && log.data(log.index(0, ReadingsModel::Number)).toInt() == 1
          && !log.data(log.index(3, ReadingsModel::Mode)).toString().startsWith("2nd") && log.hasOtherValues(),
        "the value by its number, the mode the function alone: " + log.data(log.index(3, ReadingsModel::Mode)).toString());
  check(ReadingsModel::modeColumn(2, PortKey{}, 0) == "3rd" && ReadingsModel::modeColumn(3, PortKey{}, 0) == "4th"
        && ReadingsModel::modeColumn(0, PortKey{}, 0).isEmpty(), "the third and fourth value by their place: "
        + ReadingsModel::modeColumn(2, PortKey{}, 0) + "/" + ReadingsModel::modeColumn(3, PortKey{}, 0));
  check(log.data(log.index(1, ReadingsModel::Value), ReadingsModel::DvalRole).toDouble() == 0.0122, "dval role");
  check(log.headerData(ReadingsModel::Value, Qt::Horizontal).toString() == "Value", "header");
  {
    // a meter whose secondary display is off sends an empty second value:
    // the display clears it, the log does not take it
    Reading off;
    off.id = 1;
    off.msecs = QDateTime::currentMSecsSinceEpoch();
    const int before = log.rowCount();
    log.appendReading(off);
    check(log.rowCount() == before, "an empty second value is no row");
  }

  // --- 1b. a day change brings the date into every row, and it goes again
  //         when the rows of the first day have been dropped ---
  {
    ReadingsModel days;
    int timeChanges = 0;
    QObject::connect(&days, &QAbstractItemModel::dataChanged, [&](const QModelIndex &tl, const QModelIndex &br)
    {
      if (tl.column() == ReadingsModel::Time && br.column() == ReadingsModel::Time && tl.row() == 0)
        ++timeChanges;
    });
    days.append(entry(1.0, "1.000", "V", 0));
    days.append(entry(2.0, "2.000", "V", 86400));   // the next day
    check(!days.isSingleDay() && timeChanges == 1, "the second day switches the dates on");
    check(days.data(days.index(0, ReadingsModel::Time)).toString() == "2026-09-21 14:03:05.250", "time cell with date");
    check(days.data(days.index(1, ReadingsModel::Time)).toString() == "2026-09-22 14:03:05.250", "next day's cell");
    days.setMaxRows(1);
    check(days.isSingleDay() && timeChanges == 2, "one day left: the dates go");
    check(days.data(days.index(0, ReadingsModel::Time)).toString() == "14:03:05.250", "time only again");
    days.append(entry(3.0, "3.000", "V", 90000));
    days.clear();
    check(days.isSingleDay(), "cleared log is single-day");
  }

  // --- 2. statistics skip overloads and secondary values ---
  ReadingsModel::Stats s = log.stats();
  check(s.id == 0 && s.count == 3 && s.numeric == 2,
        QString("stats of the main value: count %1 numeric %2").arg(s.count).arg(s.numeric));
  check(qFuzzyCompare(s.min, 0.0122) && qFuzzyCompare(s.max, 1.234), "stats min/max");
  check(qFuzzyCompare(s.mean, (1.234 + 0.0122) / 2), "stats mean");
  check(s.unit == "V", "stats unit is the newest main reading's base unit, not the 2nd value's: " + s.unit);
  {
    const ReadingsModel::Stats second = log.stats(1);
    check(second.count == 1 && second.numeric == 1 && second.unit == "Hz" && qFuzzyCompare(second.max, 50.0),
          QString("stats of the second value: %1 rows, %2").arg(second.count).arg(second.unit));
    // the main value left out of the table: the line is of the first one shown
    log.setValueHidden(0, true);
    check(log.stats().id == 1, "stats of the first value shown");
    log.setValueHidden(0, false);
  }
  {
    // another function before: not in the numbers (611 Ω were "Max 611 V")
    ReadingsModel mixed;
    ReadingsModel::Entry ohm = entry(611, "611.0", "Ohm", 0);
    ohm.port = PortKey::fromString("resistance");
    ohm.flags = 0;
    mixed.append(ohm);
    ReadingsModel::Entry ac = entry(5, "5.000", "V", 1);
    ac.port = PortKey::fromString("voltage.ac");
    mixed.append(ac);
    mixed.append(entry(-0.17333, "-173.33", "mV", 2));
    mixed.append(entry(0.03704, "37.04", "mV", 3));
    const ReadingsModel::Stats m = mixed.stats();
    check(m.count == 4 && m.numeric == 2 && qFuzzyCompare(m.max, 0.03704) && qFuzzyCompare(m.min, -0.17333) && m.unit == "V",
          QString("stats over the newest function only: %1 of %2, max %3").arg(m.numeric).arg(m.count).arg(m.max));
  }

  // --- 3. ring: the oldest rows go ---
  log.setMaxRows(3);
  check(log.rowCount() == 3 && removed == 1, "shrinking maxRows trims the oldest");
  check(log.data(log.index(0, ReadingsModel::Value)).toString() == "12.2", "first row is now the second reading");
  log.append(entry(2.0, "2.000", "V", 4));
  check(log.rowCount() == 3 && removed == 2, "append at the limit drops one");
  check(log.data(log.index(2, ReadingsModel::Value)).toString() == "2.000", "newest row is last");
  log.setMaxRows(0);
  check(log.maxRows() == 1 && log.rowCount() == 1, "maxRows is at least 1");
  log.setMaxRows(1000);

  // --- 3b. paused: readings pass by ---
  log.setPaused(true);
  log.append(entry(9.0, "9.000", "V", 5));
  check(log.rowCount() == 1 && log.isPaused(), "paused log drops the reading");
  log.setPaused(false);
  log.append(entry(9.0, "9.000", "V", 5));
  check(log.rowCount() == 2, "resumed log takes readings again");

  // --- 3c. the meter's fixed-width text without leading zeros, as the
  //          meter shows it (table, copy and export) ---
  log.append(entry(0.0, "000.00", "mV", 6));
  log.append(entry(-0.0293, "-029.30", "mV", 7));
  check(log.data(log.index(2, ReadingsModel::Value)).toString() == "0.00", "000.00 -> 0.00");
  check(log.data(log.index(3, ReadingsModel::Value)).toString() == "-29.30", "-029.30 -> -29.30");

  // --- 4. clipboard text ---
  log.clear();
  check(log.rowCount() == 0 && resets == 1, "clear resets");
  log.append(entry(1.0, "1.000", "V", 0));
  log.append(entry(2.0, "2.000", "V", 1));
  const QString text = log.toText();
  const QStringList lines = text.split('\n', Qt::SkipEmptyParts);
  // the log had a second value before the clear: the number stays
  check(lines.size() == 3 && lines[0].startsWith("Time\tNo.\tValue\tUnit"), "tab text has a header: " + lines.value(0));
  check(lines[2] == "2026-09-21 14:03:06.250\t1\t2.000\tV\tDC\tAUTO\t", "tab text row: " + lines.value(2));
  check(log.toText({1}).split('\n', Qt::SkipEmptyParts).size() == 2, "selected rows only");
  {
    // more values: copied with their number; one left out of the table is
    // left out of the copy, not of the export
    ReadingsModel more;
    more.append(entry(1.0, "1.000", "V", 0));
    more.append(entry(50.0, "50.0", "Hz", 0, 1));
    more.append(entry(2.0, "2.000", "V", 1));
    QStringList rows = more.toText().split('\n', Qt::SkipEmptyParts);
    check(rows.size() == 4 && rows[0].startsWith("Time\tNo.\tValue") && rows[2].contains("\t2\t50.0\tHz"),
          "copied with the number: " + rows.value(0) + " | " + rows.value(2));
    int changed = 0;
    QObject::connect(&more, &ReadingsModel::hiddenValuesChanged, [&] { ++changed; });
    more.setValueHidden(1, true);
    more.setValueHidden(1, true);
    rows = more.toText().split('\n', Qt::SkipEmptyParts);
    check(changed == 1 && rows.size() == 3 && !rows.join('\n').contains("Hz"), "a value left out is not copied");
    QTemporaryDir dir;
    check(more.write(dir.filePath("more.csv")), "export with a value left out");
    QFile all(dir.filePath("more.csv"));
    check(all.open(QIODevice::ReadOnly) && QString::fromUtf8(all.readAll()).contains("2nd"), "the export keeps it, as 2nd");
  }

  // --- 5. CSV export ---
  QTemporaryDir tmp;
  const QString path = tmp.filePath("readings.csv");
  QString error;
  check(log.write(path, &error), "write succeeds: " + error);
  QFile f(path);
  check(f.open(QIODevice::ReadOnly | QIODevice::Text), "csv opens");
  const QStringList csv = QString::fromUtf8(f.readAll()).split('\n', Qt::SkipEmptyParts);
  check(csv.size() == 3 && csv[0] == "timestamp;value;unit;mode;range;hold", "csv header: " + csv.value(0));
  check(csv[1] == "2026-09-21T14:03:05,250;1.000;V;DC;AUTO;0", "csv row: " + csv.value(1));
  ReadingsModel empty;
  check(!empty.write(path, &error) && !error.isEmpty(), "empty log refuses to export");
  check(!log.write(tmp.filePath("no/such/dir/x.csv"), &error), "unwritable path fails");

  // --- 5b. the limit test: a column once a verdict has come, also in the CSV ---
  {
    ReadingsModel judged;
    judged.append(entry(1.0, "1.000", "V", 0));
    check(!judged.hasVerdicts(), "limit: no verdict, no column");
    bool shown = false;
    QObject::connect(&judged, &ReadingsModel::verdictsChanged, [&](bool on) { shown = on; });
    ReadingsModel::Entry fail = entry(5.2, "5.200", "V", 1);
    fail.flags |= SampleFlag::LimitHigh;
    judged.append(fail);
    check(judged.hasVerdicts() && shown && judged.data(judged.index(1, ReadingsModel::Limit)).toString() == "FAIL HI"
          && judged.data(judged.index(0, ReadingsModel::Limit)).toString().isEmpty(), "limit: the verdict in its column");
    check(judged.data(judged.index(1, ReadingsModel::Limit), Qt::ForegroundRole).value<QColor>() == QColor(0xc6, 0x28, 0x28),
          "limit: FAIL in red");
    const QString p = tmp.filePath("judged.csv");
    QFile jf(p);
    check(judged.write(p, &error) && jf.open(QIODevice::ReadOnly | QIODevice::Text)
          && QString::fromUtf8(jf.readAll()).split('\n').value(2).endsWith(";AUTO;0;FAIL HI"), "limit: the CSV has the column");
  }

  // --- 5b. a note of the graph marks the reading nearest to its time;
  //          the store's marks: notes in the order of time, meta.json ---
  {
    ReadingsModel noted;
    for (int secs : { 0, 10, 20 })
      noted.append(entry(1.0, "1.000", "V", secs));
    const QDateTime t0 = entry(0, "", "V", 0).when;
    noted.markAt(t0.addSecs(12), QColor(0x1e, 0x90, 0xff), "Load on");
    noted.markAt(t0.addSecs(16), QColor(0x1e, 0x90, 0xff), "Load off");
    noted.markAt(t0.addSecs(99), QColor(0x1e, 0x90, 0xff), "Late");
    check(noted.data(noted.index(1, 0), Qt::ToolTipRole).toString() == "Load on"
            && noted.data(noted.index(2, 0), Qt::ToolTipRole).toString() == "Late"
            && noted.data(noted.index(0, 0), Qt::ToolTipRole).toString().isEmpty(),
          "notes: the nearest reading marked");

    Recording rec;
    rec.start = t0;
    rec.sampleTimeTenths = 10;
    rec.unit = "V";
    rec.values = { 1.0, 2.0, 3.0, 4.0 };
    // a file of before: a mark without kind is an alarm's
    rec.meta["marks"] = QJsonArray{ QJsonObject{ { "t", 2000.0 }, { "color", "#ffd03030" }, { "name", "Alarm Hi" } } };
    RecordingStore store;
    store.load(rec);
    check(store.marks().size() == 1 && store.marks().at(0).kind == RecordingStore::MarkKind::Alarm, "notes: an old mark is an alarm's");
    check(store.addNote(1000, "Start", "R1 10 k") == 0 && store.addNote(2000, "Same time", {}) == 2
            && store.addNote(-1, "Now", {}) == 3 && store.marks().at(3).t == store.markTime(),
          "notes: by time, after those of the same time; -1 at the newest reading");
    store.setMarkText(1, "Alarm Hi", "fuse checked");
    store.removeMark(2);
    const QJsonArray marks = store.toRecording().meta.value("marks").toArray();
    check(marks.size() == 3 && marks.at(0).toObject().value("kind").toString() == "note"
            && marks.at(0).toObject().value("note").toString() == "R1 10 k"
            && !marks.at(1).toObject().contains("kind") && marks.at(1).toObject().value("note").toString() == "fuse checked",
          "notes: kind and text in meta.json, an alarm's mark without kind");
    Recording again = rec;
    again.meta["marks"] = marks;
    RecordingStore back;
    back.load(again);
    check(back.marks().size() == 3 && back.marks().at(0).kind == RecordingStore::MarkKind::Note
            && back.marks().at(0).note == "R1 10 k" && back.marks().at(0).color == RecordingStore::kNoteColor
            && back.marks().at(1).note == "fuse checked",
          "notes: read back");

    // dragged: the mark goes to its new time, in the order of time
    check(back.setMarkTime(0, 2500) == 1 && back.marks().at(1).name == "Start" && back.marks().at(0).name == "Alarm Hi",
          "notes: moved after the alarm's mark");
    // the readings table's mark goes along
    noted.moveMark(t0.addSecs(12), t0.addSecs(1), QColor(0x1e, 0x90, 0xff), "Load on");
    check(noted.data(noted.index(1, 0), Qt::ToolTipRole).toString().isEmpty()
            && noted.data(noted.index(0, 0), Qt::ToolTipRole).toString() == "Load on",
          "notes: the table's mark moved");

    // in a CSV: mark;note on the nearest row, quoted, and back as notes
    Recording withMarks = rec;
    withMarks.meta["marks"] = QJsonArray{
      QJsonObject{ { "t", 1100.0 }, { "color", "#ff1e90ff" }, { "name", "Load on" }, { "note", "R1; 10 k\n\"hot\"" }, { "kind", "note" } },
      QJsonObject{ { "t", 1200.0 }, { "color", "#ffd03030" }, { "name", "Alarm Hi" } },
      QJsonObject{ { "t", 2900.0 }, { "color", "#ff1e90ff" }, { "name", "End" }, { "kind", "note" } } };
    const QString csv = tmp.filePath("marks.csv");
    QString err;
    check(RecordingFile::write(withMarks, csv, &err), "marks: CSV written " + err);
    QFile cf(csv);
    check(cf.open(QIODevice::ReadOnly | QIODevice::Text), "marks: CSV opened");
    const QString text = QString::fromUtf8(cf.readAll());
    cf.close();
    check(text.section('\n', 0, 0).endsWith(";status;mark;note")
            && text.contains(";Load on | Alarm Hi;\"R1; 10 k\n\"\"hot\"\" | \"\n") && text.contains(";End;\n"),
          "marks: the columns, two on one row, quoted: " + text);
    const std::optional<Recording> csvBack = RecordingFile::read(csv, &err);
    const QJsonArray rm = csvBack ? csvBack->meta.value("marks").toArray() : QJsonArray();
    check(csvBack && csvBack->values.size() == 4 && rm.size() == 3 && rm.at(0).toObject().value("t").toDouble() == 1000
            && rm.at(0).toObject().value("note").toString() == "R1; 10 k\n\"hot\""
            && rm.at(1).toObject().value("name").toString() == "Alarm Hi" && rm.at(2).toObject().value("t").toDouble() == 3000
            && rm.at(2).toObject().value("kind").toString() == "note",
          "marks: read back as notes at their rows");
    const QString rpath = tmp.filePath("marks.qtdmm-rec");
    check(RecordingFile::writeAny(withMarks, rpath, &err), "marks: recording written " + err);
    const std::optional<Recording> recBack = RecordingFile::read(rpath, &err);
    check(recBack && recBack->meta.value("marks").toArray().at(0).toObject().value("t").toDouble() == 1100,
          "marks: a .qtdmm-rec keeps them exactly in meta.json, not in its CSV");
  }

  // --- 6. spreadsheet export ---
  {
    log.markLast(QColor("#d82222"), "Low voltage");
    QString err;
    check(log.writeAny(tmp.filePath("r.xlsx"), &err), "xlsx: " + err);
    check(log.writeAny(tmp.filePath("r.ods"), &err), "ods: " + err);
    check(log.writeAny(tmp.filePath("r2.csv"), &err), "csv through writeAny: " + err);
    check(!empty.writeAny(tmp.filePath("e.ods"), &err), "empty refuses");
  }

  // --- 7. a thin model over a store: two logs of one store see the same
  //         rows, the limit and the pause are the store's, and the table
  //         follows a series that is independent of the recording ---
  {
    RecordingStore store;
    ReadingsModel mine;
    mine.append(entry(1.0, "1.000", "V", 0));   // goes to its own store
    mine.setStore(&store);
    check(mine.rowCount() == 0 && mine.store() == &store, "setStore: the log shows the new store, empty");
    mine.setMaxRows(2);
    check(store.readingCapacity() == 2, "setStore: the limit is the store's");
    QAbstractItemModelTester shared(&mine, QAbstractItemModelTester::FailureReportingMode::Warning);

    ReadingsModel other;
    other.setStore(&store);
    int rowsSeen = 0;
    QObject::connect(&other, &QAbstractItemModel::rowsInserted, [&] { ++rowsSeen; });

    Reading r;
    r.value = 0.0122;
    r.text = "12.2";
    r.unit = "mV";
    r.port = PortKey::fromString("voltage.dc");
    r.flags = SampleFlag::DC | SampleFlag::Autorange;
    r.range = "AUTO";
    r.msecs = QDateTime(QDate(2026, 9, 21), QTime(14, 3, 5, 250)).toMSecsSinceEpoch();
    store.setReading(r);   // the recorder's feed, as the MeterController makes it
    store.setReading(r);
    store.setReading(r);
    check(mine.rowCount() == 2 && other.rowCount() == 2 && rowsSeen == 3,
          QString("shared store: %1 and %2 rows").arg(mine.rowCount()).arg(other.rowCount()));
    check(mine.data(mine.index(0, ReadingsModel::Value)).toString() == "12.2" &&
          mine.data(mine.index(0, ReadingsModel::Time)).toString() == "14:03:05.250",
          "shared store: the cells come from the store's row");

    mine.markLast(QColor("#d82222"), "Alarm 1");
    check(other.data(other.index(1, 0), Qt::ToolTipRole).toString() == "Alarm 1" &&
          other.data(other.index(1, 0), Qt::BackgroundRole).value<QColor>().alpha() == 70,
          "shared store: the mark shows in the other view");

    other.setPaused(true);
    store.setReading(r);
    check(mine.rowCount() == 2 && mine.isPaused(), "shared store: the pause is the store's");
    other.setPaused(false);
    mine.clear();
    check(other.rowCount() == 0 && store.readingCount() == 0, "shared store: clear empties it for both");
  }

  // --- 8. the store goes before the log (the main window deletes its views
  //         one by one): the log falls back to its own store, empty ---
  {
    ReadingsModel log;
    log.append(entry(1.0, "1.000", "V", 0));   // stays in its own store
    QAbstractItemModelTester tester(&log, QAbstractItemModelTester::FailureReportingMode::Warning);
    {
      RecordingStore store;
      log.setStore(&store);
      log.append(entry(2.0, "2.000", "V", 0));
      check(log.rowCount() == 1, "store gone: the shared store has the row");
    }
    check(log.store() != nullptr && log.rowCount() == 0 && log.stats().count == 0,
          QString("store gone: the log must be empty, has %1 rows").arg(log.rowCount()));
    log.append(entry(3.0, "3.000", "V", 0));
    check(log.rowCount() == 1 && log.entry(0).dval == 3.0, "store gone: the log goes on with its own store");
  }

  // --- 7. the widget: the number column with more values, a value left out
  //        of the table, the statistics line names its value ---
  {
    ReadingsWidget widget;
    widget.resize(700, 300);
    ReadingsModel *model = widget.log();
    auto *view = widget.findChild<QTableView *>();
    QLabel *stats = nullptr;
    for (QLabel *l : widget.findChildren<QLabel *>())
      if (l->textInteractionFlags() & Qt::TextSelectableByMouse)
        stats = l;
    model->append(entry(1.0, "1.000", "V", 0));
    check(view && view->isColumnHidden(ReadingsModel::Number), "one value: no number column");
    check(stats && !stats->text().contains("Value 1"), "one value: the line names none: " + (stats ? stats->text() : ""));
    model->append(entry(0.5, "0.500", "A", 0, 1));
    model->append(entry(2.0, "2.000", "V", 1));
    model->append(entry(0.6, "0.600", "A", 1, 1));
    model->setValueNames({ "Battery voltage", "Battery current" });
    check(!view->isColumnHidden(ReadingsModel::Number), "two values: the number column");
    check(stats->text().contains("Value\u00a01\u00a0(Battery\u00a0voltage):") && stats->text().contains("2\u00a0reading"),
          "the line names its value and counts its readings: " + stats->text());
    model->setValueHidden(1, true);
    check(view->model()->rowCount() == 2 && view->model()->index(1, ReadingsModel::Value).data().toString() == "2.000",
          QString("value 2 left out: %1 rows").arg(view->model()->rowCount()));
    model->append(entry(0.7, "0.700", "A", 2, 1));
    model->append(entry(3.0, "3.000", "V", 2));
    check(view->model()->rowCount() == 3, "new rows of value 2 stay out");
    model->setValueHidden(1, false);
    model->setValueHidden(0, true);
    check(stats->text().contains("Value\u00a02\u00a0(Battery\u00a0current)") && view->model()->rowCount() == 3,
          "value 1 left out: the line is of value 2: " + stats->text());
    // a reading reaches the line a moment later, many at once in one go
    const QString before = stats->text();
    for (int i = 0; i < 5; ++i)
      model->append(entry(0.8, "0.800", "A", 3 + i, 1));
    check(stats->text() == before, "the line waits for the readings to come together");
    QEventLoop wait;
    QTimer::singleShot(400, &wait, &QEventLoop::quit);
    wait.exec();
    check(stats->text() != before && stats->text().contains("8\u00a0reading"), "then it has them: " + stats->text());
    model->setValueHidden(0, false);
    check(view->model() == model && view->model()->rowCount() == 11, "all shown: the table on the model again");
  }

  // several meters: their readings in one table with the Device column, the
  // window's meter's values by an empty source, the others' by theirs
  {
    ReadingsModel multi;
    QAbstractItemModelTester tester(&multi);
    multi.setStore(nullptr);
    QSignalSpy devices(&multi, &ReadingsModel::devicesChanged);
    multi.setDevices(true, "device:a", [](const QString &source) { return source == "device:a" ? "Sim A" : "Sim B"; });
    check(devices.size() == 1 && multi.hasDevices(), "Device column on");
    auto reading = [](double v, const QString &source, int id, int ms)
    {
      Reading r;
      r.value = v;
      r.text = QString::number(v, 'f', 3);
      r.unit = "V";
      r.id = id;
      r.source = source;
      r.msecs = QDateTime(QDate(2026, 10, 28), QTime(12, 0)).toMSecsSinceEpoch() + ms;
      return r;
    };
    multi.appendReading(reading(1.5, "device:a", 0, 0));
    multi.appendReading(reading(2.5, "device:b", 0, 100));
    multi.appendReading(reading(1.6, "device:a", 0, 200));
    multi.appendReading(reading(0.3, "device:b", 1, 300));
    check(multi.rowCount() == 4 && multi.data(multi.index(1, ReadingsModel::Device)).toString() == "Sim B"
            && multi.data(multi.index(0, ReadingsModel::Device)).toString() == "Sim A",
          "the meter of each row");
    check(multi.valueOf(0) == ValueRef(0) && multi.valueOf(3) == ValueRef(1, "device:b"),
          "the window's by an empty source, the other's by its own");
    const ReadingsModel::Stats st = multi.stats();
    check(st.numeric == 2 && qAbs(st.mean - 1.55) < 1e-9, QString("stats of the window's main value only: %1").arg(st.mean));
    multi.setValueHidden(ValueRef(0, "device:b"), true);
    check(!multi.rowShown(1) && multi.rowShown(0) && multi.rowShown(3), "another meter's value hidden");
    const QStringList lines = multi.toText().split('\n', Qt::SkipEmptyParts);
    check(lines.size() == 4 && lines[0].startsWith("Time\tDevice\t") && lines[2].contains("\tSim A\t"),
          "copied with the Device column: " + lines.value(0));
    QTemporaryDir dir;
    const QString csv = dir.filePath("multi.csv");
    check(multi.write(csv), "csv written");
    QFile f(csv);
    check(f.open(QIODevice::ReadOnly), "csv opens");
    check(QString::fromUtf8(f.readLine()).startsWith("device;timestamp;"), "csv with the device first");
    multi.setDevices(false, "device:a", {});
    check(!multi.hasDevices() && multi.toText().split('\n').value(0).startsWith("Time\tNo."), "one meter: no Device column");
  }

  // a table without the window's meter: the stats of the first value another
  // meter's rows show
  {
    ReadingsModel others;
    others.setStore(nullptr);
    others.setDevices(true, "device:a", [](const QString &) { return QStringLiteral("Sim B"); });
    for (int i = 0; i < 3; ++i)
    {
      Reading r;
      r.value = 2.0 + i;
      r.text = QString::number(r.value, 'f', 3);
      r.unit = "V";
      r.source = "device:b";
      r.msecs = QDateTime(QDate(2026, 10, 28), QTime(12, 0)).toMSecsSinceEpoch() + i * 100;
      others.appendReading(r);
    }
    const ReadingsModel::Stats st = others.stats();
    check(st.source == "device:b" && st.id == 0 && st.count == 3 && qAbs(st.mean - 3.0) < 1e-9,
          QString("another meter's stats: %1 readings of %2").arg(st.count).arg(st.source));
  }

  if (failed == 0)
    qInfo() << "All reading log tests passed.";
  else
    qWarning() << failed << "reading log test(s) failed.";
  return failed == 0 ? 0 : 1;
}
