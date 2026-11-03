// Copyright (c) 2026 The QtDMM developers
// SPDX-License-Identifier: GPL-3.0-or-later
//
// The assistant "Add device" without a main window: the order of its pages
// per connection, the models each connection offers, the name that follows
// the model until it is typed in, Next only when the meter is complete, and
// the two buttons that finish it.

#include <QtWidgets>
#include <QTemporaryDir>

#include "ui/dialogs/adddevicedlg.h"
#include "ui/devicesettings.h"
#include "ui/devicesymbol.h"
#include "device/devicetype.h"
#include "core/devicelibrary.h"
#include "testutil.h"

int main(int argc, char **argv)
{
  if (qEnvironmentVariableIsEmpty("QT_QPA_PLATFORM"))
    qputenv("QT_QPA_PLATFORM", "offscreen");
  QApplication app(argc, argv);
  QTemporaryDir dir;
  DeviceLibrary library(dir.path());
  library.add("Simulated meter", { { "DMM/model", "QtDMM Virtual meter" } });   // the old model name

  // 1. the models per connection
  for (const DmmDecoder::DMMInfo &info : DmmDecoder::getDeviceConfigurations())
  {
    int kinds = 0;
    for (AddDeviceDlg::Connection c : { AddDeviceDlg::Cable, AddDeviceDlg::Bluetooth, AddDeviceDlg::Sigrok, AddDeviceDlg::Simulated })
      kinds += AddDeviceDlg::offers(c, info) ? 1 : 0;
    check(kinds == 1, "one connection for " + info.name);
    check(AddDeviceDlg::offers(AddDeviceDlg::Network, info) == AddDeviceDlg::offers(AddDeviceDlg::Cable, info),
          "network offers the cable meters: " + info.name);
  }
  auto offered = [](AddDeviceDlg::Connection c, const QString &name)
  {
    for (const DmmDecoder::DMMInfo &info : DmmDecoder::getDeviceConfigurations())
      if (info.name == name)
        return AddDeviceDlg::offers(c, info);
    return false;
  };
  check(offered(AddDeviceDlg::Cable, "Uni-Trend UT61E"), "UT61E is a cable meter");
  check(offered(AddDeviceDlg::Bluetooth, "Victron SmartShunt"), "SmartShunt is Bluetooth");
  check(offered(AddDeviceDlg::Bluetooth, "Uni-Trend UT61E+ (UT-D07B Bluetooth) *"), "UT61E+ with UT-D07B is Bluetooth");
  check(offered(AddDeviceDlg::Cable, "Uni-Trend UT61E+"), "UT61E+ with its cable is a cable meter");
  check(offered(AddDeviceDlg::Sigrok, "Keysight 34465A (sigrok) *"), "34465A is sigrok");
  check(offered(AddDeviceDlg::Simulated, "QtDMM Calculated value"), "calculated value is simulated");

  // 2. page order: sigrok and simulated go from page 1 straight to page 3
  {
    AddDeviceDlg dlg(&library);
    dlg.setDiscoverers([](AddDeviceDlg::Connection) { return QList<Discoverer *>(); });
    check(dlg.page() == AddDeviceDlg::ConnectionPage, "starts on page 1");
    check(!dlg.canGoNext(), "no Next on page 1");
    dlg.chooseConnection(AddDeviceDlg::Sigrok);
    check(dlg.page() == AddDeviceDlg::DevicePage, "sigrok: page 3");
    check(dlg.settings()->isSigrokMeter(), "sigrok: a sigrok model chosen");
    for (const QString &m : dlg.settings()->models())
      check(offered(AddDeviceDlg::Sigrok, m), "sigrok offers only sigrok models: " + m);
    check(!dlg.settings()->models().contains("Uni-Trend UT61E"), "sigrok: no cable meter");
    dlg.back();
    check(dlg.page() == AddDeviceDlg::ConnectionPage, "back to page 1");

    dlg.chooseConnection(AddDeviceDlg::Simulated);
    check(dlg.page() == AddDeviceDlg::DevicePage && dlg.settings()->isVirtual(), "simulated: page 3, the virtual meter");
    check(dlg.name() == "Simulated meter (2)", "name: unique in My devices: " + dlg.name());
    check(dlg.canGoNext(), "virtual meter: Next");
    dlg.next();
    check(dlg.page() == AddDeviceDlg::TargetPage, "page 4");
    dlg.back();
    check(dlg.page() == AddDeviceDlg::DevicePage, "back to page 3");
    dlg.next();
    dlg.findChild<QToolButton *>("ui_connect")->click();
    check(dlg.result() == QDialog::Accepted && dlg.target() == AddDeviceDlg::Connect, "Connect finishes");
    check(dlg.keys().value("DMM/model") == "QtDMM Simulated meter", "keys: the model");
  }

  // the searches of the real dialog look at this machine; here the finds are given
  auto noSearch = [](AddDeviceDlg::Connection) { return QList<Discoverer *>(); };

  // 2a. page 4 shows the device: its picture and what was set up. Every
  // kind's template draws with QSvgRenderer; with QTDMM_SHOT_DIR set the
  // page is saved as a picture to look at
  {
    auto drawn = [](const QPixmap &pm)
    {
      const QImage img = pm.toImage();
      int n = 0;
      for (int y = 0; y < img.height(); ++y)
        for (int x = 0; x < img.width(); ++x)
          n += qAlpha(img.pixel(x, y)) > 0;
      return n;
    };
    for (int t = DeviceType::HandheldDmm; t <= DeviceType::Generic; ++t)
    {
      const auto type = static_cast<DeviceType::Type>(t);
      check(drawn(DeviceSymbol::pixmap(QString(), type, QSize(130, 150))) > 2000, "symbol drawn: " + DeviceType::id(type));
    }
    check(drawn(DeviceSymbol::unknown(QSize(130, 150))) > 2000, "symbol drawn: unknown");

    const QString shots = qEnvironmentVariable("QTDMM_SHOT_DIR");
    struct Case
    {
      QString shot;
      AddDeviceDlg::Connection connection;
      QString port;
      QVariantMap keys;
      QString type;          ///< in the info, "-" for manual settings
      QString connectionText;
    };
    const QList<Case> cases = {
      { "handheld", AddDeviceDlg::Cable, "/dev/ttyUSB0", { { "DMM/model", "Uni-Trend UT61E" } }, "Handheld multimeter",
        "/dev/ttyUSB0, 19200 7O1" },
      { "bench", AddDeviceDlg::Cable, "HID 0x1a86:0xe008 /dev/hidraw3", { { "DMM/model", "Uni-Trend UT803" } },
        "Bench multimeter", "USB-HID /dev/hidraw3<" },
      // the TFA sensor's own HID port: no serial parameters either
      { "tfa", AddDeviceDlg::Cable, "HIDHOLTEK 0x04d9:0xa052 /dev/hidraw4",
        { { "DMM/model", "TFA Dostmann AIRCO2NTROL Mini" } }, "Air quality meter", "USB-HID /dev/hidraw4<" },
      { "victron", AddDeviceDlg::Bluetooth, "AA:BB:CC:DD:EE:FF",
        { { "DMM/model", "Victron SmartSolar MPPT" }, { "Port settings/ble-address", "AA:BB:CC:DD:EE:FF" },
          { "Port settings/ble-key", "00112233445566778899aabbccddeeff" } }, "Solar charge controller", "Bluetooth LE" },
      { "simulated", AddDeviceDlg::Simulated, QString(), {}, "Virtual meter", "simulated in QtDMM" },   // protocol "-"
      { "manual", AddDeviceDlg::Cable, "/dev/ttyACM0", {}, "-", "/dev/ttyACM0" },
    };
    for (const Case &c : cases)
    {
      AddDeviceDlg dlg(&library);
      dlg.setDiscoverers(noSearch);
      dlg.chooseConnection(c.connection);
      if (!c.port.isEmpty())
      {
        dlg.setPort(c.port);
        dlg.next();
      }
      if (!c.keys.isEmpty())
        dlg.settings()->load(c.keys);
      dlg.next();
      check(dlg.page() == AddDeviceDlg::TargetPage, c.shot + ": page 4");
      const QString info = dlg.findChild<QLabel *>("ui_deviceInfo")->text();
      check(info.contains(">" + c.type + "<") && info.contains(c.connectionText), c.shot + ": info " + info);
      check(!info.contains("00112233445566778899aabbccddeeff") && !info.contains("SERIAL "), c.shot + ": no key, no SERIAL: " + info);
      if (c.connection == AddDeviceDlg::Simulated)
        check(!info.contains("Sigrok"), "simulated: no internal protocol name: " + info);
      check(drawn(dlg.findChild<QLabel *>("ui_deviceSymbol")->pixmap()) > 2000, c.shot + ": a picture");
      if (!shots.isEmpty())
      {
        dlg.resize(660, 560);
        dlg.show();
        QApplication::processEvents();
        dlg.grab().save(QDir(shots).filePath("page4_" + c.shot + ".png"));
      }
    }
  }

  // 2b. page 4 always: Save only keeps it in My devices without connecting
  {
    AddDeviceDlg dlg(&library);
    dlg.setDiscoverers(noSearch);
    dlg.chooseConnection(AddDeviceDlg::Simulated);
    dlg.next();
    check(dlg.page() == AddDeviceDlg::TargetPage, "page 4 also in an empty window");
    QToolButton *keep = dlg.findChild<QToolButton *>("ui_saveOnly");
    check(keep && keep->text().contains("without connecting"), "Save only: says so");
    keep->click();
    check(dlg.result() == QDialog::Accepted && dlg.target() == AddDeviceDlg::SaveOnly, "Save only finishes");
  }

  // 3. the name follows the model until it is typed in
  {
    AddDeviceDlg dlg(&library);
    dlg.setDiscoverers(noSearch);
    dlg.chooseConnection(AddDeviceDlg::Bluetooth);
    check(dlg.page() == AddDeviceDlg::PortPage, "Bluetooth: page 2");
    check(!dlg.canGoNext(), "page 2 without a port: no Next");
    dlg.setPort("AA:BB:CC:DD:EE:FF");
    check(dlg.canGoNext(), "page 2 with an address typed in: Next");
    dlg.next();
    check(dlg.page() == AddDeviceDlg::DevicePage, "Bluetooth: page 3");
    dlg.settings()->load({ { "DMM/model", "Victron SmartShunt" } });
    check(dlg.name() == "Victron SmartShunt", "name follows the model: " + dlg.name());
    check(!dlg.canGoNext(), "Victron without address and key: no Next");
    dlg.settings()->load({ { "DMM/model", "Victron SmartShunt" }, { "Port settings/ble-address", "AA:BB:CC:DD:EE:FF" },
                           { "Port settings/ble-key", "00112233445566778899aabbccddeeff" } });
    check(dlg.canGoNext(), "Victron with address and key: Next");
    auto *name = dlg.findChild<QLineEdit *>("ui_name");
    name->setText("Battery");
    name->setModified(true);
    dlg.settings()->load({ { "DMM/model", "Victron SmartSolar MPPT" } });
    check(dlg.name() == "Battery", "a typed name stays: " + dlg.name());
    name->clear();
    check(!dlg.canGoNext(), "no name: no Next");
    dlg.back();
    check(dlg.page() == AddDeviceDlg::PortPage, "back from page 3 to page 2");
    dlg.back();
    check(dlg.page() == AddDeviceDlg::ConnectionPage, "back to page 1");
    dlg.reject();
    check(dlg.target() == AddDeviceDlg::NoTarget, "cancel: no target");
  }

  // 4. cable: the finds as cards, recognised ones first; a port typed in; Connect
  {
    AddDeviceDlg dlg(&library);
    dlg.setDiscoverers(noSearch);
    const QString hidPlace = DeviceLibrary::place({ { "Port settings/device", "HID 0x1a86:0xe008 /dev/hidraw3" } });
    dlg.setPlacesInUse({ { hidPlace, "In use by the instance u" } });
    dlg.chooseConnection(AddDeviceDlg::Cable);
    check(dlg.page() == AddDeviceDlg::PortPage, "cable: page 2");
    Candidate serial;
    serial.kind = Candidate::Serial;
    serial.key = "/dev/ttyUSB0";
    serial.title = "USB-serial adapter (CH340)";
    serial.keys.insert("Port settings/device", "SERIAL /dev/ttyUSB0");
    Candidate hid;
    hid.kind = Candidate::UsbCable;
    hid.key = "/dev/hidraw3";
    hid.title = "UNI-T, serial meters";
    hid.keys.insert("Port settings/device", "HID 0x1a86:0xe008 /dev/hidraw3");
    hid.models = QStringList { "Uni-Trend UT61E", "Uni-Trend UT803" };
    Candidate locked = serial;
    locked.key = "/dev/ttyUSB1";
    locked.keys.insert("Port settings/device", "SERIAL /dev/ttyUSB1");
    locked.problem = "/dev/ttyUSB1 belongs to the group dialout";
    dlg.addCandidate(serial);
    dlg.addCandidate(hid);
    dlg.addCandidate(locked);
    dlg.addCandidate(serial);   // the same find twice: one card
    check(dlg.candidates() == QStringList({ "/dev/hidraw3", "/dev/ttyUSB0", "/dev/ttyUSB1" }),
          "recognised first, each once: " + dlg.candidates().join(", "));
    auto *cards = dlg.findChild<QListWidget *>("ui_cards");
    check(cards->item(0)->text().contains("In use by the instance u"), "card: in use: " + cards->item(0)->text());
    dlg.chooseCandidate("/dev/ttyUSB1");
    check(!dlg.canGoNext(), "a port QtDMM may not open: no Next");
    dlg.chooseCandidate("/dev/hidraw3");
    check(dlg.findChild<QLineEdit *>("ui_port")->text() == "HID 0x1a86:0xe008 /dev/hidraw3", "card fills the port field");
    dlg.next();
    check(dlg.settings()->keys().value("DMM/model") == "Uni-Trend UT61E", "the likeliest model: "
          + dlg.settings()->keys().value("DMM/model").toString());
    check(dlg.name() == "Uni-Trend UT61E", "name after the model: " + dlg.name());
    check(dlg.knownDevice().isEmpty(), "a new device");
    dlg.back();
    dlg.setPort("/dev/ttyACM0");   // typed in
    dlg.next();
    check(dlg.keys().value("Port settings/device") == "SERIAL /dev/ttyACM0", "typed port: " + dlg.keys().value("Port settings/device").toString());
    check(dlg.name() == "Meter", "nothing known of the port: manual settings: " + dlg.name());
    dlg.next();
    QToolButton *connectNow = dlg.findChild<QToolButton *>("ui_connect");
    check(connectNow->text().contains("connect at once"), "Connect: into My devices and connect at once");
    connectNow->click();
    check(dlg.target() == AddDeviceDlg::Connect, "Connect finishes");
  }

  // 4b. a find that is one of My devices: that entry, no second one
  {
    const QString ut61e = library.add("Bench UT61E", { { "DMM/model", "Uni-Trend UT61E" },
                                                       { "Port settings/device", "HID 0x1a86:0xe008 /dev/hidraw2" } });
    AddDeviceDlg dlg(&library);
    dlg.setDiscoverers(noSearch);
    dlg.chooseConnection(AddDeviceDlg::Cable);
    Candidate hid;
    hid.kind = Candidate::UsbCable;
    hid.key = "/dev/hidraw5";   // plugged in again: another number, the same cable type
    hid.title = "UNI-T, serial meters";
    hid.keys.insert("Port settings/device", "HID 0x1a86:0xe008 /dev/hidraw5");
    hid.models = QStringList { "Uni-Trend UT803", "Uni-Trend UT61E" };
    dlg.addCandidate(hid);
    check(dlg.findChild<QListWidget *>("ui_cards")->item(0)->text().contains("Bench UT61E"), "card: already in My devices");
    dlg.chooseCandidate("/dev/hidraw5");
    dlg.next();
    check(dlg.knownDevice() == ut61e, "known find: the entry");
    check(dlg.name() == "Bench UT61E", "known find: its name: " + dlg.name());
    check(dlg.settings()->keys().value("DMM/model") == "Uni-Trend UT61E", "known find: its model, not the family's first");
    check(dlg.keys().value("Port settings/device").toString().endsWith("/dev/hidraw5"), "known find: the place found now");
    dlg.settings()->load({ { "DMM/model", "Uni-Trend UT803" }, { "Port settings/device", "HID 0x1a86:0xe008 /dev/hidraw5" } });
    check(dlg.knownDevice().isEmpty(), "another model at that place: a new device");
  }

  // 5. network: page 2 with the bridges found, or host:port typed in
  {
    AddDeviceDlg dlg(&library);
    dlg.setDiscoverers(noSearch);
    dlg.chooseConnection(AddDeviceDlg::Network);
    check(dlg.page() == AddDeviceDlg::PortPage, "network: page 2");
    Candidate bridge;
    bridge.kind = Candidate::Network;
    bridge.key = "192.168.1.20:4711";
    bridge.title = "qtdmm-bridge on dory";
    bridge.keys.insert("Port settings/device", "RFC2217 192.168.1.20:4711");
    dlg.addCandidate(bridge);
    dlg.chooseCandidate(bridge.key);
    check(dlg.findChild<QLineEdit *>("ui_port")->text() == "192.168.1.20:4711",
          "network: the bridge port without its type: " + dlg.findChild<QLineEdit *>("ui_port")->text());
    dlg.next();
    check(dlg.page() == AddDeviceDlg::DevicePage, "network: on to page 3");
    check(dlg.keys().value("Port settings/device") == "RFC2217 192.168.1.20:4711",
          "network: the bridge port: " + dlg.keys().value("Port settings/device").toString());
    dlg.back();
    check(dlg.page() == AddDeviceDlg::PortPage, "network: back to page 2");
    dlg.setPort("bench:4000");
    dlg.next();
    check(dlg.keys().value("Port settings/device") == "RFC2217 bench:4000",
          "network typed: " + dlg.keys().value("Port settings/device").toString());
  }

  // 6. a ZOTEK "Bluetooth DMM": all models share the name, the first packet
  //    tells which one it is (FrameProbe); no packet leaves the order
  {
    // a meter that streams one packet once it is opened
    class FakeMeter : public QIODevice
    {
    public:
      explicit FakeMeter(const QByteArray &data) : m_data(data) {}
      bool open(OpenMode mode) override
      {
        QIODevice::open(mode);
        if (!m_data.isEmpty())
          QTimer::singleShot(10, this, [this] { Q_EMIT readyRead(); });
        return true;
      }
      bool isSequential() const override { return true; }
      qint64 bytesAvailable() const override { return m_data.size() + QIODevice::bytesAvailable(); }
    protected:
      qint64 readData(char *data, qint64 max) override
      {
        const qint64 n = qMin<qint64>(max, m_data.size());
        memcpy(data, m_data.constData(), size_t(n));
        m_data.remove(0, int(n));
        return n;
      }
      qint64 writeData(const char *, qint64 len) override { return len; }
    private:
      QByteArray m_data;
    };
    const QByteArray type3 = QByteArray::fromHex("1b847041085c7d7f66fa3a");   // ZT-300AB: -12.34 V DC
    auto zotekFind = []
    {
      Candidate c;
      c.kind = Candidate::Bluetooth;
      c.key = "AA:BB:CC:DD:EE:03";
      c.title = "Bluetooth DMM";
      c.keys.insert("Port settings/ble-address", "AA:BB:CC:DD:EE:03 Bluetooth DMM");
      c.models = QStringList{ "ZOTEK ZT-5B *", "ZOTEK ZT-5BQ *", "ZOTEK ZT-300AB *", "ANENG AN9002 *" };
      c.probe = FrameFormat::ZotekBLE;
      return c;
    };
    auto waitFor = [](const std::function<bool()> &done)
    {
      QElapsedTimer t;
      t.start();
      while (!done() && t.elapsed() < 3000)
        QCoreApplication::processEvents(QEventLoop::AllEvents, 20);
    };
    {
      AddDeviceDlg dlg(&library);
      dlg.setDiscoverers(noSearch);
      int opened = 0;
      dlg.setProbePort([&](const Candidate &) { ++opened; return new FakeMeter(type3); });
      dlg.chooseConnection(AddDeviceDlg::Bluetooth);
      const Candidate c = zotekFind();
      dlg.addCandidate(c);
      dlg.chooseCandidate(c.key);
      waitFor([&] { return dlg.candidateModels(c.key).value(0) == "ZOTEK ZT-300AB *"; });
      check(dlg.candidateModels(c.key)
              == QStringList({ "ZOTEK ZT-300AB *", "ANENG AN9002 *", "ZOTEK ZT-5B *", "ZOTEK ZT-5BQ *" }),
            "ZOTEK: the type-3 packet puts the ZT-300AB first: " + dlg.candidateModels(c.key).join(", "));
      dlg.chooseCandidate(c.key);
      check(opened == 1, "ZOTEK: listened to once");
      dlg.next();
      check(dlg.keys().value("DMM/model") == "ZOTEK ZT-300AB *",
            "ZOTEK: page 3 has the model it named: " + dlg.keys().value("DMM/model").toString());
    }
    {
      AddDeviceDlg dlg(&library);
      dlg.setDiscoverers(noSearch);
      FakeMeter *silent = nullptr;
      QObject *probe = nullptr;
      dlg.setProbePort([&](const Candidate &) { return silent = new FakeMeter(QByteArray()); });
      dlg.chooseConnection(AddDeviceDlg::Bluetooth);
      const Candidate c = zotekFind();
      dlg.addCandidate(c);
      dlg.chooseCandidate(c.key);
      probe = dlg.findChild<FrameProbe *>();
      check(probe && silent && silent->isOpen(), "ZOTEK: the meter is listened to");
      dlg.next();   // on before it said anything: the probe ends, the order stays
      check(!dlg.findChild<FrameProbe *>(), "ZOTEK: the probe ends with Next");
      check(dlg.keys().value("DMM/model") == "ZOTEK ZT-5B *",
            "ZOTEK silent: the first model as found: " + dlg.keys().value("DMM/model").toString());
    }
  }

  // 7. Bluetooth not usable: its tile off, the tooltip says why; a search
  //    that fails says so instead of "nothing found"
  {
    AddDeviceDlg dlg(&library);
    dlg.setDiscoverers([](AddDeviceDlg::Connection) { return QList<Discoverer *>(); });
    auto *tile = dlg.findChild<QToolButton *>(QString("ui_connection%1").arg(int(AddDeviceDlg::Bluetooth)));
    dlg.setBluetoothCheck([] { return QString("Bluetooth is switched off."); });
    check(tile && !tile->isEnabled() && tile->toolTip().contains("switched off"),
          "Bluetooth off: the tile off, why in its tooltip: " + (tile ? tile->toolTip() : QString()));
    dlg.setBluetoothCheck([] { return QString(); });
    check(tile && tile->isEnabled() && !tile->toolTip().contains("cannot"), "Bluetooth usable: the tile on");

    class Failing : public Discoverer
    {
    public:
      void start() override
      {
        m_error = "Input/output error";
        QTimer::singleShot(0, this, &Discoverer::finished);
      }
    };
    dlg.setDiscoverers([](AddDeviceDlg::Connection) { return QList<Discoverer *>{ new Failing }; });
    dlg.chooseConnection(AddDeviceDlg::Bluetooth);
    QApplication::processEvents();
    QString shown;
    for (QLabel *l : dlg.findChildren<QLabel *>())
      if (l->text().contains("search failed"))
        shown = l->text();
    check(shown.contains("Input/output error"), "a failed search says why: " + shown);
  }

  if (failed)
    qWarning() << failed << "check(s) failed";
  else
    qInfo() << "all checks passed";
  return failed ? 1 : 0;
}
