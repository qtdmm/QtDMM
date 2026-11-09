// Copyright (c) 2026 The QtDMM developers
// SPDX-License-Identifier: GPL-3.0-or-later
//
// The capture of a meter's frames: the list keeps the last ones, a frame kept
// with what the meter showed, the YAML in the form of the spec vectors with
// no port or address in it, and the window fed by MeterConnection::frame().
#include <QApplication>
#include <QCheckBox>
#include <QFile>
#include <QLineEdit>
#include <QPushButton>
#include <QTemporaryDir>
#include <QTreeWidget>

#include "device/capturelog.h"
#include "device/meterconnection.h"
#include "ui/dialogs/capturedlg.h"
#include "testutil.h"

namespace
{
// a Fluke 18x line: -121.43 V DC (docs/protocols/spec/fluke_qm.yaml)
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

  return failed == 0 ? 0 : 1;
}
