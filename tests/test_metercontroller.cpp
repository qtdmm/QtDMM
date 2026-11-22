// Copyright (c) 2026 The QtDMM developers
// SPDX-License-Identifier: GPL-3.0-or-later
//
// MeterController behind its SCPI server, over TCP on localhost: what a
// command asks of the UI (connect, record) runs after the command, and the
// checks count what is still on its way - "INP ON;INP ON" connects once,
// "INIT;INIT" starts once and leaves -213 (review R10-05).

#include <QtCore>
#include <QtNetwork>

#include "device/meterconnection.h"
#include "service/metercontroller.h"
#include "service/scpiserver.h"
#include "testutil.h"

// lets the queued work run
static void settle(int ms = 200)
{
  waitFor([] { return false; }, ms);
}

int main(int argc, char **argv)
{
  QCoreApplication app(argc, argv);
  MeterController ctl;
  ScpiConfig config;
  config.enabled = true;
  config.port = 0;   // the system picks one: a fixed port may be in use
  ctl.applyScpi(config);
  quint16 port = 0;
  for (const ServiceState &s : ctl.services())
    if (s.id == QLatin1String("scpi"))
      port = s.port;
  check(port != 0, "the SCPI server listens");

  QList<bool> connects;
  QList<bool> recordings;
  QObject::connect(&ctl, &MeterController::connectRequested, [&](bool on) { connects << on; });
  QObject::connect(&ctl, &MeterController::recordingRequested, [&](bool start) { recordings << start; });

  QTcpSocket c;
  c.connectToHost(QHostAddress::LocalHost, port);
  check(waitFor([&] { return c.state() == QAbstractSocket::ConnectedState; }), "connected");
  auto query = [&](const QByteArray &line)
  {
    c.write(line + "\n");
    c.flush();
    if (!waitFor([&] { return c.canReadLine(); }))
      return QString();
    return QString::fromLatin1(c.readLine()).trimmed();
  };
  auto send = [&](const QByteArray &line)
  {
    c.write(line + "\n");
    c.flush();
    settle();
  };

  // nothing runs inside the command, once after it
  send("INP ON;INP ON");
  check(connects == QList<bool>{ true }, QString("INP ON;INP ON: %1 connect request(s)").arg(connects.size()));
  connects.clear();
  // no UI opened the meter: it is not wanted, OFF has nothing to do
  send("INP OFF");
  check(connects.isEmpty(), "INP OFF while not wanted: nothing");

  send("INIT;INIT");
  check(recordings == QList<bool>{ true }, QString("INIT;INIT: %1 start(s)").arg(recordings.size()));
  check(query("SYST:ERR?").startsWith("-213"), "the second INIT leaves -213");
  recordings.clear();
  // once the start has run, the next INIT is checked against the recorder again
  send("INIT");
  check(recordings == QList<bool>{ true }, "INIT after the start ran: checked anew");

  // another meter: the graph axis and the alarm page drop the unit, and the
  // next reading announces its own, even when it is the same
  QStringList units;
  QStringList baseUnits;
  QObject::connect(&ctl, &MeterController::unitChanged, [&](const QString &u) { units << u; });
  QObject::connect(&ctl, &MeterController::baseUnitChanged, [&](const QString &u) { baseUnits << u; });
  ctl.forgetUnit();
  check(units == QStringList{ QString() } && baseUnits == QStringList{ QString() }, "forgetUnit() announces no unit");
  check(ctl.unit().isEmpty() && ctl.baseUnit().isEmpty(), "forgetUnit() forgets the unit");

  // the min/max memory of each value of its own (CALCulate:AVERage (@n)):
  // the display follows the second value, the main one keeps its memory
  {
    auto *dmm = ctl.findChild<MeterConnection *>();
    auto frame = [&](double main, double second, bool hold = false)
    {
      DmmDecoder::DmmResponse r;
      r.main().dval = main;
      r.main().val = QString::number(main);
      r.main().unit = "V";
      r.main().special = "DC";
      r.hold = hold;
      DmmDecoder::DmmChannel &s = r.addChannel(1);
      s.dval = second;
      s.val = QString::number(second);
      s.unit = "Hz";
      s.special = "HZ";
      Q_EMIT dmm->response(r);
    };
    // every reading names its meter: this instance, or its "My devices" entry
    QStringList sources;
    const auto from = QObject::connect(&ctl, &MeterController::reading, [&](const Reading &rd) { sources << rd.source; });
    frame(1, 50);
    ctl.setDevice("ab12cd34", "UT61E bench");
    frame(1, 50);
    check(query("INST:CAT?") == "\"UT61E_BENCH\"", "INST:CAT? names the device: " + query("INST:CAT?"));
    ctl.setDevice(QString(), QString());
    check(query("INST:CAT?") == "\"DEFAULT\"", "INST:CAT? without a device: the instance");
    QObject::disconnect(from);
    check(sources == QStringList({ "instance:default", "instance:default", "device:ab12cd34", "device:ab12cd34" }),
          "Reading::source: " + sources.join(", "));
    send("CALC:AVER:CLE");
    // the memory of each value as a signal (the analog meter on another value)
    QMap<int, std::pair<double, double>> told;
    const auto conn = QObject::connect(&ctl, &MeterController::valueMinMaxChanged, [&](int id, double lo, double hi)
    {
      told[id] = { lo, hi };
    });
    frame(1, 50);
    frame(3, 60);
    frame(9, 99, true);   // held: no new reading
    ctl.setMinMaxValue(1);
    frame(2, 55);
    check(query("CALC:AVER:MIN? (@1:2)") == "+1.000000E+00,+5.000000E+01"
            && query("CALC:AVER:MAX? (@1,2)") == "+3.000000E+00,+6.000000E+01",
          "each value its own min/max, the held reading left out");
    check(query("CALC:AVER:COUN?") == "1" && query("CALC2:AVER:COUN?") == "3",
          "the display's memory began anew with the second value, the value's own went on");
    check(told.value(1) == std::make_pair(50.0, 60.0) && told.value(0) == std::make_pair(1.0, 3.0),
          QString("each value tells its min/max: %1..%2").arg(told.value(1).first).arg(told.value(1).second));
    send("CALC:AVER:CLE (@2)");
    check(std::isnan(told.value(1).first) && told.value(0).second == 3.0, "CLEar (@2) tells that value's memory empty");
    QObject::disconnect(conn);
    check(query("CALC:AVER:COUN? (@1:2)") == "3,0" && query("CALC:AVER:COUN?") == "1",
          "CLEar (@2): that value's own memory alone");
    send("CALC:AVER:CLE");
    check(query("CALC:AVER:COUN? (@1:2)") == "0,0", "CLEar: all of them");
  }

  // the limit test counts its verdicts, one per reading of its value; a
  // new test and the min/max reset start the count anew
  {
    auto *dmm = ctl.findChild<MeterConnection *>();
    auto frame = [&](double v)
    {
      DmmDecoder::DmmResponse r;
      r.main().dval = v;
      r.main().val = QString::number(v);
      r.main().unit = "V";
      r.main().special = "DC";
      Q_EMIT dmm->response(r);
    };
    LimitTest t;
    t.enabled = true;
    t.form = LimitTest::Range;
    t.lower = 4;
    t.upper = 6;
    ctl.setLimitTest(t);
    QList<qint64> told;
    const auto conn = QObject::connect(&ctl, &MeterController::limitCountChanged,
                                       [&](const LimitTest::Count &c) { told << c.total(); });
    for (double v : { 5.0, 7.0, 5.5, 3.0, 4.0 })
      frame(v);
    const LimitTest::Count c = ctl.limitCount();
    check(c.pass == 3 && c.high == 1 && c.low == 1 && c.fail() == 2 && c.total() == 5,
          QString("limit count: 3 PASS, 1 HI, 1 LO, got %1/%2/%3").arg(c.pass).arg(c.high).arg(c.low));
    check(told == QList<qint64>({ 1, 2, 3, 4, 5 }), "limit count: told with every verdict");
    ctl.resetMinMax();
    check(ctl.limitCount().total() == 0 && told.last() == 0, "limit count: reset with min/max");
    frame(5);
    ctl.setLimitTest(t);
    check(ctl.limitCount().total() == 0, "limit count: a new limit test starts at 0");
    QObject::disconnect(conn);
    ctl.setLimitTest(LimitTest());
    frame(5);
    check(ctl.limitCount().total() == 0, "limit test off: nothing counted");

    // the same over SCPI, as on Keysight's bench meters: CALCulate:LIMit
    // (34465A), CALCulate:FUNCtion LIMit + CALCulate:STATe (34410A)
    QList<LimitTest> changed;
    const auto watch = QObject::connect(&ctl, &MeterController::limitTestChanged,
                                        [&](const LimitTest &lt) { changed << lt; });
    LimitTest pct;
    pct.nominal = 5;
    pct.tolerance = 10;
    ctl.setLimitTest(pct);
    check(query("CALC:LIM?") == "0" && query("CALC:STAT?") == "0", "CALC:LIM? off");
    check(query("CALC:LIM:LOW?") == "+4.500000E+00" && query("CALC:LIM:UPP:DATA?") == "+5.500000E+00",
          "CALC:LIM:LOW?/UPP? of 5 V ± 10 %: " + query("CALC:LIM:LOW?"));
    send("CALC:LIM:UPP 6");
    check(ctl.limitTest().form == LimitTest::Range && ctl.limitTest().low() == 4.5 && ctl.limitTest().high() == 6,
          "an upper limit set: lower and upper limit, the lower one kept");
    send("CALCulate:LIMit:LOWer:DATA 4");
    send("CALC:LIM:STAT ON");
    check(ctl.limitTest().enabled && ctl.limitTest().low() == 4 && query("CALC:LIM:STATe?") == "1", "CALC:LIM:STAT ON");
    check(!changed.isEmpty() && changed.last().enabled, "limitTestChanged: the window keeps it");
    check(query("CALC:LIM:LOW? MIN") == "-1.000000E+15" && query("CALC:LIM:UPP? MAX") == "+1.000000E+15",
          "CALC:LIM:LOW? MIN / UPP? MAX");
    frame(7);
    check(query("QTDMM:LIM:VERD?") == "HI" && (query("STAT:QUES?").toInt() & (1 << 12)), "FAIL HI: verdict and bit 12");
    frame(3);
    check(query("QTDMM:LIM:VERD?") == "LO" && (query("STAT:QUES?").toInt() & (1 << 11)), "FAIL LO: bit 11");
    frame(5);
    check(query("QTDMM:LIMit:VERDict?") == "PASS" && !(query("STAT:QUES?").toInt() & (3 << 11)), "PASS: neither bit");
    check(query("QTDMM:LIM:COUN?") == "1,1,1", "QTDMM:LIM:COUN? " + query("QTDMM:LIM:COUN?"));
    send("CALC:LIM:CLE");
    check(query("QTDMM:LIM:COUN?") == "0,0,0", "CALC:LIM:CLE resets the count");
    send("CALC:STAT OFF");
    check(!ctl.limitTest().enabled && query("QTDMM:LIM:VERD?") == "NONE", "CALC:STAT OFF");
    check(query("CALC:FUNC?") == "LIM", "CALC:FUNC? LIM");
    send("CALC:FUNC LIM");
    send("CALC:FUNC DB");
    check(query("SYST:ERR?").startsWith("-224") && query("SYST:ERR?").startsWith("0"), "CALC:FUNC DB: -224, LIM: fine");
    send("CALC:LIM:LOW abc");
    check(query("SYST:ERR?").startsWith("-104"), "CALC:LIM:LOW abc: -104");
    send("CALC:LIM:STAT maybe");
    check(query("SYST:ERR?").startsWith("-224"), "CALC:LIM:STAT maybe: -224");
    QObject::disconnect(watch);
  }

  // a fault word ("FUSE"): said once and marked where it began; no number
  // shown ("----"): neither
  {
    auto *dmm = ctl.findChild<MeterConnection *>();
    QStringList errors, marks;
    QObject::connect(&ctl, &MeterController::error, [&](const QString &e) { errors << e; });
    QObject::connect(&ctl, &MeterController::markRequested,
                     [&](const QColor &, const QString &name, bool, bool) { marks << name; });
    auto shown = [&](const QString &text, bool fault)
    {
      DmmDecoder::DmmResponse r;
      r.main().val = text;
      r.main().unit = "mA";
      r.main().special = "DC";
      r.main().noValue = !fault;
      r.main().fault = fault ? text : QString();
      Q_EMIT dmm->response(r);
    };
    shown("FUSE", true);
    shown("FUSE", true);
    check(errors.filter("FUSE").size() == 1 && marks == QStringList{ "FUSE" },
          QString("FUSE: one message, one mark, got %1 / %2").arg(errors.join('|'), marks.join('|')));
    shown("----", false);
    check(errors.filter("----").isEmpty() && marks.size() == 1, "dashes: no message, no mark");
  }

  // what a QtDMM recording keeps of the meter: not the sounds and commands,
  // paths of this computer
  {
    MeterController m;
    m.setDevice("ab12", "Bench");
    m.setModel("UNI-T UT61E");
    Alarm al;
    al.name = "hot";
    al.sound = "/home/x/ring.wav";
    al.command = "notify-send hot";
    m.setAlarms({ al });
    LimitTest t;
    t.enabled = true;
    t.highSound = "/home/x/high.wav";
    m.setLimitTest(t);
    DmmDecoder::DmmResponse r;
    r.main().dval = 1.5;
    r.main().val = "1.5";
    r.main().unit = "V";
    r.main().special = "DC";
    Q_EMIT m.findChild<MeterConnection *>()->response(r);
    const QJsonObject info = m.recordingInfo();
    const QJsonObject v0 = info.value("values").toArray().first().toObject();
    check(v0.value("quantity") == "voltage.dc" && !v0.contains("name"),
          "recordingInfo: the quantity by its key, no name the meter did not give: "
            + QString::fromUtf8(QJsonDocument(v0).toJson(QJsonDocument::Compact)));
    const QJsonObject a = info.value("alarms").toArray().first().toObject();
    check(info.value("name") == "Bench" && info.value("model") == "UNI-T UT61E"
            && info.value("values").toArray().first().toObject().value("id").toInt() == 0,
          "recordingInfo: name, model, the main value");
    check(a.value("name") == "hot" && !a.contains("sound") && !a.contains("command"),
          "recordingInfo: the alarm without its sound and command");
    check(info.value("limit-test").toObject().value("enabled").toBool()
            && !info.value("limit-test").toObject().contains("high-sound"),
          "recordingInfo: the limit test without its sounds");
  }

  // two meters, one ServiceHost: one server, INSTrument chooses the meter
  {
    ServiceHost host;
    MeterController bench(nullptr, &host);
    auto *shunt = new MeterController(nullptr, &host);
    bench.setInstanceId("bench");
    shunt->setDevice("ab12cd34", "SmartShunt");
    auto feed = [](MeterController *m, double v, const char *unit)
    {
      DmmDecoder::DmmResponse r;
      r.main().dval = v;
      r.main().val = QString::number(v);
      r.main().unit = unit;
      r.main().special = "DC";
      Q_EMIT m->findChild<MeterConnection *>()->response(r);
    };
    auto ask = [&](const char *line) { return QString::fromLatin1(host.scpi()->process(line)).trimmed(); };
    check(ask("INST:CAT?") == "\"BENCH\",\"SMARTSHUNT\"", "one server, both meters: " + ask("INST:CAT?"));
    feed(&bench, 1.5, "V");
    feed(shunt, 12.8, "V");
    check(ask("UNIT?") == "\"V\"" && host.scpi()->selectedDevice()->name() == "bench", "the first meter at first");
    host.scpi()->process("INST SMARTSHUNT");
    check(ask("INST:NSEL?") == "2", "INST SMARTSHUNT");
    delete shunt;
    check(ask("INST:CAT?") == "\"BENCH\"" && ask("INST?") == "\"\"", "a meter that goes leaves the server, none chosen");
    check(ctl.serviceHost() != &host && bench.serviceHost() == &host, "a controller without a host has its own");
  }

  if (failed)
  {
    qWarning() << failed << "MeterController check(s) failed";
    return 1;
  }
  qInfo() << "All MeterController tests passed.";
  return 0;
}
