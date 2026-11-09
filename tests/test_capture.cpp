// Copyright (c) 2026 The QtDMM developers
// SPDX-License-Identifier: GPL-3.0-or-later
//
// The capture of a meter's frames: the list keeps the last ones, a frame kept
// with what the meter showed, the YAML in the form of the spec vectors with
// no port or address in it, and the window fed by MeterConnection::frame().
#include <QApplication>
#include <QCheckBox>
#include <QFile>
#include <QGroupBox>
#include <QLabel>
#include <QSet>
#include <QLineEdit>
#include <QPushButton>
#include <QTemporaryDir>
#include <QTreeWidget>

#include "device/capturelog.h"
#include "device/capturesteps.h"
#include "device/meterconnection.h"
#include "ui/dialogs/capturedlg.h"
#include "testutil.h"

namespace
{
// a Fluke 18x line: -121.43 V DC (docs/protocols/spec/fluke_qm.yaml)
// a decoded value as the decoders deliver it
CaptureLog::Frame decoded(const QString &val, const QString &unit, const QString &special, qint64 wall = 0)
{
  DmmDecoder::DmmResponse r;
  r.main().val = val;
  r.main().unit = unit;
  r.main().special = special;
  r.range = "AUTO";
  return CaptureLog::Frame{ wall, "x", r };
}

const QByteArray kDcVolt = QByteArray::fromHex("300D514D2C2D3132312E3433205644430D");

// feeds @p bytes through the connection as the frame reader does
void feed(MeterConnection &dmm, const QByteArray &bytes, qint64 wall)
{
  QMetaObject::invokeMethod(&dmm, "readEventSLOT", Q_ARG(QByteArray, bytes), Q_ARG(int, 0), Q_ARG(qint64, 1),
                            Q_ARG(qint64, wall));
}
}

