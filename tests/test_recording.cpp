// Copyright (c) 2026 The QtDMM developers
// SPDX-License-Identifier: GPL-3.0-or-later
//
// RecordingFile: the recorder's CSV formats without any widget. Reads the
// fixtures under tests/data/graph, checks the derived sample time, unit
// scaling (µ and the older ASCII u), the legacy tab format, round trips and
// the error paths.

#include <QtCore>
#include <QJsonArray>
#include <QJsonObject>

#include "recording/recordingfile.h"
#include "recording/zipfile.h"
#include "core/sampletypes.h"
#include "testutil.h"

int main(int argc, char **argv)
{
  QCoreApplication app(argc, argv);
  if (argc != 2)
  {
    qCritical() << "usage: test_recording <tests/data/graph>";
    return 1;
  }
  const QString dataDir = QString::fromLocal8Bit(argv[1]);
  QTemporaryDir tmp;

  // --- 1. every fixture reads, with a plausible sample time ---
  for (const char *name : {"legacy.txt", "legacy_nan.txt", "new_dpoint.csv", "new_komma_ms.csv", "new_larger.csv"})
  {
    QString err;
    const auto rec = RecordingFile::read(dataDir + "/" + name, &err);
    check(rec.has_value(), QString("%1 reads (%2)").arg(name, err));
    if (rec)
    {
      check(!rec->values.isEmpty() && rec->start.isValid(), QString("%1: values and start").arg(name));
      check(rec->sampleTimeTenths >= 1, QString("%1: sample time >= 1").arg(name));
      check(!rec->unit.isEmpty(), QString("%1: has a unit").arg(name));
    }
  }

  // --- 2. new_larger.csv: 272 rows over ~54 s -> 2 tenths, start from the first row ---
  {
    const auto rec = RecordingFile::read(dataDir + "/new_larger.csv");
    check(rec && rec->sampleTimeTenths == 2, QString("sample time 0.2 s (got %1)").arg(rec ? rec->sampleTimeTenths : -1));
    check(rec && rec->start == QDateTime(QDate(2025, 8, 11), QTime(10, 25, 22, 289)), "start = first timestamp");
    check(rec && qFuzzyCompare(rec->values[0], 0.0327), "first value");
  }

  // --- 3. legacy: one row per second -> 10 tenths, nan -> 0 ---
  {
    const auto rec = RecordingFile::read(dataDir + "/legacy.txt");
    check(rec && rec->sampleTimeTenths == 10, "legacy: 1 s sample time");
    const auto nan = RecordingFile::read(dataDir + "/legacy_nan.txt");
    check(nan.has_value(), "legacy with nan reads");
    if (nan)
    {
      // old QtDMM wrote an overload as nan: it is a gap again, not a 0
      check(std::count_if(nan->values.cbegin(), nan->values.cend(), [](double v) { return std::isnan(v); }) == 17,
            "nan stays a gap");
      check(nan->values.size() == 47 && nan->values[6] == 0.011452, "the values around the gaps stay");
    }
  }

  // --- 4. units: values come back in the base unit, µ and u alike; a
  //        different prefix in a later row is scaled ---
  auto writeCsv = [&](const QString &name, const QString &body)
  {
    const QString path = tmp.path() + "/" + name;
    QFile f(path);
    check(f.open(QIODevice::WriteOnly | QIODevice::Text), "temp file opens for writing");
    QTextStream(&f) << "timestamp;time (s);value;unit\n" << body;
    return path;
  };
  for (const QString &micro : {QString::fromUtf8("µ"), QString("u")})
  {
    const auto rec = RecordingFile::read(writeCsv("micro.csv",
      "2026-01-01T00:00:00,000;0;2.5;" + micro + "A\n2026-01-01T00:00:01,000;1;1.5;mA\n2026-01-01T00:00:02,000;2;3;A\n"));
    check(rec && rec->unit == "A", "micro: unit A");
    check(rec && qFuzzyCompare(rec->values[0], 2.5e-6), "micro " + micro + ": 2.5e-6 A");
    check(rec && qFuzzyCompare(rec->values[1], 1.5e-3), "milli row scaled");
    check(rec && qFuzzyCompare(rec->values[2], 3.0), "base row unscaled");
  }

  // --- 5. write -> read round trip, and the written text itself ---
  {
    Recording rec;
    rec.start = QDateTime(QDate(2026, 9, 21), QTime(14, 3, 5, 250));
    rec.sampleTimeTenths = 5;
    rec.unit = "V";
    rec.values = {0.0327, 1234.5, 2.5e-6, -0.5};
    const QString path = tmp.path() + "/out.csv";
    QString err;
    check(RecordingFile::write(rec, path, &err), "write succeeds: " + err);
    QFile f(path);
    check(f.open(QIODevice::ReadOnly | QIODevice::Text), "written file opens");   // Text: the file has \r\n on Windows
    const QStringList lines = QString::fromUtf8(f.readAll()).split('\n', Qt::SkipEmptyParts);
    check(lines.size() == 5 && lines[0] == "timestamp;time (s);value;unit;status", "header and four rows");
    check(lines.size() > 2 && lines[2].startsWith("2026-09-21T14:03:05,750;0.5;"), "second row 0.5 s later: " + (lines.size() > 2 ? lines[2] : ""));
    check(lines.size() > 2 && lines[2].endsWith(";kV;ok"), "1234.5 V written with the k prefix: " + (lines.size() > 2 ? lines[2] : ""));
    check(lines.size() > 3 && lines[3].endsWith(QString::fromUtf8(";µV;ok")), "2.5e-6 V written as µV");
    const auto back = RecordingFile::read(path, &err);
    check(back.has_value(), "round trip reads: " + err);
    if (back)
    {
      check(back->start == rec.start && back->sampleTimeTenths == 5 && back->unit == "V", "round trip: start, sample time, unit");
      check(back->values.size() == 4, "round trip: four values");
      for (int i = 0; i < qMin(4, back->values.size()); ++i)
        check(std::fabs(back->values[i] - rec.values[i]) <= 1e-6 * std::fabs(rec.values[i]) + 1e-12,
              QString("round trip value %1: %2 vs %3").arg(i).arg(back->values[i]).arg(rec.values[i]));
    }
  }

  // --- 5b. the status: OL, -OL and a gap apart, an old file without it
  //          reads nan as a gap; a status that does not fit is no file ---
  {
    const QString path = tmp.path() + "/status.csv";
    QFile f(path);
    check(f.open(QIODevice::WriteOnly | QIODevice::Text), "status file opens");
    QTextStream(&f) << "timestamp;time (s);value;unit;status;value 2;unit 2;status 2\n"
                       "2026-01-01T00:00:00,000;0;1;V;ok;;;\n"
                       "2026-01-01T00:00:01,000;1;nan;V;OL;25;A;ok\n"
                       "2026-01-01T00:00:02,000;2;nan;V;-OL;nan;A;gap\n"
                       "2026-01-01T00:00:03,000;3;nan;V;gap;1.5;mA;ok\n";
    f.close();
    QString err;
    const auto rec = RecordingFile::read(path, &err);
    check(rec && rec->values.size() == 4, "status: reads: " + err);
    if (rec)
    {
      check(rec->stateAt(0) == CellState::Ok && rec->stateAt(1) == CellState::Overload
              && rec->stateAt(2) == CellState::NegativeOverload && rec->stateAt(3) == CellState::Gap,
            "status: ok, OL, -OL, gap");
      check(std::signbit(rec->values[2]) && !std::signbit(rec->values[1]), "status: the NaN of -OL is negative");
      check(rec->extra.size() == 1 && rec->extra[0].unit == "A" && rec->extra[0].states.size() == 4
              && rec->extra[0].states[0] == CellState::None && rec->extra[0].states[2] == CellState::Gap,
            "status: value 2 with its unit, empty in the first row");
      // another unit in a later row of the column: scaled as the main value is
      check(rec->extra.size() == 1 && rec->extra[0].values.value(3) == 1.5e-3, "status: a later prefix scaled");
      const QString out = tmp.path() + "/status_out.csv";
      check(RecordingFile::write(*rec, out, &err), "status: write: " + err);
      QFile o(out);
      check(o.open(QIODevice::ReadOnly | QIODevice::Text), "status: written file opens");
      const QStringList lines = QString::fromUtf8(o.readAll()).split('\n', Qt::SkipEmptyParts);
      check(lines.value(3).endsWith(QString::fromUtf8(";nan;V;-OL;nan;A;gap")) && lines.value(1).endsWith(";1;V;ok;;;"),
            "status: written again: " + lines.join(" | "));
    }
    // only value 3 exported, and a value with an exponent (R14-03, R14-07)
    const QString threePath = tmp.path() + "/three.csv";
    QFile t(threePath);
    check(t.open(QIODevice::WriteOnly | QIODevice::Text), "value 3 file opens");
    QTextStream(&t) << "timestamp;time (s);value;unit;status;value 3;unit 3;status 3\n"
                       "2026-01-01T00:00:00,000;0;5.55111512313e-17;V;ok;2;A;ok\n";
    t.close();
    const auto three = RecordingFile::read(threePath, &err);
    check(three && three->extra.size() == 1 && three->extra[0].id == 2 && std::fabs(three->values.value(0)) < 1e-15,
          "value 3 keeps its number, an exponent reads: " + err);
    if (three)
    {
      const QString out = tmp.path() + "/three_out.csv";
      check(RecordingFile::write(*three, out, &err), "value 3: write: " + err);
      QFile o(out);
      check(o.open(QIODevice::ReadOnly | QIODevice::Text) && QString::fromUtf8(o.readLine()).trimmed().endsWith(";value 3;unit 3;status 3"),
            "value 3: the header says value 3");
    }
    // the limit test's columns at the end: the main value and value 3
    {
      const QString text = "timestamp;time (s);value;unit;status;value 3;unit 3;status 3;limit;limit 3\n"
                           "2026-01-01T00:00:00,000;0;5;V;ok;2;A;ok;PASS;FAIL LO\n"
                           "2026-01-01T00:00:01,000;1;6;V;ok;;;;FAIL HI;\n";
      const QString limitPath = tmp.path() + "/limit.csv";
      QFile lf(limitPath);
      check(lf.open(QIODevice::WriteOnly | QIODevice::Text), "limit file opens");
      QTextStream(&lf) << text;
      lf.close();
      const auto lim = RecordingFile::read(limitPath, &err);
      check(lim && lim->extra.size() == 1 && lim->extra[0].id == 2 && lim->limits.size() == 2
            && lim->limits[0] == SampleFlag::LimitPass && lim->limits[1] == SampleFlag::LimitHigh
            && lim->extra[0].limits.value(0) == SampleFlag::LimitLow && lim->extra[0].limits.value(1) == 0,
            "limit: the verdicts read, by value: " + err);
      if (lim)
      {
        const QString out = tmp.path() + "/limit_out.csv";
        check(RecordingFile::write(*lim, out, &err), "limit: write: " + err);
        QFile o(out);
        check(o.open(QIODevice::ReadOnly | QIODevice::Text) && QString::fromUtf8(o.readAll()) == text.section('\n', 0, 0) + '\n'
              + "2026-01-01T00:00:00,000;0;5;V;ok;2;A;ok;PASS;FAIL LO\n2026-01-01T00:00:01,000;1;6;V;ok;;;;FAIL HI;\n",
              "limit: written as read");
      }
      Recording none;
      none.values = { 1.0 };
      none.unit = "V";
      none.start = QDateTime(QDate(2026, 1, 1), QTime(0, 0));
      none.limits = { 0u };
      const QString out = tmp.path() + "/nolimit.csv";
      QFile o(out);
      check(RecordingFile::write(none, out, &err) && o.open(QIODevice::ReadOnly | QIODevice::Text)
            && QString::fromUtf8(o.readLine()).trimmed() == "timestamp;time (s);value;unit;status",
            "limit: no verdict, no column");
    }
    const auto old = RecordingFile::read(writeCsv("old.csv", "2026-01-01T00:00:00,000;0;nan;V\n2026-01-01T00:00:01,000;1;2;V\n"), &err);
    check(old && old->stateAt(0) == CellState::Gap && old->stateAt(1) == CellState::Ok && old->extra.isEmpty(),
          "status: an old file, nan a gap");
    for (const char *bad : { "2026-01-01T00:00:00,000;0;1;V;OL\n", "2026-01-01T00:00:00,000;0;nan;V;ok\n",
                             "2026-01-01T00:00:00,000;0;1;V;maybe\n", "2026-01-01T00:00:00,000;0;;;\n" })
    {
      const QString p = tmp.path() + "/bad_status.csv";
      QFile b(p);
      check(b.open(QIODevice::WriteOnly | QIODevice::Text), "bad status file opens");
      QTextStream(&b) << "timestamp;time (s);value;unit;status\n" << bad;
      b.close();
      check(!RecordingFile::read(p, &err), QString("status: rejected: %1").arg(bad));
    }
  }

  // --- 6. errors: missing file, header only, garbage, nothing to write ---
  {
    QString err;
    check(!RecordingFile::read(tmp.path() + "/nope.csv", &err) && !err.isEmpty(), "missing file is an error");
    check(!RecordingFile::read(writeCsv("empty.csv", ""), &err) && err.contains("header"), "header only: " + err);
    check(!RecordingFile::read(writeCsv("bad.csv", "this is not a recording\n"), &err) && !err.isEmpty(), "garbage rejected");
    check(!RecordingFile::write(Recording(), tmp.path() + "/none.csv", &err) && !err.isEmpty(), "empty recording is not written");
  }

  // --- 6. writeAny: a spreadsheet by suffix, the CSV otherwise ---
  {
    Recording rec;
    rec.start = QDateTime(QDate(2026, 9, 21), QTime(14, 3, 5, 250));
    rec.sampleTimeTenths = 5;
    rec.unit = "V";
    rec.values = {1.0, 2.0, 3.0};
    QString err;
    check(RecordingFile::writeAny(rec, tmp.filePath("a.xlsx"), &err), "xlsx via writeAny: " + err);
    check(RecordingFile::writeAny(rec, tmp.filePath("a.ods"), &err), "ods via writeAny: " + err);
    check(RecordingFile::writeAny(rec, tmp.filePath("a.csv"), &err), "csv via writeAny: " + err);
    check(QFileInfo(tmp.filePath("a.xlsx")).size() > 1000 && QFile(tmp.filePath("a.xlsx")).open(QIODevice::ReadOnly), "xlsx exists");
    QFile z(tmp.filePath("a.xlsx"));
    check(z.open(QIODevice::ReadOnly), "xlsx opens");
    check(z.read(2) == "PK", "xlsx is a zip");
    QFile c(tmp.filePath("a.csv"));
    check(c.open(QIODevice::ReadOnly | QIODevice::Text), "csv opens");
    check(QString::fromUtf8(c.readLine()).startsWith("timestamp;"), "csv is the csv");
    Recording empty;
    check(!RecordingFile::writeAny(empty, tmp.filePath("e.ods"), &err) && !err.isEmpty(), "empty refuses");
  }

  // --- QtDMM recording: the CSV in a ZIP with meta.json, read back the same ---
  {
    auto rec = RecordingFile::read(dataDir + "/new_larger.csv");
    check(rec.has_value(), "fixture for the QtDMM recording");
    if (rec)
    {
      rec->meta["marks"] = QJsonArray{ QJsonObject{ { "t", 1500.0 }, { "color", "#ffff0000" }, { "name", "Trigger" } } };
      const QString path = tmp.filePath("probe.qtdmm-rec");
      QString err;
      check(RecordingFile::writeAny(*rec, path, &err), "QtDMM recording written: " + err);
      QFile file(path);
      check(file.open(QIODevice::ReadOnly) && file.read(4) == QByteArray("PK\x03\x04", 4), "a ZIP");
      file.close();
      // the same as the plain CSV of it
      check(RecordingFile::write(*rec, tmp.filePath("probe.csv")), "the CSV of it");
      const auto plain = RecordingFile::read(tmp.filePath("probe.csv"));
      const auto back = RecordingFile::read(path, &err);
      check(back && plain && back->values == plain->values && back->start == plain->start && back->unit == plain->unit
              && back->times == plain->times,
            "values, times, start and unit as from the CSV: " + err);
      check(back && back->meta.value("format").toInt() == 1
              && back->meta.value("marks").toArray().first().toObject().value("name").toString() == "Trigger",
            "meta.json comes back with the marks");
      // a ZIP without the CSV is no recording
      ZipWriter zip;
      zip.add("other.txt", "x", false);
      QFile bad(tmp.filePath("bad.qtdmm-rec"));
      check(bad.open(QIODevice::WriteOnly) && bad.write(zip.finish()) > 0, "bad file written");
      bad.close();
      check(!RecordingFile::read(bad.fileName(), &err) && !err.isEmpty(), "a ZIP without recording.csv: an error");
    }
  }

  if (failed == 0)
    qInfo() << "All recording file tests passed.";
  else
    qWarning() << failed << "recording file test(s) failed.";
  return failed == 0 ? 0 : 1;
}
