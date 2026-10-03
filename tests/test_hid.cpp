// Tests for the HID cable driver's pure parts: chip detection from the port
// entry and the input report layouts of the CH9325 and CH9329 cables (no
// hardware needed; the layouts follow sigrok's serial_hid_ch9325.c and PR #298).
#include <QCoreApplication>
#include <QDebug>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <cstring>

#include "device/transports/hidserial.h"
#include "device/transports/hidholtek.h"

static int failed = 0;

static void check(bool cond, const QString &what)
{
  if (!cond)
  {
    qWarning() << "FAILED:" << what;
    failed++;
  }
}

int main(int argc, char **argv)
{
  QCoreApplication app(argc, argv);
  using Chip = HIDSerialDevice::Chip;

  // --- 1. chip by usb id / port entry ---
  check(HIDSerialDevice::chipFor(0x1a86, 0xe008) == Chip::CH9325, "1a86:e008 is a CH9325");
  check(HIDSerialDevice::chipFor(0x04fa, 0x2490) == Chip::CH9325, "04fa:2490 (HE2325U) is CH9325-compatible");
  check(HIDSerialDevice::chipFor(0x1a86, 0xe429) == Chip::CH9329, "1a86:e429 is a CH9329");
  check(HIDSerialDevice::chipFor(0x10c4, 0xea80) == Chip::CP2110, "10c4:ea80 is a CP2110");
  check(HIDSerialDevice::chipFor(0x0820, 0x0001) == Chip::BU86X, "0820:0001 is a Brymen BU-86X");
  check(HIDSerialDevice::chipFor(0x1234, 0x5678) == Chip::CH9325, "unknown ids fall back to CH9325");
  check(HIDSerialDevice::chipForEntry("HID 0x1a86:0xe429 /dev/hidraw3") == Chip::CH9329, "entry with e429");
  check(HIDSerialDevice::chipForEntry("HID 0x1a86:0xe008 \\\\?\\hid#vid_1a86&pid_e008#7&1a2b#{4d1e55b2}") == Chip::CH9325, "windows entry");
  check(HIDSerialDevice::chipForEntry("/dev/hidraw3") == Chip::CH9325, "bare path defaults to CH9325");
  check(HIDSerialDevice::pathForEntry("HID 0x1a86:0xe429 /dev/hidraw3") == "/dev/hidraw3", "path split off");
  check(HIDSerialDevice::pathForEntry("/dev/hidraw3") == "/dev/hidraw3", "bare path kept");

  // --- 2. CH9325 report: 0xF0 | count, payload bytes with the top bit set ---
  {
    unsigned char out[64];
    const unsigned char r[8] = { 0xf3, 0xb1, 0xb2, 0xb3, 0x00, 0x00, 0x00, 0x00 };
    int n = HIDSerialDevice::unpackReport(Chip::CH9325, r, 8, out);
    check(n == 3 && out[0] == 0x31 && out[1] == 0x32 && out[2] == 0x33, "CH9325: 3 bytes, top bit stripped");
    const unsigned char empty[8] = { 0xf0, 0, 0, 0, 0, 0, 0, 0 };
    check(HIDSerialDevice::unpackReport(Chip::CH9325, empty, 8, out) == 0, "CH9325: empty report");
    const unsigned char full[8] = { 0xf7, 0x80, 0x81, 0x82, 0x83, 0x84, 0x85, 0x86 };
    n = HIDSerialDevice::unpackReport(Chip::CH9325, full, 8, out);
    check(n == 7 && out[6] == 0x06, "CH9325: 7 bytes");
    const unsigned char shortRep[2] = { 0xf3, 0xb1 };
    check(HIDSerialDevice::unpackReport(Chip::CH9325, shortRep, 2, out) == -1, "CH9325: count beyond report is malformed");
  }

  // --- 3. CH9329 report: count, then raw bytes (8 bit, no masking) ---
  {
    unsigned char out[64];
    unsigned char r[64];
    memset(r, 0, sizeof r);
    r[0] = 4; r[1] = 0x2b; r[2] = 0x30; r[3] = 0x0d; r[4] = 0x8a;
    int n = HIDSerialDevice::unpackReport(Chip::CH9329, r, 64, out);
    check(n == 4 && out[0] == 0x2b && out[3] == 0x8a, "CH9329: 4 raw bytes, top bit kept");
    r[0] = 0;
    check(HIDSerialDevice::unpackReport(Chip::CH9329, r, 64, out) == 0, "CH9329: empty report");
    r[0] = 63;
    for (int i = 1; i <= 63; ++i) r[i] = static_cast<unsigned char>(i);
    n = HIDSerialDevice::unpackReport(Chip::CH9329, r, 64, out);
    check(n == 63 && out[62] == 63, "CH9329: full 63-byte report");
    r[0] = 64;
    check(HIDSerialDevice::unpackReport(Chip::CH9329, r, 64, out) == -1, "CH9329: count 64 is malformed");
    r[0] = 10;
    check(HIDSerialDevice::unpackReport(Chip::CH9329, r, 5, out) == -1, "CH9329: count beyond report is malformed");
  }

  // --- 3b. CP2110: same data layout, plus the UART_CONFIG feature report ---
  {
    unsigned char out[64];
    unsigned char r[64] = { 3, 0x41, 0x42, 0x43 };
    check(HIDSerialDevice::unpackReport(Chip::CP2110, r, 64, out) == 3 && out[2] == 0x43, "CP2110: count byte then data");
    const QByteArray cfg = HIDSerialDevice::cp2110ConfigReport(19200, 7, 2, 1);
    check(cfg.toHex(' ') == "50 00 00 4b 00 02 00 02 00", "CP2110 config 19200 7O1: " + cfg.toHex(' '));
    const QByteArray cfg2 = HIDSerialDevice::cp2110ConfigReport(2400, 8, 0, 2);
    check(cfg2.toHex(' ') == "50 00 00 09 60 00 00 03 01", "CP2110 config 2400 8N2: " + cfg2.toHex(' '));
    check(HIDSerialDevice::cp2110ConfigReport(0, 0, 9, 0).mid(1, 4).toHex() == "00002580", "defaults: 9600 when unset");
  }

  // --- 3c. BU-86X: raw reports, and the write layouts of all chips ---
  {
    unsigned char out[64];
    const unsigned char r[8] = { 0x86, 0x86, 0x86, 0x86, 0x11, 0x22, 0x33, 0x44 };
    check(HIDSerialDevice::unpackReport(Chip::BU86X, r, 8, out) == 8 && out[0] == 0x86 && out[7] == 0x44, "BU-86X: whole report is data");
    check(HIDSerialDevice::unpackReport(Chip::BU86X, r, 3, out) == 3, "BU-86X: short report");
    const QByteArray req = QByteArray::fromHex("008666");
    check(HIDSerialDevice::packWrite(Chip::BU86X, req).toHex(' ') == "00 00 86 66", "BU-86X write: id placeholder + data");
    const QByteArray w9329 = HIDSerialDevice::packWrite(Chip::CH9329, QByteArray("D\n"));
    check(w9329.size() == 65 && w9329.left(4).toHex(' ') == "00 02 44 0a", "CH9329 write: placeholder, count, data, padded to 64");
    check(HIDSerialDevice::packWrite(Chip::CP2110, QByteArray("D\n")).toHex(' ') == "02 44 0a", "CP2110 write: count as report id");
    check(HIDSerialDevice::packWrite(Chip::CH9325, QByteArray("D\n")).isEmpty(), "CH9325 cannot write");
  }

  // --- 3d. the shared vectors: tests/data/hid_cables.json is what the bridge
  //         tests read too, so both implementations are held to one truth ---
  {
    const QString path = argc > 1 ? QString::fromLocal8Bit(argv[1]) : QString();
    QFile f(path);
    check(!path.isEmpty() && f.open(QIODevice::ReadOnly), "hid_cables.json given and readable: " + path);
    const QJsonObject root = QJsonDocument::fromJson(f.readAll()).object();
    check(!root.isEmpty(), "hid_cables.json parses");
    auto hex = [](const QJsonValue &v) { return QByteArray::fromHex(v.toString().remove(' ').toLatin1()); };

    int n = 0;
    for (const QJsonValue &v : root["cables"].toArray())
    {
      const QJsonObject c = v.toObject();
      const Chip chip = HIDSerialDevice::chipFor(c["vid"].toString().toUShort(nullptr, 16),
                                                 c["pid"].toString().toUShort(nullptr, 16));
      check(HIDSerialDevice::chipName(chip) == c["chip"].toString(),
            QString("cable %1:%2 is a %3").arg(c["vid"].toString(), c["pid"].toString(), c["chip"].toString()));
      ++n;
    }
    check(n == 5, "five cables in the table");

    for (const QJsonValue &v : root["unpack"].toArray())
    {
      const QJsonObject t = v.toObject();
      const QByteArray report = hex(t["report"]);
      unsigned char out[64];
      const int len = HIDSerialDevice::unpackReport(HIDSerialDevice::chipFromName(t["chip"].toString()),
                                                    reinterpret_cast<const unsigned char *>(report.constData()),
                                                    report.size(), out);
      const QString label = QString("unpack %1 %2 (%3)").arg(t["chip"].toString(), t["report"].toString(), t["why"].toString());
      if (t["expect"].isNull())
        check(len == -1, label + ": malformed expected");
      else
        check(len >= 0 && QByteArray(reinterpret_cast<const char *>(out), len) == hex(t["expect"]), label);
    }

    for (const QJsonValue &v : root["pack"].toArray())
    {
      const QJsonObject t = v.toObject();
      const QByteArray r = HIDSerialDevice::packWrite(HIDSerialDevice::chipFromName(t["chip"].toString()), hex(t["data"]));
      const QString label = QString("pack %1 (%2)").arg(t["chip"].toString(), t["why"].toString());
      if (t.contains("expect_prefix"))
        check(r.size() == t["expect_length"].toInt() && r.startsWith(hex(t["expect_prefix"])), label);
      else
        check(r == hex(t["expect"]), label + ": " + r.toHex(' '));
    }

    for (const QJsonValue &v : root["config"].toArray())
    {
      const QJsonObject t = v.toObject();
      const int parity = t["parity"].toString() == "E" ? 1 : t["parity"].toString() == "O" ? 2 : 0;
      QByteArray r;
      if (t["chip"].toString() == "CP2110")
        r = HIDSerialDevice::cp2110ConfigReport(t["baud"].toInt(), t["bits"].toInt(), parity, t["stopbits"].toInt());
      else
        r = HIDSerialDevice::ch9325ConfigReport(t["baud"].toInt(), t["bits"].toInt());
      check(r == hex(t["expect"]), QString("config %1 %2: %3").arg(t["chip"].toString()).arg(t["baud"].toInt()).arg(r.toHex(' ')));
    }
  }

  // --- 3e. the Holtek sensor (TFA AIRCO2NTROL): record layout and line framing ---
  {
    HidHoltekDevice::Record r;
    const QByteArray co2 = QByteArray::fromHex("50 03 2C 7F 0D 00 00 00");   // 0x032C = 812 ppm, sum 0x7F
    check(HidHoltekDevice::parseReport(co2, r) && r.address == 0x50 && r.raw == 812, "Holtek: CO2 record");
    const QByteArray temp = QByteArray::fromHex("42 12 68 BC 0D 00 00 00"); // 0x1268 = 4712
    check(HidHoltekDevice::parseReport(temp, r) && r.address == 0x42 && r.raw == 4712, "Holtek: temperature record");
    QByteArray bad = co2;
    bad[3] = 0x00;
    check(!HidHoltekDevice::parseReport(bad, r), "Holtek: bad checksum is rejected");
    check(!HidHoltekDevice::parseReport(QByteArray::fromHex("50 03"), r), "Holtek: 2-byte report is too short");
    check(HidHoltekDevice::parseReport(QByteArray::fromHex("50 03 2C"), r) && r.raw == 812, "Holtek: 3-byte report carries no checksum");
    check(HidHoltekDevice::formatLine(0x50, 812) == "50 812\n", "Holtek: CO2 line");
    check(HidHoltekDevice::formatLine(0x41, 4520) == "41 4520\n", "Holtek: humidity line");
    check(HidHoltekDevice::formatLine(0x0a, 7) == "0a 7\n", "Holtek: address is two hex digits");
    check(HidHoltekDevice::pathForEntry("HIDHOLTEK 0x04d9:0xa052 /dev/hidraw3") == "/dev/hidraw3", "Holtek: path split off");
    check(HidHoltekDevice::pathForEntry("/dev/hidraw3") == "/dev/hidraw3", "Holtek: bare path kept");
    DmmDecoder::DMMInfo info;
    HidHoltekDevice dev(info, "HIDHOLTEK 0x04d9:0xa052 /nonexistent/hidraw99");
    check(!dev.open(QIODevice::ReadWrite), "Holtek: open() fails without a device");
    check(!dev.errorString().isEmpty(), "Holtek: a failed open() says why");
  }

  // --- 4. a device object without hardware reports itself closed ---
  {
    DmmDecoder::DMMInfo info;
    info.baud = 9600;
    info.bits = 8;
    HIDSerialDevice dev(info, "HID 0x1a86:0xe429 /nonexistent/hidraw99");
    check(dev.chip() == Chip::CH9329, "chip taken from the entry");
    check(!dev.open(QIODevice::ReadWrite), "open() fails without a device");
  }

  if (failed == 0)
    qInfo() << "All HID cable tests passed.";
  else
    qWarning() << failed << "HID cable test(s) failed.";
  return failed == 0 ? 0 : 1;
}