int main(int argc, char **argv)
{
  if (qEnvironmentVariableIsEmpty("QT_QPA_PLATFORM"))
    qputenv("QT_QPA_PLATFORM", "offscreen");
  QApplication app(argc, argv);

  // 1. the list keeps the last frames; kept ones stay when it is cleared
  {
    CaptureLog log;
    for (int i = 0; i < CaptureLog::kFrames + 20; ++i)
      log.add(CaptureLog::Frame{ i, QByteArray(1, char(i)), std::nullopt });
    check(log.frames().size() == CaptureLog::kFrames && log.frames().first().wall == 20, "the oldest frames go");
    check(log.keep("  1.5   V ") && log.kept().last().shown == "1.5 V" && log.kept().last().frame.wall == 519,
          "the newest is kept by default");
    check(log.keep("x", 0) && log.kept().last().frame.wall == 20, "a frame chosen is kept");
    check(!log.keep("x", CaptureLog::kFrames), "no frame beyond the list");
    log.removeKept(1);
    log.clear();
    check(log.kept().size() == 1 && log.frames().isEmpty(), "clearing keeps the kept");
    check(!log.keep("x"), "nothing to keep in an empty list");
  }

  // 1b. a frame of the second value says which
  {
    CaptureLog log;
    DmmDecoder::DmmResponse r;
    r.main().id = 2;
    r.main().val = "43.10";
    r.main().unit = "%RH";
    log.add(CaptureLog::Frame{ 0, "41", r });
    log.keep("43.1 %RH");
    check(log.yaml({}, false).contains("    expect: {id: 2}\n"), log.yaml({}, false));
  }

  // 2. names
  {
    const QDateTime at(QDate(2026, 11, 9), QTime(14, 3));
    check(CaptureLog::fileName("UNI-T", "UT61E", at) == "qtdmm-capture-uni-t-ut61e-2026-11-09-1403.yaml",
          CaptureLog::fileName("UNI-T", "UT61E", at));
    check(CaptureLog::fileName("", "", at) == "qtdmm-capture-meter-2026-11-09-1403.yaml", "a meter without name");
    check(CaptureLog::connectionName(Transport::PortType::BleGatt) == "Bluetooth LE", "Bluetooth in a word");
  }

  // 3. through the connection and the window: decoded, not understood, the YAML
  MeterConnection dmm(nullptr);
  DmmDecoder::DMMInfo info;
  info.vendor = "Fluke";
  info.model = "189";
  dmm.setDmmInfo(info);
  // a port with a key in it: neither may reach the file
  dmm.setDevice("ble AA:BB:CC:DD:EE:FF 0123456789abcdef0123456789abcdef");
  dmm.setFormat(FrameFormat::FlukeQM);
  {
    CaptureDlg dlg(&dmm, "My Fluke", nullptr);
    auto *frames = dlg.findChild<QTreeWidget *>("ui_captureFrames");
    auto *kept = dlg.findChild<QTreeWidget *>("ui_captureKept");
    auto *shown = dlg.findChild<QLineEdit *>("ui_captureShown");
    auto *keep = dlg.findChild<QPushButton *>("ui_captureKeep");
    auto *pause = dlg.findChild<QPushButton *>("ui_capturePause");
    auto *all = dlg.findChild<QCheckBox *>("ui_captureAllFrames");
    check(frames && kept && shown && keep && pause && all, "the window's parts");

    feed(dmm, kDcVolt, 1000);
    feed(dmm, "garbage", 2000);
    check(frames->topLevelItemCount() == 2, "two frames listed");
    check(frames->topLevelItem(0)->text(2).startsWith("-121.43 V DC"), frames->topLevelItem(0)->text(2));
    check(frames->topLevelItem(1)->text(2) == "not understood", frames->topLevelItem(1)->text(2));
    check(frames->topLevelItem(0)->text(1) == "30 0D 51 4D 2C 2D 31 32 31 2E 34 33 20 56 44 43 0D", "bytes as hex");

    check(!keep->isEnabled(), "nothing to keep without what the meter shows");
    frames->setCurrentItem(frames->topLevelItem(0));
    shown->setText("-121.43 V DC");
    check(keep->isEnabled(), "keep with what it shows");
    keep->click();
    check(kept->topLevelItemCount() == 1 && kept->topLevelItem(0)->text(0) == "-121.43 V DC" && shown->text().isEmpty(),
          "kept, the field cleared");
    frames->clearSelection();
    shown->setText("Err");
    keep->click();
    check(dlg.log().kept().last().frame.bytes == "garbage", "without a selection the newest");

    pause->click();
    feed(dmm, kDcVolt, 3000);
    check(frames->topLevelItemCount() == 2, "paused: no frame added");
    pause->click();

    const QString yaml = dlg.yaml();
    check(yaml.contains("protocol: FlukeQM\n") && yaml.contains("devices: [\"Fluke 189\"]")
            && yaml.contains("over Bluetooth LE"),
          "the head");
    check(yaml.contains("  - source_line: \"-121.43 V DC\"\n    bytes: 30 0D 51 4D 2C 2D 31 32 31 2E 34 33 20 56 44 43 0D\n"
                        "    reading: {value: \"-121.43\", unit: \"V\", range_mode: \"auto\"}\n"
                        "    expect: {special: \"DC\"}\n"),
          yaml);
    check(yaml.contains("  - source_line: \"Err\"\n    bytes: 67 61 72 62 61 67 65\n    # QtDMM did not understand"), "the frame not understood");
    check(!yaml.contains("frames:"), "no list unless asked");
    for (const char *secret : { "AA:BB", "aa:bb", "0123456789", "My Fluke" })
      check(!yaml.contains(secret), QString("no %1 in the file").arg(secret));
    all->setChecked(true);
    check(dlg.yaml().contains("frames:\n  - {t: \"") && dlg.yaml().contains("# not understood"), "the list asked for");

    QTemporaryDir dir;
    const QString file = dir.filePath("c.yaml");
    QString error;
    check(dlg.write(file, &error), error);
    QFile in(file);
    check(in.open(QIODevice::ReadOnly) && in.readAll().startsWith("# A capture of QtDMM"), "written");
  }

  // 4. no copy of the frames while nobody looks
  {
    int seen = 0;
    {
      CaptureDlg dlg(&dmm, "x", nullptr);
      feed(dmm, kDcVolt, 4000);
      seen = int(dlg.log().frames().size());
    }
    check(seen == 1, "the window sees the frame");
  }

  // 5. the steps of a kind of meter
  {
    const QList<CaptureSteps::Step> dmmSteps = CaptureSteps::forType(DeviceType::HandheldDmm);
    check(!dmmSteps.isEmpty() && dmmSteps.first().id == "dc_v", "a multimeter starts with DC V");
    for (DeviceType::Type t : { DeviceType::HandheldDmm, DeviceType::ClampMeter, DeviceType::Thermometer,
                                DeviceType::Luxmeter, DeviceType::SoundLevel })
    {
      QSet<QString> ids;
      for (const CaptureSteps::Step &st : CaptureSteps::forType(t))
      {
        check(st.instruction && !ids.contains(st.id), QString("step %1 once, with an instruction").arg(st.id));
        check(st.key.isEmpty() || DmmDecoder::keyNames().contains(st.key), QString("key %1 known").arg(st.key));
        ids << st.id;
      }
      check(!ids.isEmpty(), "steps for the kind");
    }
    bool clamp = false;
    for (const CaptureSteps::Step &st : CaptureSteps::forType(DeviceType::ClampMeter))
      clamp = clamp || st.id == "ac_a_clamp";
    check(clamp, "a clamp meter measures with its clamp");
    check(CaptureSteps::forType(DeviceType::BatteryMonitor).isEmpty(), "no steps for a battery monitor");
  }

  // 6. the state of a frame leaves the digits out
  {
    using namespace CaptureSteps;
    const FrameFormat::DataFormat f = FrameFormat::FlukeQM;
    const State a = stateOf(decoded("1.2345", "V", "DC"), f);
    const State b = stateOf(decoded("1.2391", "V", "DC"), f);
    const State neg = stateOf(decoded("-1.2345", "V", "DC"), f);
    const State range = stateOf(decoded("12.345", "V", "DC"), f);
    const State ol = stateOf(decoded("0L", "MOhm", "OH"), f);
    check(a.understood && a == b, "another value, the same state");
    check(a != neg && a != range && a != stateOf(decoded("1.2345", "V", "AC"), f), "sign, range and coupling count");
    check(!stateOf(CaptureLog::Frame{ 0, "abc", std::nullopt }, f).understood, "a frame not understood");

    Expect dcv;
    dcv.quantity = Quantity::Voltage;
    dcv.defining = SampleFlag::DC;
    Expect negative = dcv;
    negative.negative = true;
    Expect open;
    open.quantity = Quantity::Resistance;
    open.overload = true;
    check(matches(dcv, a) && matches(dcv, neg) && !matches(negative, a) && matches(negative, neg), "DC V and negative");
    check(matches(open, ol) && !matches(open, stateOf(decoded("1.000", "kOhm", "OH"), f)), "OL");
    check(!matches(dcv, ol), "Ω is not DC V");
    check(describe(negative).contains("DC") && describe(open).contains("OL"), describe(negative));

    // a new state that holds for a second
    CaptureWatcher w;
    check(!w.add(a, 0) && !w.add(b, 500) && w.add(a, 1000) && !w.add(a, 1500), "proposed once after a second");
    check(!w.add(neg, 1600) && !w.steady() && w.add(neg, 2600), "a new state starts again");
    w.setBaseline(neg);
    check(!w.add(neg, 3000) && !w.add(neg, 5000), "not the state the step started from");
    w.setBaseline(std::nullopt);
    check(w.add(neg, 5100), "without a baseline any steady state");
  }

  // 7. guided through the window
  {
    MeterConnection guidedDmm(nullptr);
    DmmDecoder::DMMInfo gInfo;
    gInfo.vendor = "Fluke";
    gInfo.model = "287";
    gInfo.protocol = FrameFormat::FlukeQM;
    guidedDmm.setDmmInfo(gInfo);
    guidedDmm.setFormat(FrameFormat::FlukeQM);
    CaptureDlg dlg(&guidedDmm, "x", nullptr);
    auto *guided = dlg.findChild<QGroupBox *>("ui_captureGuided");
    auto *shown = dlg.findChild<QLineEdit *>("ui_captureShown");
    auto *keep = dlg.findChild<QPushButton *>("ui_captureKeep");
    auto *skip = dlg.findChild<QPushButton *>("ui_captureSkip");
    auto *hint = dlg.findChild<QLabel *>("ui_captureStepHint");
    auto *kept = dlg.findChild<QTreeWidget *>("ui_captureKept");
    check(guided && shown && keep && skip && hint && kept && dlg.step() == 0, "the guided steps of a multimeter");

    const auto feedFor = [&](const QByteArray &line, qint64 from)
    {
      for (qint64 t = from; t <= from + 1200; t += 400)
        feed(guidedDmm, line, t);
    };
    feedFor("QM,+1.2345 V DC\r", 10000);
    check(shown->text().startsWith("1.2345 V DC") || shown->text().startsWith("+1.2345 V DC"), shown->text());
    check(hint->text().isEmpty(), "DC V is what the step wants");
    keep->click();
    check(dlg.step() == 1 && dlg.log().kept().last().step == "dc_v" && kept->topLevelItem(0)->text(2) == "dc_v",
          "kept in step dc_v, on to the next");
    feedFor("QM,+1.2391 V DC\r", 12000);
    check(shown->text().isEmpty(), "the same state again is not proposed");
    feedFor("QM,-1.2345 V DC\r", 14000);
    check(shown->text().contains("-1.2345"), shown->text());
    shown->setText("-1.2345 V DC (corrected)");
    keep->click();
    check(dlg.log().kept().last().shown == "-1.2345 V DC (corrected)", "what the user typed is kept");

    skip->click();   // no AC V
    check(dlg.step() == 3, "skipped AC V");
    feedFor("QM,+230.1 V AC\r", 16000);   // not Ω: proposed, with a hint
    check(!shown->text().isEmpty() && !hint->text().isEmpty(), hint->text());
    shown->setText("120.3 Ohm");   // the meter showed Ω, QtDMM read V AC
    keep->click();
    check(dlg.log().kept().last().unexpected, "kept although QtDMM read something else");

    const QString yaml = dlg.yaml();
    check(yaml.contains("  # step dc_v\n  - source_line: \"") && yaml.contains("  # step dc_v_neg\n"), yaml);
    check(yaml.contains("  # step ohm - QtDMM read something else than the step wants\n"), yaml);
    check(yaml.contains("# steps skipped: ac_v\n"), yaml);

    // back to AC V: no longer skipped
    for (QPushButton *b : dlg.findChildren<QPushButton *>())
      if (b->text() == "&Back")
      {
        b->click();
        b->click();
      }
    check(dlg.step() == 2 && !dlg.yaml().contains("steps skipped"), "back to a skipped step");

    // by hand: no step taken
    guided->setChecked(false);
    shown->setText("by hand");
    keep->click();
    check(dlg.step() == 2 && dlg.log().kept().last().step.isEmpty(), "without guided steps nothing moves on");
  }

  return failed == 0 ? 0 : 1;
}
