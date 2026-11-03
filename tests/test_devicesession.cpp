// Copyright (c) 2026 The QtDMM developers
// SPDX-License-Identifier: GPL-3.0-or-later
//
// DeviceSession: meters of "My devices" opened in the process, on one SCPI
// server; their recordings follow the one that leads (the first, called the
// window's meter here). Simulated meters only, no hardware.

#include <QCoreApplication>
#include <QDateTime>
#include <QFile>
#include <QTemporaryDir>

#include "core/alarm.h"
#include "core/devicelibrary.h"
#include "core/limittest.h"
#include "core/reading.h"
#include "core/readingadapter.h"
#include "core/sampletypes.h"
#include "recording/recordingstore.h"
#include "service/devicesession.h"
#include "service/metercontroller.h"
#include "service/scpiserver.h"
#include "service/servicehost.h"
#include "service/valuefeed.h"
#include "testutil.h"

int main(int argc, char **argv)
{
  QCoreApplication app(argc, argv);
  QTemporaryDir dir;
  DeviceLibrary lib(dir.path());
  const QString sim = lib.add("Bench sim", {{"DMM/model", "QtDMM Simulated meter"}, {"Port settings/device", "calc V/DC 1"},
                                            {"DMM/virtual-waveform", 0}, {"DMM/virtual-min", "1.5"}, {"DMM/virtual-max", "1.5"}});
  const QString calc = lib.add("Power", {{"DMM/model", "QtDMM Calculated value"}, {"Port settings/device", "calc W bench_sim * 2"},
                                         {"DMM/calc-unit", "W"}, {"DMM/calc-expression", "bench_sim * 2"}});

  DeviceSession session;
  session.setInstanceId("test");
  session.setConfigDir(dir.path());
  // the first meter: it leads the recordings; disconnected, its readings
  // come from the test
  const QString w1 = lib.add("Window meter", {{"DMM/model", "QtDMM Simulated meter"}, {"Port settings/device", "calc V/DC 1"},
                                              {"DMM/virtual-waveform", 0}, {"DMM/virtual-min", "1"}, {"DMM/virtual-max", "1"}});
  MeterController *first = session.open(w1);
  if (!first)
    return 1;
  MeterController &window = *first;
  window.connectMeter(false);
  auto ask = [&](const char *line) { return QString::fromLatin1(session.host()->scpi()->process(line)).trimmed(); };

  // 1. open from My devices: connected, readings with its source
  QString error;
  MeterController *ctl = session.open(sim, &error);
  check(ctl && error.isEmpty(), "open the simulated meter: " + error);
  if (!ctl)
    return 1;
  Reading last;
  bool got = false;
  QObject::connect(ctl, &MeterController::reading, [&last, &got](const Reading &r) { last = r; got = true; });
  check(waitFor([&] { return got; }, 4000), "a reading of the simulated meter");
  check(last.source == "device:" + sim && qAbs(last.value - 1.5) < 1e-9,
        QString("reading %1 from %2").arg(last.value).arg(last.source));
  check(session.controllers().size() == 2 && session.controllers().first() == &window, "the window's meter first");
  check(session.controller("device:" + sim) == ctl, "found by source");
  check(ask("INST:CAT?") == "\"WINDOW_METER\",\"BENCH_SIM\"", "both meters on one server: " + ask("INST:CAT?"));

  // 2. refused: twice, unknown
  check(!session.open(sim, &error) && error.startsWith("-221,"), "open twice: " + error);
  check(!session.open("nosuch", &error) && error.startsWith("-224,"), "unknown id: " + error);
  // 3. SCPI: catalog, open, close
  const QString cat = ask("QTDMM:DEV:CAT?");
  check(cat.contains(QString("\"%1\",\"Bench sim\",1").arg(sim)) && cat.contains(QString("\"%1\",\"Power\",0").arg(calc)),
        "QTDMM:DEV:CAT?: " + cat);
  ask(QString("QTDMM:DEV:OPEN \"%1\"").arg(calc).toLatin1());
  check(ask("SYST:ERR?").startsWith("0,") && session.controller("device:" + calc), "QTDMM:DEV:OPEN");
  // the calculated value reads the simulated meter of this process by its name
  if (MeterController *power = session.controller("device:" + calc))
  {
    double value = 0;
    QObject::connect(power, &MeterController::reading, [&value](const Reading &r) { value = r.value; });
    check(waitFor([&] { return qAbs(value - 3.0) < 1e-9; }, 4000), QString("bench_sim * 2 = 3: %1").arg(value));
  }
  // the meters of the process only, by their names
  {
    const QMap<QString, CalcDevice::Input> in = session.calcInputs()();
    check(in.contains("bench_sim") && qAbs(in.value("bench_sim").value - 1.5) < 1e-9 && !in.contains("test"),
          QString("the meters of the process: bench_sim %1").arg(in.value("bench_sim").value));
  }
  check(DeviceSession::variableName("UT61E Bench") == "ut61e_bench" && DeviceSession::variableName("34465A") == "_34465a",
        "variable names");
  check(ask("INST:CAT?") == "\"WINDOW_METER\",\"BENCH_SIM\",\"POWER\"", "three meters: " + ask("INST:CAT?"));
  // one that does not connect: the SCPI error in English
  const QString broken = lib.add("Broken", {{"DMM/model", "QtDMM Calculated value"}, {"Port settings/device", "calc W"},
                                            {"DMM/calc-unit", "W"}, {"DMM/calc-expression", ""}});
  QString why;
  check(!session.open(broken, &why) && why == "-240,Hardware error; Broken: does not connect", "does not connect: " + why);
  ask("QTDMM:DEV:OPEN \"nosuch\"");
  check(ask("SYST:ERR?").startsWith("-224,"), "QTDMM:DEV:OPEN of an unknown id");
  // a second of the same name is POWER_2, and stays so when the first closes
  const QString twin = lib.add("power", {{"DMM/model", "QtDMM Simulated meter"}, {"Port settings/device", "calc V/DC 7"},
                                         {"DMM/virtual-waveform", 0}, {"DMM/virtual-min", "7"}, {"DMM/virtual-max", "7"}});
  check(session.open(twin) && ask("INST:CAT?").endsWith("\"POWER\",\"POWER_2\""), "the same name twice: " + ask("INST:CAT?"));
  // the meter chosen closed: none chosen, not silently another one
  ask("INST POWER");
  ask(QString("QTDMM:DEV:CLOS \"%1\"").arg(calc).toLatin1());
  check(!session.controller("device:" + calc) && ask("INST:CAT?") == "\"WINDOW_METER\",\"BENCH_SIM\",\"POWER_2\"",
        "QTDMM:DEV:CLOS: " + ask("INST:CAT?"));
  check(ask("INST?") == "\"\"" && ask("MEAS?") == "9.91E+37", "the meter chosen closed: none: " + ask("INST?"));
  ask("*CLS");
  ask("INIT");
  check(ask("SYST:ERR?").startsWith("-241,"), "the meter chosen closed: INIT -241");
  session.close(twin);

  // 4. recording: the window's starts and stops the meter here, on its time
  // base; INIT and ABOR on the meter here start and stop the window's (as
  // its Record button: the window's start there)
  RecordingStore *lead = window.recorder();
  RecordingStore *mine = ctl->recorder();
  session.setRecordingRequests([lead](bool start)
  {
    if (start)
      lead->start();
    else
      lead->stop();
  });
  auto settle = [] { for (int i = 0; i < 3; ++i) QCoreApplication::processEvents(); };
  lead->start();
  check(mine->isRunning() && mine->startClock() == lead->startClock() && mine->startDateTime() == lead->startDateTime(),
        "the window's start starts the meter here, at the same moment");
  lead->stop();
  check(mine->state() == RecordingStore::View, "the window's stop stops it");
  ask("INST BENCH_SIM");
  ask("*CLS");
  ask("INIT");
  settle();
  check(lead->isRunning() && mine->isRunning() && ask("SYST:ERR?").startsWith("0,"), "INIT here starts the window's");
  check(waitFor([&] { return mine->count() >= 2; }, 4000), QString("readings recorded: %1").arg(mine->count()));
  check(mine->unit() == "V", "the unit of the meter here: " + mine->unit());
  ask("ABOR");
  settle();
  check(!lead->isRunning() && mine->state() == RecordingStore::View && mine->dirty(), "ABOR here stops the window's");
  // saved into the folder for recordings, by name only, nothing overwritten
  ctl->setSaveDir(dir.path() + "/rec");
  ask("QTDMM:REC:SAVE \"bench\"");
  check(ask("SYST:ERR?").startsWith("0,") && QFile::exists(dir.path() + "/rec/bench.csv") && !mine->dirty(),
        "QTDMM:REC:SAVE");
  ask("QTDMM:REC:SAVE \"bench.csv\",RAW");
  check(ask("SYST:ERR?").startsWith("-257,"), "QTDMM:REC:SAVE: no file overwritten");
  ask("QTDMM:REC:SAVE \"../bench\"");
  check(ask("SYST:ERR?").startsWith("-257,"), "QTDMM:REC:SAVE: a file name only");
  ask("QTDMM:REC:SAVE \"raw.csv\", raw");
  check(ask("SYST:ERR?").startsWith("0,") && QFile::exists(dir.path() + "/rec/raw.csv"), "QTDMM:REC:SAVE ..., RAW");
  ask("QTDMM:REC:SAVE plain,RAW");
  check(ask("SYST:ERR?").startsWith("0,") && QFile::exists(dir.path() + "/rec/plain.csv"), "QTDMM:REC:SAVE without quotes, RAW");
  // a name every system takes as a file name in that folder, of a format QtDMM writes
  for (const char *name : { "x:y", "C:x", "con", "NUL.csv", "w.exe", "a,b", "end." })
  {
    ask(QString("QTDMM:REC:SAVE \"%1\"").arg(name).toLatin1());
    check(ask("SYST:ERR?").startsWith("-257,"), QString("QTDMM:REC:SAVE \"%1\": -257").arg(name));
  }
  // one not saved is overwritten, with a message
  QString replaced;
  {
    QObject::connect(&session, &DeviceSession::error, &session, [&replaced](const QString &text) { replaced = text; });
    lead->start();
    check(replaced.isEmpty(), "saved: no message at the next start: " + replaced);
    lead->stop();
    lead->start();
    check(mine->isRunning() && replaced.contains("Bench sim"), "not saved: replaced, said so: " + replaced);
    lead->stop();
  }
  // the window Live: the meter here too, so that a start reaching back
  // (the pre-trigger) has its readings
  lead->live();
  check(mine->state() == RecordingStore::Live, "the window Live: the meter here too");
  check(waitFor([&] { return mine->count() >= 2; }, 4000), "Live: readings");
  lead->startAt(Sample::now() - 2000, QDateTime::currentDateTime().addMSecs(-2000));
  check(mine->isRunning() && mine->count() >= 2 && mine->series().last().t >= 1500,
        QString("started 2 s back: its Live readings, the last at %1 ms").arg(mine->count() ? mine->series().last().t : -1));
  lead->stop();
  // opened while the window records: it joins on the window's time base
  lead->start();
  ask(QString("QTDMM:DEV:OPEN \"%1\"").arg(calc).toLatin1());
  if (MeterController *power = session.controller("device:" + calc))
  {
    check(power->recorder()->isRunning() && power->recorder()->startClock() == lead->startClock(), "opened while recording: joins");
    session.close(calc);
  }
  lead->stop();

  // 5. closed: the window's meter alone
  session.close(sim);
  check(!session.controller("device:" + sim), "closed");
  check(session.controllers().size() == 1, "only the window's meter left");
  // the window alone: its time begins with its first reading, as before
  {
    lead->start();
    waitFor([] { return false; }, 50);
    ReadingAdapter adapter;
    Reading r = ReadingAdapter::reading(adapter.adaptValue(1, "1.000", "V", "DC", "AUTO", false, true, false, 0,
                                                           QDateTime::currentMSecsSinceEpoch()));
    lead->setReading(r);
    check(lead->count() == 1 && lead->series().first().t == 0 && lead->startClock() == r.t,
          QString("the window alone: from its first reading, %1 ms").arg(lead->count() ? lead->series().first().t : -1));
    lead->stop();
  }

  // 6. no window (a later program without one): INIT on one of the meters
  // starts all of the process at one moment, ABOR stops them
  {
    const QString two = lib.add("Bench two", {{"DMM/model", "QtDMM Simulated meter"}, {"Port settings/device", "calc V/DC 2"},
                                              {"DMM/virtual-waveform", 0}, {"DMM/virtual-min", "2"}, {"DMM/virtual-max", "2"}});
    DeviceSession alone;
    alone.setInstanceId("alone");
    alone.setConfigDir(dir.path());
    MeterController *a = alone.open(sim);
    MeterController *b = alone.open(two);
    check(a && b && alone.controllers().size() == 2, "no window: two meters");
    if (a && b)
    {
      auto tell = [&](const char *line) { return QString::fromLatin1(alone.host()->scpi()->process(line)).trimmed(); };
      tell("INST BENCH_TWO");
      tell("INIT");
      settle();
      check(a->recorder()->isRunning() && b->recorder()->isRunning()
              && a->recorder()->startClock() == b->recorder()->startClock(),
            "no window: INIT starts both at one moment");
      tell("ABOR");
      settle();
      check(a->recorder()->state() == RecordingStore::View && b->recorder()->state() == RecordingStore::View,
            "no window: ABOR stops both");
    }
  }

  // 9. a view's value of a meter open beside the window (ValueFeed): its
  //    readings; closed, "no value" and the value kept; open again, back
  {
    if (!session.controller("device:" + sim))
      session.open(sim);
    ValueFeed feed(&session);
    QList<Reading> got;
    int changes = 0;
    QObject::connect(&feed, &ValueFeed::reading, [&got](const Reading &r) { got << r; });
    QObject::connect(&feed, &ValueFeed::controllerChanged, [&changes](MeterController *) { ++changes; });
    feed.setValue(ValueRef(0));
    check(!feed.controller(), "an empty source: no meter");
    feed.setValue(ValueRef(0, "device:" + sim));
    check(feed.controller() == session.controller("device:" + sim) && changes == 2, "another meter's value");
    check(waitFor([&] { return !got.isEmpty() && got.last().source == "device:" + sim && !got.last().noNumber(); }, 4000),
          "its readings come");
    const QString name = lib.find(sim)->name;
    check(feed.caption() == name && feed.notice().isEmpty(),
          "it says where the value comes from, no notice: " + feed.caption() + " / " + feed.notice());
    // renamed in My devices: the open meter and the view take the name
    lib.rename(sim, "Renamed");
    check(waitFor([&] { return feed.caption() == "Renamed"; }, 2000)
            && session.controller("device:" + sim)->deviceName() == "Renamed",
          "renamed: " + feed.caption());
    lib.rename(sim, name);
    waitFor([&] { return feed.caption() == name; }, 2000);
    got.clear();
    session.close(sim);
    check(!feed.controller() && !got.isEmpty() && got.last().quality == Quality::NoValue
            && feed.value() == ValueRef(0, "device:" + sim),
          "closed: no value, the value kept");
    check(feed.caption() == name && feed.notice() == "Not connected",
          "closed: its name from My devices, not connected: " + feed.caption() + " / " + feed.notice());
    got.clear();
    session.open(sim);
    check(feed.controller() && !feed.notice().isEmpty(), "open again: a notice until its value comes: " + feed.notice());
    check(feed.controller() && waitFor([&] { return !got.isEmpty() && !got.last().noNumber(); }, 4000),
          "open again: its readings again");
    check(feed.notice().isEmpty(), "and then no notice: " + feed.notice());
  }

  // 10. rules with the entry: set when opened, a change (also over SCPI)
  //     goes back into it; the meter keys and their tidying leave them
  {
    session.close(sim);
    Alarm high;
    high.name = "High";
    high.condition = Alarm::Above;
    high.a = 2;
    LimitTest test;
    test.enabled = true;
    test.form = LimitTest::Range;
    test.lower = 1;
    test.upper = 2;
    check(lib.setRules(sim, Alarm::listToJson({ high }), test.toJsonText()), "rules into the entry");
    MeterController *b = session.open(sim);
    check(b && b->alarms()->alarms().size() == 1 && b->alarms()->alarms().first().name == "High"
            && b->limitTest().enabled && b->limitTest().upper == 2,
          "opened with the entry's rules");
    if (b)
    {
      Alarm low = high;
      low.name = "Low";
      low.condition = Alarm::Below;
      b->setAlarms({ high, low });
      check(Alarm::listFromJson(lib.find(sim)->alarms).size() == 2, "a new alarm list into the entry");
      ask("INST BENCH_SIM");
      ask("CALC:LIM:UPP 3");
      check(LimitTest::fromJsonText(lib.find(sim)->limitTest).upper == 3,
            "a limit set over SCPI into the entry: " + lib.find(sim)->limitTest);
    }
    QVariantMap keys = lib.find(sim)->keys;
    keys.insert("DMM/virtual-max", "1.6");
    lib.update(sim, keys);
    check(!lib.find(sim)->alarms.isEmpty() && !lib.find(sim)->limitTest.isEmpty() && !lib.find(sim)->keys.contains("Alarms/list"),
          "update() of the meter keys keeps the rules, apart from them");
    DeviceLibrary again(dir.path());   // tidies the file
    check(Alarm::listFromJson(again.find(sim)->alarms).size() == 2, "tidying keeps them");
    const QString copy = lib.duplicate(sim);
    check(Alarm::listFromJson(lib.find(copy)->alarms).size() == 2, "a copy has them too");
    lib.remove(copy);
  }

  // 11. a recording from a graph takes only the meters of its values, also
  //     one opened while it runs; the next takes all again. SCPI SAVE at
  //     the window writes the meters that recorded with it
  {
    MeterController *b = session.controller("device:" + sim);
    if (!b)
      b = session.open(sim);
    MeterController *power = session.open(calc);
    check(b && power, "both open");
    if (!b || !power)
      return 1;
    RecordingStore *lead = window.recorder();
    lead->live();
    session.setRecordingScope({ b->source() });
    lead->start();
    check(b->recorder()->isRunning() && power->recorder()->state() == RecordingStore::Live,
          "a graph's recording: only the meters of its values, the others stay Live");
    lead->stop();
    lead->start();
    check(b->recorder()->isRunning() && power->recorder()->isRunning(), "the next recording: all again");
    lead->stop();
    session.close(calc);
    session.setRecordingScope({ b->source() });
    lead->start();
    power = session.open(calc);
    check(power && !power->recorder()->isRunning(), "opened while it runs, no value of it in the graph: stays out");
    lead->stop();
    session.close(calc);
    session.setRecordingScope({ b->source(), "device:" + calc });
    lead->start();
    power = session.open(calc);
    check(power && power->recorder()->isRunning(), "opened while it runs, a value of it in the graph: joins");

    // the window's recording saved over SCPI: with theirs
    static ReadingAdapter adapter;
    Reading r = ReadingAdapter::reading(adapter.adaptValue(1.0, "1.000", "V", "DC", "AUTO", false, true, false, 0,
                                                           QDateTime::currentMSecsSinceEpoch()));
    r.t = Sample::now();
    lead->setReading(r);
    check(waitFor([&] { return b->recorder()->count() >= 2 && power->recorder()->count() >= 1; }, 4000), "readings recorded");
    lead->stop();
    window.setSaveDir(dir.path() + "/rec");
    check(window.saveRecording("together", false).isEmpty(), "QTDMM:REC:SAVE at the window");
    QFile f(dir.path() + "/rec/together.csv");
    const QString head = f.open(QIODevice::ReadOnly) ? QString::fromUtf8(f.readLine()).trimmed() : QString();
    check(head.contains(";Bench sim: value;Bench sim: unit;Bench sim: status") && head.contains(";Power: value;")
            && !b->recorder()->dirty() && !power->recorder()->dirty(),
          "with the meters that recorded with it, saved too: " + head);
    session.close(calc);
  }

  // 12. the session's recording: the meter of the trigger value leads and
  //     starts the window's too; a recording keeps its lead; closed while it
  //     leads, the recording ends for all and the window's leads again
  {
    MeterController *b = session.controller("device:" + sim);
    if (!b)
      b = session.open(sim);
    check(b != nullptr, "Bench sim open");
    if (!b)
      return 1;
    RecordingStore *lead = window.recorder();
    lead->live();
    RecordingStore::Setup setup;
    setup.sampleTime = 5;
    setup.trigger = ValueRef(0, b->source());
    session.setRecordingSetup(setup);
    check(session.lead() == b && b->recorder()->sampleTime() == 5 && lead->sampleTime() == 5,
          "the trigger value's meter leads; the sample time for all");
    session.startRecording();
    check(b->recorder()->isRunning() && lead->isRunning() && lead->startClock() == b->recorder()->startClock()
            && session.isRecording(),
          "its start starts the window's, on one time base");
    setup.trigger = ValueRef(0);
    session.setRecordingSetup(setup);
    check(session.lead() == b, "a recording keeps its lead while the trigger value changes");
    session.close(sim);
    check(!lead->isRunning() && !session.isRecording() && session.lead() == &window,
          "the lead closed: the recording ends for all, the window's leads again");
    session.startRecording();
    check(lead->isRunning(), "the window's leads its own");
    session.stopRecording();
    check(lead->state() == RecordingStore::View, "stopped");
  }

  // the session deleted with meters open: they go first, and what follows
  // their end (AlarmService) finds the session without them, not freed
  {
    auto *gone = new DeviceSession;
    gone->setInstanceId("gone");
    gone->setConfigDir(dir.path());
    MeterController *one = gone->open(sim);
    QList<qsizetype> seen;
    if (one)
      QObject::connect(one, &QObject::destroyed, [&seen, gone] { seen << gone->controllers().size(); });
    delete gone;
    check(one && seen == QList<qsizetype>{ 0 }, "session deleted: its meters first, it lists none while they go");
  }

  return failed == 0 ? 0 : 1;
}
