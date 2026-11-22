// Copyright (c) 2026 The QtDMM developers
// SPDX-License-Identifier: GPL-3.0-or-later
//
// "My devices": the meter keys (Settings::isMeterKey(), the same that a new
// instance does not copy), DeviceLibrary in devices.conf - what an entry
// keeps, the old snapshots cut down, a find recognised by its place - and
// the stable name of a serial port.
#include <QCoreApplication>
#include <QDeadlineTimer>
#include <QDebug>
#include <QDir>
#include <QFile>
#include <QSettings>
#include <QSignalSpy>
#include <QTemporaryDir>

#include "core/devicelibrary.h"
#include "core/settings.h"
#include "device/transports/serial.h"
#include "testutil.h"

int main(int argc, char **argv)
{
  QCoreApplication app(argc, argv);
  QTemporaryDir dir;
  // what the model table says in QtDMM (InstanceWidget)
  DeviceLibrary::setModelTransport([](const QString &model) -> QString
  {
    if (model.startsWith("Victron"))
      return "ble";
    return model == "Uni-Trend UT60BT" ? "blegatt" : QString();
  });

  // --- 1. the meter keys: exactly what copyConfig() leaves out of a new
  //        instance (apart from the window position and the SCPI switch) ---
  Settings ut61e("ut61e", dir.path());
  ut61e.setString("DMM/model", "UNI-T UT61E");
  ut61e.setString("DMM/data-format", "Cyrustek ES51922");
  ut61e.setBool("DMM/rts", false);
  ut61e.setString("Port settings/device", "SERIAL /dev/ttyUSB0");
  ut61e.setInt("Port settings/baud", 19200);
  ut61e.setString("Port settings/ble-key", "");
  ut61e.setString("Port settings/custom_device0", "RFC2217 bench:4000");
  ut61e.setString("Port settings/sigrok_exe", "/usr/bin/sigrok-cli");
  ut61e.setInt("Graph/sample-time", 5);
  ut61e.setInt("Position/x", 10);
  ut61e.save();
  const QVariantMap keys = ut61e.meterKeys();
  check(keys.size() == 6 && keys.value("DMM/model") == "UNI-T UT61E" && keys.contains("Port settings/baud")
          && !keys.contains("Port settings/custom_device0") && !keys.contains("Port settings/sigrok_exe")
          && !keys.contains("Graph/sample-time"),
        "meter keys: " + QStringList(keys.keys()).join(", "));

  // a simulated meter or calculated value of before without its port line:
  // its model keeps its keys (they open as a logic node)
  {
    const QVariantMap sim = DeviceLibrary::entryKeys({ { "DMM/model", "QtDMM Simulated meter" }, { "DMM/virtual-max", "5" } });
    const QVariantMap calc = DeviceLibrary::entryKeys({ { "DMM/model", "QtDMM Calculated value" }, { "DMM/calc-expression", "u * i" } });
    check(sim.value("DMM/virtual-max") == "5" && calc.value("DMM/calc-expression") == "u * i",
          "an entry of before without its port line keeps its keys");
  }

  // --- 2. the library: add, list, apply into another instance and back ---
  DeviceLibrary lib(dir.path());
  QSignalSpy changed(&lib, &DeviceLibrary::changed);
  check(lib.list().isEmpty(), "library: empty at first");
  const QString name = QString::fromUtf8("UT61E (Werkbank) Ü/ä = ; # [x]");
  const QString id = lib.add(name, keys);
  check(!id.isEmpty() && changed.size() >= 1, "library: add");
  QList<MyDevice> all = lib.list();
  const QVariantMap entry = DeviceLibrary::entryKeys(keys);
  check(all.size() == 1 && all.first().name == name && all.first().keys == entry
          && entry.size() == 5 && !entry.contains("Port settings/ble-key"),
        "library: the entry as added (no Bluetooth key at a serial port), special characters in the name: "
          + (all.isEmpty() ? QString() : all.first().name));
  check(all.first().where() == "/dev/ttyUSB0" && all.first().model() == "UNI-T UT61E", "library: where and model");
  check(QFile::exists(dir.path() + "/devices.conf"), "library: devices.conf next to the instances");
  check(!Settings("x", dir.path()).getConfigInstances().contains("devices"), "library: devices.conf is no instance");

  Settings other("other", dir.path());
  other.setString("DMM/model", "Brymen BM869s");
  other.setInt("Graph/sample-time", 1);
  other.save();
  other.setValues(lib.find(id)->keys);
  other.save();
  check(other.meterKeys() == entry && other.getInt("Graph/sample-time") == 1, "apply: the meter keys, nothing else");
  other.setString("Port settings/baud", "2400");
  other.setString("DMM/my-device", id);
  other.save();
  check(lib.findByPlace(other.meterKeys()) == id, "findByPlace: the same port, whatever the baud rate");

  // --- 3. names, order, duplicate, rename, remove ---
  check(lib.uniqueName(name) == name + " (2)" && lib.uniqueName("BM869s") == "BM869s", "uniqueName");
  QVariantMap ble = { { "DMM/model", "Victron SmartShunt" }, { "Port settings/device", "BLE" },
                      { "Port settings/ble-address", "AA:BB:CC:DD:EE:FF" } };
  const QString id2 = lib.add("SmartShunt", ble);
  const QString id3 = lib.add("Virtual", { { "DMM/model", "QtDMM Virtual meter" },
                                           { "Port settings/device", "calc V/DC 1 + t" } });
  check(lib.find(id2)->where() == "AA:BB:CC:DD:EE:FF" && lib.find(id3)->where() == "V/DC 1 + t", "where: BLE, calc");
  auto order = [&]
  {
    QStringList n;
    for (const MyDevice &d : lib.list())
      n << d.name.left(5);
    return n.join(' ');
  };
  check(order() == "UT61E Smart Virtu", "order: as added, got " + order());
  lib.move(id3, 0);
  check(order() == "Virtu UT61E Smart", "move: got " + order());
  const QString dup = lib.duplicate(id);
  check(order() == "Virtu UT61E UT61E Smart" && lib.find(dup)->name == name + " (2)", "duplicate: right after it, got " + order());
  check(lib.rename(dup, "Bench 2") && lib.find(dup)->name == "Bench 2" && !lib.rename(dup, "  "), "rename");
  check(lib.update(id2, { { "DMM/model", "Victron SmartShunt" } }) && lib.find(id2)->keys.size() == 1, "update replaces the keys");
  check(lib.remove(dup) && !lib.find(dup) && lib.list().size() == 3, "remove");

  // --- 4. two instances: one sees what the other wrote, nothing is lost ---
  {
    DeviceLibrary a(dir.path()), b(dir.path());
    const QString ia = a.add("A", { { "DMM/model", "A" } });
    const QString ib = b.add("B", { { "DMM/model", "B" } });
    check(!ia.isEmpty() && !ib.isEmpty() && a.find(ib) && b.find(ia) && a.list().size() == 5,
          QString("two instances: both entries kept, got ids '%1' '%2', %3 entries")
            .arg(ia, ib).arg(a.list().size()));
    QSignalSpy seen(&a, &DeviceLibrary::changed);
    b.rename(ia, "A2");
    QDeadlineTimer wait(2000);
    while (seen.isEmpty() && !wait.hasExpired())
      QCoreApplication::processEvents(QEventLoop::AllEvents, 50);
    const std::optional<MyDevice> renamed = a.find(ia);
    check(!seen.isEmpty() && renamed && renamed->name == "A2",
          QString("two instances: a change in one is changed() in the other, signal %1, name '%2'")
            .arg(seen.isEmpty() ? "missing" : "seen", renamed ? renamed->name : QString("(no entry)")));
  }

  // --- 5. what an entry keeps: the model, the place, what the way of
  //        connecting needs - nothing of another transport ---
  {
    const QVariantMap snapshot = { { "DMM/model", "UNI-T UT61E" }, { "DMM/data-format", "CyrustekES51922" },
                                   { "DMM/display", "22000" }, { "DMM/number-of-values", "1" },
                                   { "DMM/configured", "true" }, { "DMM/rts", "false" }, { "DMM/dtr", "true" },
                                   { "DMM/external-setup", "false" },
                                   { "Port settings/device", "SERIAL /dev/ttyUSB0" },
                                   { "Port settings/baud", "19200" }, { "Port settings/bits", "7" },
                                   { "Port settings/parity", "2" }, { "Port settings/stop-bits", "1" },
                                   { "DMM/virtual-formula", "sin(t)" }, { "DMM/virtual-unit", "V" },
                                   { "DMM/calc-unit", "W" }, { "DMM/calc-expression", "u*i" },
                                   { "Port settings/ble-address", "18:90:67:00:00:01 UT60BT" },
                                   { "Port settings/ble-key", "0123456789abcdef0123456789abcdef" },
                                   { "Port settings/ble-main", "PV" }, { "Port settings/ble-second", "V" },
                                   { "Port settings/ble-third", "-" }, { "Port settings/ble-fourth", "I" },
                                   { "Port settings/sigrok-conn", "" }, { "DMM/my-device", "abc" } };
    QStringList serial = DeviceLibrary::entryKeys(snapshot).keys();
    check(serial.join(',') == "DMM/data-format,DMM/display,DMM/dtr,DMM/external-setup,DMM/model,DMM/number-of-values,"
                              "DMM/rts,Port settings/baud,Port settings/bits,Port settings/device,"
                              "Port settings/parity,Port settings/stop-bits",
          "entry: serial, got " + serial.join(','));
    QVariantMap victron = snapshot;
    victron.insert("DMM/model", "Victron SmartShunt");
    victron.insert("Port settings/device", "ble 18:90:67:00:00:01");
    const QStringList ble = DeviceLibrary::entryKeys(victron).keys();
    check(ble.join(',') == "DMM/data-format,DMM/display,DMM/model,DMM/number-of-values,Port settings/ble-address,"
                           "Port settings/ble-fourth,Port settings/ble-key,Port settings/ble-main,Port settings/ble-second,"
                           "Port settings/ble-third,Port settings/device",
          "entry: Victron, got " + ble.join(','));
    QVariantMap gatt = snapshot;
    gatt.insert("DMM/model", "Uni-Trend UT60BT");
    const QStringList g = DeviceLibrary::entryKeys(gatt).keys();
    check(g.contains("Port settings/ble-address") && !g.contains("Port settings/ble-key") && !g.contains("Port settings/baud"),
          "entry: GATT by its model, got " + g.join(','));
    QVariantMap virt = snapshot;
    virt.insert("DMM/model", "QtDMM Virtual meter");
    virt.insert("Port settings/device", "calc V/DC sin(t)");
    const QStringList v = DeviceLibrary::entryKeys(virt).keys();
    check(v.contains("DMM/virtual-formula") && v.contains("DMM/virtual-unit") && !v.contains("DMM/calc-unit")
            && !v.contains("Port settings/baud"),
          "entry: virtual meter, got " + v.join(','));
  }

  // --- 6. a devices.conf of whole snapshots is cut down when it is read;
  //        a Bluetooth meter with the port of the meter before gets its own ---
  {
    QTemporaryDir old;
    {
      QSettings f(old.path() + "/devices.conf", QSettings::IniFormat);
      f.setValue("device-0000aaaa/name", "Uni-Trend UT60BT");
      f.setValue("device-0000aaaa/order", 1);
      f.setValue("device-0000aaaa/DMM/model", "Uni-Trend UT60BT");
      f.setValue("device-0000aaaa/Port settings/device", "SERIAL /dev/serial/by-id/usb-Prolific-if00-port0");
      f.setValue("device-0000aaaa/Port settings/ble-address", "18:90:67:00:00:01 UT60BT");
      f.setValue("device-0000aaaa/Port settings/baud", "600");
      f.setValue("device-0000aaaa/DMM/virtual-formula", "sin(t)");
      f.setValue("device-0000bbbb/name", "UT61E");
      f.setValue("device-0000bbbb/order", 2);
      f.setValue("device-0000bbbb/DMM/model", "UNI-T UT61E");
      f.setValue("device-0000bbbb/Port settings/device", "SERIAL /dev/serial/by-id/usb-Prolific-if00-port0");
      f.setValue("device-0000bbbb/Port settings/baud", "19200");
      f.setValue("device-0000bbbb/Port settings/ble-address", "18:90:67:00:00:01 UT60BT");
      f.setValue("device-0000bbbb/Port settings/ble-main", "");
      f.setValue("device-0000bbbb/DMM/configured", "true");
    }
    DeviceLibrary tidied(old.path());
    const std::optional<MyDevice> ut60 = tidied.find("0000aaaa");
    const std::optional<MyDevice> ut61 = tidied.find("0000bbbb");
    check(ut60 && ut60->name == "Uni-Trend UT60BT" && ut60->order == 1
            && ut60->keys.value("Port settings/device") == "blegatt 18:90:67:00:00:01"
            && !ut60->keys.contains("Port settings/baud") && !ut60->keys.contains("DMM/virtual-formula"),
          "tidy: the UT60BT at its Bluetooth address, got " + (ut60 ? QStringList(ut60->keys.keys()).join(',') : QString()));
    check(ut61 && ut61->order == 2 && ut61->keys.value("Port settings/baud") == "19200"
            && !ut61->keys.contains("Port settings/ble-address") && !ut61->keys.contains("Port settings/ble-main")
            && !ut61->keys.contains("DMM/configured"),
          "tidy: the UT61E without Bluetooth keys, got " + (ut61 ? QStringList(ut61->keys.keys()).join(',') : QString()));
    check(QSettings(old.path() + "/devices.conf", QSettings::IniFormat).value("device-0000bbbb/Port settings/ble-main",
                                                                               "gone") == "gone",
          "tidy: in the file as well");

    // --- 7. found = known: by the place, not by all keys ---
    check(tidied.findByPlace({ { "Port settings/ble-address", "18:90:67:00:00:01" } }) == "0000aaaa",
          "place: a Bluetooth find by its address");
    check(tidied.findByPlace({ { "Port settings/ble-address", "18:90:67:00:00:02" } }).isEmpty(),
          "place: another address is another device");
    const QString hidA = tidied.add("UT803", { { "DMM/model", "UNI-T UT803" },
                                               { "Port settings/device", "HID 0x1a86:0xe008 /dev/hidraw2" } });
    check(tidied.findByPlace({ { "Port settings/device", "HID 0x1a86:0xe008 /dev/hidraw5" } }) == hidA,
          "place: an HID cable plugged in again (another hidraw number)");
    const QString hidB = tidied.add("UT803 (2)", { { "DMM/model", "UNI-T UT803" },
                                                   { "Port settings/device", "HID 0x1a86:0xe008 /dev/hidraw3" } });
    check(tidied.findByPlace({ { "Port settings/device", "HID 0x1a86:0xe008 /dev/hidraw3" } }) == hidB,
          "place: of two cables of one type the one at the path");
    check(DeviceLibrary::place({ { "Port settings/device", "ble cb:09:e4:16:33:db 0123 PV V" } })
            == DeviceLibrary::place({ { "Port settings/ble-address", "CB:09:E4:16:33:DB SmartShunt" } }),
          "place: the address of a Victron device string and of a find");
    // a simulated meter is nowhere, also with a port left over from before 26.2
    check(DeviceLibrary::place({ { "DMM/model", "QtDMM Virtual meter" },
                                 { "Port settings/device", "SERIAL /dev/ttyUSB0" } }).isEmpty(),
          "place: none for a simulated meter with an old port");
  }

#ifdef Q_OS_LINUX
  // --- 8. the stable port name: the link in by-id that points to the port ---
  {
    QDir(dir.path()).mkpath("dev/serial/by-id");
    const QString tty = dir.path() + "/dev/ttyUSB0";
    QFile f(tty);
    check(f.open(QIODevice::WriteOnly), "ttyUSB0 written");
    f.close();
    check(QFile(dir.path() + "/dev/ttyUSB1").open(QIODevice::WriteOnly), "ttyUSB1 written");
    const QString byId = dir.path() + "/dev/serial/by-id";
    QFile::link("../../ttyUSB1", byId + "/usb-Other_cable-if00-port0");
    QFile::link("../../ttyUSB0", byId + "/usb-WCH.CN_USB_Quad_Serial_0123-if00-port0");
    check(SerialDevice::stablePortName(tty, byId) == byId + "/usb-WCH.CN_USB_Quad_Serial_0123-if00-port0",
          "stable: the by-id link, got " + SerialDevice::stablePortName(tty, byId));
    check(SerialDevice::stablePortName(dir.path() + "/dev/ttyS0", byId) == dir.path() + "/dev/ttyS0",
          "stable: no link, the port as it is");
    const QString link = byId + "/usb-Other_cable-if00-port0";
    check(SerialDevice::stablePortName(link, byId) == link, "stable: a by-id name stays");
    // a USB-HID cable: its udev link in /dev/qtdmm, none: as it is
    {
      QDir(dir.path()).mkpath("dev/qtdmm");
      check(QFile(dir.path() + "/dev/hidraw3").open(QIODevice::WriteOnly), "hidraw3 written");
      const QString links = dir.path() + "/dev/qtdmm";
      QFile::link("../hidraw3", links + "/holtek-co2-pci-0000_02_00_0-usb-0_11_6_1_0");
      const QString raw = "HIDHOLTEK 0x04d9:0xa052 " + dir.path() + "/dev/hidraw3";
      check(SerialDevice::stableDevice(raw, links) == "HIDHOLTEK 0x04d9:0xa052 " + links + "/holtek-co2-pci-0000_02_00_0-usb-0_11_6_1_0",
            "stable HID: the udev link, got " + SerialDevice::stableDevice(raw, links));
      const QString other = "HID 0x1a86:0xe008 " + dir.path() + "/dev/hidraw4";
      check(SerialDevice::stableDevice(other, links) == other, "stable HID: no link, as it is");
      check(SerialDevice::stableDevice("calc V 1", links) == "calc V 1", "stable: another kind as it is");
    }

    // a by-id name in the entry, the ttyUSB in the find: the same place
    DeviceLibrary ports(dir.path());
    const QString onCable = ports.add("On the WCH cable", { { "DMM/model", "UNI-T UT61E" },
                                      { "Port settings/device",
                                        "SERIAL " + byId + "/usb-WCH.CN_USB_Quad_Serial_0123-if00-port0" } });
    check(ports.findByPlace({ { "Port settings/device", "SERIAL " + tty } }) == onCable,
          "place: by-id and ttyUSB0 are one port");
    check(ports.findByPlace({ { "Port settings/device", "SERIAL " + dir.path() + "/dev/ttyUSB1" } }).isEmpty(),
          "place: the other cable is not it");
  }
#endif

#if !defined(Q_OS_WIN)
  // 10. a write that fails is reported (review R9-04): the check after an
  //     error reads the file, not the cache QSettings keeps in the process
  {
    QTemporaryDir ro;
    DeviceLibrary lib(ro.path());
    const QVariantMap sim { {"DMM/model", "QtDMM Simulated meter"}, {"Port settings/device", "calc V/DC t"} };
    check(!lib.add("First", sim).isEmpty(), "read-only: the first entry");
    // QSaveFile needs a file next to devices.conf: the directory read-only
    QFile::setPermissions(ro.path(), QFile::ReadOwner | QFile::ExeOwner);
    if (QFileInfo(ro.path()).isWritable())
      qInfo() << "read-only check skipped: the directory stays writable (root?)";
    else
    {
      check(lib.add("Second", sim).isEmpty(), "read-only: add() says it failed");
      const std::optional<MyDevice> first = lib.list().isEmpty() ? std::nullopt : std::optional(lib.list().first());
      check(first && !lib.rename(first->id, "Renamed"), "read-only: rename() says it failed");
    }
    QFile::setPermissions(ro.path(), QFile::ReadOwner | QFile::WriteOwner | QFile::ExeOwner);
  }
#endif

  // an entry brought back by a workspace keeps its id; one taken or not a
  // plain word gets a new one
  {
    QTemporaryDir own;
    DeviceLibrary lib(own.path());
    const QVariantMap sim{ { "DMM/model", "QtDMM Simulated meter" } };
    check(lib.add("Back", sim, "ab12cd34") == "ab12cd34" && lib.find("ab12cd34")->name == "Back", "add() with its id");
    const QString again = lib.add("Twice", sim, "ab12cd34");
    check(!again.isEmpty() && again != "ab12cd34", "the id taken: a new one");
    const QString odd = lib.add("Odd", sim, "x/y]");
    check(!odd.isEmpty() && odd != "x/y]" && lib.find(odd), "no plain word: a new one");
  }

  if (failed)
  {
    qWarning() << failed << "device library check(s) failed";
    return 1;
  }
  qInfo() << "All device library tests passed.";
  return 0;
}
