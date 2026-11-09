// Copyright (c) 2026 The QtDMM developers
// SPDX-License-Identifier: GPL-3.0-or-later
//
// LabPlotLive: a recording that grows, followed into a CSV and a LabPlot
// project with a live data source on it. The recording comes from a lambda
// here, rows added between the updates.

#include <QtCore>

#include "recording/labplotlive.h"
#include "testutil.h"

namespace
{
QString text(const QString &path)
{
  QFile f(path);
  return f.open(QIODevice::ReadOnly) ? QString::fromUtf8(f.readAll()) : QString();
}
}

int main(int argc, char **argv)
{
  QCoreApplication app(argc, argv);
  QTemporaryDir tmp;

  Recording rec;
  rec.start = QDateTime::currentDateTime();
  rec.sampleTimeTenths = 5;
  rec.unit = "V";
  rec.device = "Sim, A";   // a comma: LabPlot splits its names there
  RecordingColumn amps;
  amps.device = "Sim B";
  amps.unit = "A";
  rec.extra << amps;
  auto add = [&rec](double v, double a)
  {
    rec.values << v;
    rec.states << (std::isnan(v) ? CellState::Gap : CellState::Ok);
    rec.extra[0].values << a;
    rec.extra[0].states << CellState::Ok;
  };
  add(1.0, 0.5);
  add(qQNaN(), 0.25);
  add(3.0, 0.125);
  bool running = true;

  LabPlotLive live([&rec] { return rec; }, [&running] { return running; });
  int stopped = 0;
  QObject::connect(&live, &LabPlotLive::stopped, [&stopped] { ++stopped; });
  const QString csv = tmp.filePath("live.csv"), project = tmp.filePath("live.lml");
  QString err;
  check(live.start(csv, project, &err) && live.isRunning(), "started: " + err);
  // the newest row held back while it records
  check(text(csv) == "#time[s];Sim A: value [V];Sim B: value [A]\n0;1;0.5\n0.5;nan;0.25\n",
        "the CSV so far: " + text(csv));
  const QString lml = text(project);
  check(lml.contains("<liveDataSource name=\"Live\"") && lml.contains("fileName=\"" + QDir::toNativeSeparators(csv))
          && lml.contains("vectorNames=\"time[s],Sim A: value [V],Sim B: value [A]\"")
          && lml.contains("columnModes=\"Double,Double,Double\"")
          && lml.contains("yColumn=\"live/Live/Sim B: value [A]\""),
        "the project: a live data source on the file");
  check(lml.count("<cartesianPlot ") == 2, "a plot per unit");

  // two more rows: the held one and one of them go in
  add(4.0, 0.0625);
  add(5.0, 0.03125);
  live.update();
  check(live.rowsWritten() == 4 && text(csv).endsWith("1;3;0.125\n1.5;4;0.0625\n"), "update appends: " + text(csv));
  check(stopped == 0, "still running");

  // a value gone: nan in its column from then on
  add(6.0, 0);
  rec.extra.clear();
  live.update();
  check(text(csv).endsWith("2;5;nan\n"), "a column gone: nan " + text(csv));

  // the recording ended: the rest goes in, stopped
  running = false;
  live.update();
  check(!live.isRunning() && stopped == 1 && text(csv).endsWith("2.5;6;nan\n"), "ended: all in, stopped " + text(csv));

  // a new recording (fewer rows than written): stops without writing
  running = true;
  check(live.start(tmp.filePath("b.csv"), tmp.filePath("b.lml"), &err), "started again: " + err);
  rec.values.resize(1);
  rec.states.resize(1);
  live.update();
  check(!live.isRunning() && stopped == 2, "another recording: stopped");

  // nothing recorded: refused
  Recording none;
  LabPlotLive empty([&none] { return none; }, [] { return true; });
  check(!empty.start(tmp.filePath("c.csv"), tmp.filePath("c.lml"), &err) && !err.isEmpty(), "nothing: refused");

  if (failed == 0)
    qInfo() << "All LabPlot live tests passed.";
  else
    qWarning() << failed << "LabPlot live test(s) failed.";
  return failed == 0 ? 0 : 1;
}
