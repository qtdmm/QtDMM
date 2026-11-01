// Copyright (c) 2026 The QtDMM developers
// SPDX-License-Identifier: GPL-3.0-or-later
//
// "Find device": the families behind the USB cables, the USB-serial
// adapters, the access check with its fix, the udev rules, and the
// discoverers ending their search. No meter is opened.
#include <QCoreApplication>
#include <QDeadlineTimer>
#include <QDebug>
#include <QFile>
#include <QSignalSpy>
#include <QTemporaryDir>

#include "device/discovery/discovery.h"
#include "testutil.h"

static bool finishes(Discoverer &d, int ms)
{
  QSignalSpy done(&d, &Discoverer::finished);
  d.start();
  QDeadlineTimer wait(ms);
  while (done.isEmpty() && !wait.hasExpired())
    QCoreApplication::processEvents(QEventLoop::AllEvents, 50);
  d.stop();
  return !done.isEmpty();
}

int main(int argc, char **argv)
{
  QCoreApplication app(argc, argv);

  // QTDMM_DISCOVERY_LIVE=1: search everything here and list what turns up
  // (not part of ctest; addresses shortened, nothing is opened)
  if (qEnvironmentVariableIsSet("QTDMM_DISCOVERY_LIVE"))
  {
    UsbDiscoverer usb;
    SerialDiscoverer serial;
    BridgeDiscoverer bridge;
    BleDiscoverer ble;
    int running = 0;
    for (Discoverer *d : std::initializer_list<Discoverer *> { &usb, &serial, &bridge, &ble })
    {
      QObject::connect(d, &Discoverer::found, [](const Candidate &c)
      {
        QString detail = c.detail;
        if (c.kind == Candidate::Bluetooth)
          detail = c.key.left(8) + "…";
        qInfo().noquote() << QString("[%1] %2 | %3 | models: %4 | %5").arg(c.kind).arg(c.title, detail)
                               .arg(c.models.mid(0, 3).join(", ")).arg(c.problem.isEmpty() ? "ok" : c.problem);
      });
      QObject::connect(d, &Discoverer::finished, [&running] { --running; });
      ++running;
      d->start();
    }
    qInfo().noquote() << "Bluetooth:" << (BleDiscoverer::unavailable().isEmpty() ? "available" : BleDiscoverer::unavailable());
    QDeadlineTimer wait(12000);
    while (running > 0 && !wait.hasExpired())
      QCoreApplication::processEvents(QEventLoop::AllEvents, 50);
    return 0;
  }

  // --- 1. the families of the USB cables and sensors ---
  {
    const auto d09 = Families::forUsb(0x10c4, 0xea80);
    const auto d09b = Families::forUsb(0x1a86, 0xe429);
    check(d09 && d09b && d09->cable == "UT-D09" && d09->portType == "HID" && !d09->models.isEmpty()
            && d09->models.first() == "Uni-Trend UT61E+" && d09->models == d09b->models,
          "UT-D09: the UT61+ family, UT61E+ first: " + (d09 ? d09->models.join(", ") : QString()));
    const auto d04 = Families::forUsb(0x1a86, 0xe008);
    check(d04 && d04->cable == "UT-D04" && d04->models.size() > 5 && d04->models.first() == "Uni-Trend UT61E"
            && !d04->models.contains("Uni-Trend UT61E+") && d04->hint.contains("RS232"),
          "UT-D04: the serial UNI-T meters, UT61E first: " + (d04 ? d04->models.join(", ") : QString()));
    check(Families::forUsb(0x04fa, 0x2490).has_value(), "UT-D04 with the Hoitek chip");
    const auto bu86x = Families::forUsb(0x0820, 0x0001);
    check(bu86x && !bu86x->models.isEmpty() && bu86x->models.first().startsWith("Brymen"), "BU-86X: Brymen");
    const auto tfa = Families::forUsb(0x04d9, 0xa052);
    check(tfa && tfa->portType == "HIDHOLTEK" && tfa->models.size() == 3
            && tfa->models.first() == "TFA Dostmann AIRCO2NTROL Mini *",
          "TFA: the AIRCO2NTROL Mini first, HIDHOLTEK: " + (tfa ? tfa->models.join(", ") : QString()));
    check(!Families::forUsb(0x046d, 0xc52b), "a mouse is no meter");
  }

  // --- 2. USB-serial adapters by VID:PID ---
  check(Families::adapter(0x0403, 0x6001) == "FTDI" && Families::adapter(0x1a86, 0x7523) == "CH340"
          && Families::adapter(0x067b, 0x2303) == "PL2303" && Families::adapter(0x0557, 0x2008) == "PL2303"
          && Families::adapter(0x10c4, 0xea60) == "CP210x" && Families::adapter(0x1234, 0x5678).isEmpty(),
        "adapters by VID:PID");
  check(Families::models(FrameFormat::Invalid, true).size() > 50, "all serial models");
  {
    const QStringList victron = Families::models(FrameFormat::VictronBLE, false);
    check(Families::preferByName(victron, "SmartShunt HQ2203").first() == "Victron SmartShunt"
            && Families::preferByName(victron, "SmartSolar HQ2105").first() == "Victron SmartSolar MPPT"
            && Families::preferByName(victron, "").first() == victron.first(),
          "Victron: the model named like the device first");
    // the APPA Bluetooth meters advertise "vendor model", written their way
    const QStringList appa = Families::models(FrameFormat::AppaDmm, false);
    const auto first = [&](const QString &name) { return Families::preferByName(appa, name).first(); };
    check(first("APPA S1") == "APPA S1 *" && first("BENNING MM10-1") == "BENNING MM 10-1 *"
            && first("KPS DMM9000BT") == "KPS DMM9000BT (Bluetooth) *" && first("MEGGER DCM1500S") == "Megger DCM1500S *"
            && first("RS PRO 157B") == "RS PRO 157B *" && first("APPA 506B") == "APPA 506B (Bluetooth) *",
          "APPA: the model named like the device first, got " + first("BENNING MM10-1") + " / " + first("KPS DMM9000BT"));
    check(!appa.contains("APPA 506 *") && appa.contains("APPA S0 *"), "APPA over Bluetooth: only the rows without a baud rate");
  }

  // --- 3. access: nothing to say when QtDMM may open it; else why, and
  //        the fix - the udev rule with the cable's ids, or the group ---
#ifdef Q_OS_LINUX
  {
    QTemporaryDir dir;
    const QString path = dir.path() + "/hidraw3";
    QFile f(path);
    check(f.open(QIODevice::WriteOnly), "access: the device file written");
    f.close();
    check(accessProblem(path, true, 0x1a86, 0xe008).problem.isEmpty(), "access: a readable device is fine");
    f.setPermissions(QFileDevice::ReadOther);
    const AccessProblem hid = accessProblem(path, true, 0x1a86, 0xe008);
    if (QFileInfo(path).isWritable())
      qInfo() << "running as root: the access check is skipped";
    else
    {
      check(hid.problem.contains(path) && hid.fix.contains("ATTRS{idVendor}==\"1a86\"")
              && hid.fix.contains("ATTRS{idProduct}==\"e008\"") && hid.fix.contains("udevadm"),
            "access: HID problem and fix:\n" + hid.problem + "\n" + hid.fix);
      const AccessProblem tty = accessProblem(path, false);
      check(tty.fix.contains("usermod -aG"), "access: serial fix is the group:\n" + tty.fix);
    }
    check(accessProblem(dir.path() + "/gone", true).problem.isEmpty(), "access: a missing path says nothing");
  }
#endif
  const QString rules = udevRules();
  for (const char *id : { "\"1a86\", ATTRS{idProduct}==\"e008\"", "\"04fa\", ATTRS{idProduct}==\"2490\"",
                          "\"10c4\", ATTRS{idProduct}==\"ea80\"", "\"1a86\", ATTRS{idProduct}==\"e429\"",
                          "\"0820\", ATTRS{idProduct}==\"0001\"", "\"04d9\", ATTRS{idProduct}==\"a052\"" })
    check(rules.contains(id), QString("udev rules: %1").arg(id));
  check(rules.count("TAG+=\"uaccess\"") == 6, "udev rules: uaccess for each");
  if (argc > 1)
  {
    // the file the packages install is the same as the text the dialog shows
    QFile file(QString::fromLocal8Bit(argv[1]) + "/assets/linux/70-qtdmm.rules");
    check(file.open(QIODevice::ReadOnly | QIODevice::Text) && QString::fromUtf8(file.readAll()) == rules,
          "udev rules: assets/linux/70-qtdmm.rules differs from udevRules()");
  }

  // --- 4. the discoverers end their search (whatever is plugged in) ---
  {
    UsbDiscoverer usb;
    SerialDiscoverer serial;
    BridgeDiscoverer bridge;
    BleDiscoverer ble;
    check(finishes(usb, 3000), "USB search ends");
    check(finishes(serial, 3000), "serial search ends");
    check(finishes(bridge, 6000), "network search ends");
    if (BleDiscoverer::unavailable().isEmpty())
      qInfo() << "Bluetooth available: its scan is not run in the test";
    else
      check(finishes(ble, 2000), "Bluetooth search without Bluetooth ends at once");
  }

  if (failed)
  {
    qWarning() << failed << "discovery check(s) failed";
    return 1;
  }
  qInfo() << "All discovery tests passed.";
  return 0;
}
