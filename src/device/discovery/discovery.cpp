// Copyright (c) 2026 The QtDMM developers
// SPDX-License-Identifier: GPL-3.0-or-later
#include "device/discovery/discovery.h"

#include <QCoreApplication>
#include <QFileInfo>
#include <QSerialPortInfo>
#include <algorithm>

#include "device/dmmdecoder.h"
#include "device/framereader.h"
#include "device/transports/hidpath.h"
#include "device/transports/hidreader.h"   // hidapi
#include "device/transports/serial.h"
#include "service/mdnsbrowser.h"

#ifdef QTDMM_WITH_BLE
#include <QBluetoothDeviceDiscoveryAgent>
#include <QBluetoothLocalDevice>
#include "device/transports/ble.h"
#include "device/transports/blegatt.h"
#include "device/victronble.h"

#include <QLoggingCategory>
Q_DECLARE_LOGGING_CATEGORY(lcBle)
#endif

namespace
{
QString ids(quint16 vid, quint16 pid)
{
  return QString("%1:%2").arg(vid, 4, 16, QLatin1Char('0')).arg(pid, 4, 16, QLatin1Char('0'));
}

// the likeliest first: the model most people have with that cable
QStringList likeliestFirst(QStringList models, const QString &first)
{
  std::sort(models.begin(), models.end(), [](const QString &a, const QString &b)
  {
    return a.localeAwareCompare(b) < 0;
  });
  const int i = int(models.indexOf(first));
  if (i > 0)
    models.move(i, 0);
  return models;
}
}

// ---------------------------------------------------------------- families

QStringList Families::models(FrameFormat::DataFormat format, bool serial)
{
  QStringList out;
  for (const DmmDecoder::DMMInfo &m : DmmDecoder::getDeviceConfigurations())
    if ((format == FrameFormat::Invalid || m.protocol == format) && (m.baud > 0) == serial)
      out << m.name;
  return likeliestFirst(out, QString());
}

QStringList Families::vendorModels(const QString &vendor)
{
  QStringList out;
  for (const DmmDecoder::DMMInfo &m : DmmDecoder::getDeviceConfigurations())
    if (m.vendor == vendor && m.baud > 0)
      out << m.name;
  return likeliestFirst(out, QString());
}

QStringList Families::preferByName(QStringList models, const QString &deviceName)
{
  // the model the device names itself ("BENNING MM10-1" is "BENNING MM 10-1 *",
  // "KPS DMM9000BT" the "(Bluetooth)" entry), else the first word
  auto plain = [](QString name)
  {
    name.remove(QLatin1String(" (Bluetooth)"));
    if (name.endsWith(QLatin1String(" *")))
      name.chop(2);
    return name.remove(' ').toLower();
  };
  for (int i = 0; i < models.size(); ++i)
    if (!deviceName.isEmpty() && plain(models[i]) == plain(deviceName))
    {
      models.move(i, 0);
      return models;
    }
  const QString word = deviceName.section(' ', 0, 0);
  if (word.isEmpty())
    return models;
  for (int i = 0; i < models.size(); ++i)
    if (models[i].section(' ', 1, 1).startsWith(word, Qt::CaseInsensitive))
    {
      models.move(i, 0);
      break;
    }
  return models;
}

std::optional<Families::Family> Families::forUsb(quint16 vid, quint16 pid)
{
  const QString id = ids(vid, pid);
  if (id == "10c4:ea80" || id == "1a86:e429")
  {
    // the UT-D09 cable: the protocol of the UT61+ series, or the UT181A's
    QStringList models;
    for (const QString &m : Families::models(FrameFormat::UniTUT61Plus, true) + Families::models(FrameFormat::UniTUT181A, true))
      models << m;
    return Family { QCoreApplication::translate("Discovery", "UNI-T UT61B+, UT61D+, UT61E+, UT161 or UT181A"), "UT-D09",
                    QCoreApplication::translate("Discovery", "Switch the meter on; it sends as soon as the cable is plugged in. "
                                                                  "A UT181A only with SETUP > Communication ON, again after every power-on."), "HID",
                    likeliestFirst(models, "Uni-Trend UT61E+ *") };
  }
  if (id == "1a86:e008" || id == "04fa:2490")
  {
    // the UT-D04 cable: an optical serial link, many UNI-T meters
    QStringList models;
    for (const DmmDecoder::DMMInfo &m : DmmDecoder::getDeviceConfigurations())
      if (m.vendor == "Uni-Trend" && m.baud > 0 && m.protocol != FrameFormat::UniTUT61Plus)
        models << m.name;
    return Family { QCoreApplication::translate("Discovery", "UNI-T meter with the UT-D04 cable"), "UT-D04",
                    QCoreApplication::translate("Discovery", "Press RS232 (or PC) on the meter, else it sends nothing."), "HID",
                    likeliestFirst(models, "Uni-Trend UT61E") };
  }
  if (id == "0820:0001")
  {
    QStringList models;
    for (const DmmDecoder::DMMInfo &m : DmmDecoder::getDeviceConfigurations())
      if (m.vendor == "Brymen" && m.baud == 0)
        models << m.name;
    return Family { QCoreApplication::translate("Discovery", "Brymen BM52x, BM82x or BM86x"), "BU-86X",
                    QCoreApplication::translate("Discovery", "Put the IR adapter on the meter and switch its PC link on."), "HID", likeliestFirst(models, QString()) };
  }
  if (id == "04d9:a052")
    return Family { QCoreApplication::translate("Discovery", "TFA AIRCO2NTROL CO2 monitor"), QString(), QString(), "HIDHOLTEK",
                    likeliestFirst(Families::models(FrameFormat::TfaAirControl, false), "TFA Dostmann AIRCO2NTROL Mini *") };
  return std::nullopt;
}

QString Families::adapter(quint16 vid, quint16 pid)
{
  if (vid == 0x0403)
    return "FTDI";
  if (vid == 0x1a86 && (pid == 0x7523 || pid == 0x5523 || pid == 0x7522))
    return "CH340";
  if ((vid == 0x067b) || (vid == 0x0557 && pid == 0x2008))
    return "PL2303";
  if (vid == 0x10c4 && (pid == 0xea60 || pid == 0xea70 || pid == 0xea71))
    return "CP210x";
  return QString();
}

// ---------------------------------------------------------------- access

QString udevRules()
{
  QString rules = "# QtDMM: the logged-in user may open the USB-HID cables and sensors of meters,\n"
                  "# and each gets a link that stays the same across plugging in again and\n"
                  "# restarts as long as it is in the same USB port:\n"
                  "# /dev/qtdmm/<cable>-<USB port>, e.g. /dev/qtdmm/ut-d04-ch9325-pci-0000_00_14_0-usb-0_2_1_0\n";
  for (const auto &[vid, pid, what, name] :
       { std::tuple { "1a86", "e008", "WCH CH9325 (UNI-T UT-D04)", "ut-d04-ch9325" },
         std::tuple { "04fa", "2490", "Hoitek HE2325U (UNI-T UT-D04)", "ut-d04-he2325u" },
         std::tuple { "10c4", "ea80", "SiLabs CP2110 (UNI-T UT-D09)", "ut-d09-cp2110" },
         std::tuple { "1a86", "e429", "WCH CH9329 (UNI-T UT-D09)", "ut-d09-ch9329" },
         std::tuple { "0820", "0001", "Brymen BU-86X", "brymen-bu86x" },
         std::tuple { "04d9", "a052", "Holtek (TFA AIRCO2NTROL)", "holtek-co2" } })
    rules += QString("# %3\nKERNEL==\"hidraw*\", ATTRS{idVendor}==\"%1\", ATTRS{idProduct}==\"%2\", TAG+=\"uaccess\", "
                     "ENV{QTDMM_CABLE}=\"%4\"\n")
               .arg(vid, pid, what, name);
  // the stable link (HidPath finds the cable without it, too, when it is
  // the only one of its kind)
  rules += "\n# the stable link: the cable's name and its USB port\n"
           "KERNEL==\"hidraw*\", ENV{QTDMM_CABLE}==\"?*\", IMPORT{builtin}=\"path_id\"\n"
           "KERNEL==\"hidraw*\", ENV{QTDMM_CABLE}==\"?*\", ENV{ID_PATH_TAG}==\"?*\", "
           "SYMLINK+=\"qtdmm/$env{QTDMM_CABLE}-$env{ID_PATH_TAG}\"\n";
  return rules;
}

AccessProblem accessProblem(const QString &path, bool hid, quint16 vid, quint16 pid)
{
  AccessProblem a;
#ifdef Q_OS_LINUX
  const QFileInfo info(path);
  if (!info.exists() || (info.isReadable() && info.isWritable()))
    return a;
  a.problem = QCoreApplication::translate("Discovery", "No access: %1 belongs to %2:%3").arg(path, info.owner(), info.group());
  if (hid)
  {
    const QString rule = QString("KERNEL==\"hidraw*\", ATTRS{idVendor}==\"%1\", ATTRS{idProduct}==\"%2\", TAG+=\"uaccess\"")
                           .arg(vid, 4, 16, QLatin1Char('0')).arg(pid, 4, 16, QLatin1Char('0'));
    a.fix = QCoreApplication::translate("Discovery", "A udev rule lets you open the cable. The packages of QtDMM install one; for the AppImage or "
               "Flatpak, put this line into /etc/udev/rules.d/70-qtdmm.rules:\n\n%1\n\n"
               "then load it and plug the cable in again:\n\n"
               "sudo udevadm control --reload-rules && sudo udevadm trigger").arg(rule);
  }
  else
  {
    const QString group = info.group().isEmpty() ? QStringLiteral("dialout") : info.group();
    a.fix = QCoreApplication::translate("Discovery", "Serial ports belong to the group %1. Add yourself to it:\n\n"
               "sudo usermod -aG %1 $USER\n\n"
               "then log out and in again.").arg(group);
  }
#else
  Q_UNUSED(path) Q_UNUSED(hid) Q_UNUSED(vid) Q_UNUSED(pid)
#endif
  return a;
}

// ---------------------------------------------------------------- USB-HID

void UsbDiscoverer::start()
{
  // hidapi only enumerates (no device is opened): quick, but after start()
  // returned, so the caller has its connections in place
  QTimer::singleShot(0, this, [this]
  {
    struct hid_device_info *devs = hid_enumerate(0, 0);
    for (struct hid_device_info *d = devs; d; d = d->next)
    {
      const std::optional<Families::Family> family = Families::forUsb(d->vendor_id, d->product_id);
      if (!family)
        continue;
      const QString path = QString::fromLatin1(d->path);
      // the udev link (/dev/qtdmm/...) when there is one: it stays across plugs
      const QString link = HidPath::linkFor(path);
      const QString place = link.isEmpty() ? path : link;
      Candidate c;
      c.kind = Candidate::UsbCable;
      c.key = path;
      c.title = family->title;
      c.detail = family->cable.isEmpty() ? place : QCoreApplication::translate("Discovery", "%1 cable · %2").arg(family->cable, place);
      c.hint = family->hint;
      c.models = family->models;
      c.keys.insert("Port settings/device", QString("%1 0x%2:0x%3 %4").arg(family->portType)
                                              .arg(d->vendor_id, 4, 16, QLatin1Char('0'))
                                              .arg(d->product_id, 4, 16, QLatin1Char('0')).arg(place));
      const AccessProblem a = accessProblem(path, true, d->vendor_id, d->product_id);
      c.problem = a.problem;
      c.fix = a.fix;
      Q_EMIT found(c);
    }
    hid_free_enumeration(devs);
    Q_EMIT finished();
  });
}

// ---------------------------------------------------------------- serial

void SerialDiscoverer::start()
{
  QTimer::singleShot(0, this, [this]
  {
    for (const QSerialPortInfo &p : QSerialPortInfo::availablePorts())
    {
      // the kernel lists ttyS0..31 whether or not there is a UART: only USB
      // adapters (they say who made them) and named ports count here
      if (!p.hasVendorIdentifier() && p.description().isEmpty())
        continue;
#ifdef Q_OS_WIN
      const QString port = p.portName();
#else
      const QString port = p.systemLocation();
#endif
      Candidate c;
      c.kind = Candidate::Serial;
      c.key = port;
      const QString chip = p.hasVendorIdentifier() ? Families::adapter(p.vendorIdentifier(), p.productIdentifier())
                                                   : QString();
      c.title = chip.isEmpty() ? QCoreApplication::translate("Discovery", "Serial port") : QCoreApplication::translate("Discovery", "USB-serial adapter (%1)").arg(chip);
      c.detail = p.manufacturer().isEmpty() ? port : QString("%1 · %2").arg(p.manufacturer(), port);
      c.hint = QCoreApplication::translate("Discovery", "Which meter is on it, QtDMM cannot tell yet: choose the model.");
      c.keys.insert("Port settings/device", SerialDevice::stableDevice("SERIAL " + port));   // by-id, as saved
      const AccessProblem a = accessProblem(port, false);
      c.problem = a.problem;
      c.fix = a.fix;
      Q_EMIT found(c);
    }
    Q_EMIT finished();
  });
}

// ---------------------------------------------------------------- network

void BridgeDiscoverer::start()
{
  m_browser = new MdnsBrowser(this);
  connect(m_browser, &MdnsBrowser::found, this, [this](const MdnsBrowser::Service &s)
  {
    const QString where = s.address.isNull() ? s.host : s.address.toString();
    QString host = s.host;
    if (host.endsWith(".local"))
      host.chop(6);
    const QString device = s.txt.value("name", s.txt.value("device"));
    Candidate c;
    c.kind = Candidate::Network;
    c.key = QString("%1:%2").arg(where).arg(s.port);
    c.title = QCoreApplication::translate("Discovery", "qtdmm-bridge on %1").arg(host);
    c.detail = device.isEmpty() ? c.key : QString("%1 · %2").arg(device, c.key);
    c.hint = QCoreApplication::translate("Discovery", "Which meter is on it, QtDMM cannot tell yet: choose the model.");
    c.keys.insert("Port settings/device", "RFC2217 " + c.key);
    Q_EMIT found(c);
  });
  connect(m_browser, &MdnsBrowser::finished, this, &Discoverer::finished);
  m_browser->browse("_qtdmm-bridge._tcp.local", 3000);
}

void BridgeDiscoverer::stop()
{
  if (m_browser && m_browser->isActive())
    m_browser->stop();
}

// ---------------------------------------------------------------- Bluetooth

struct BleDiscoverer::Private
{
#ifdef QTDMM_WITH_BLE
  QBluetoothDeviceDiscoveryAgent *agent = nullptr;
  QSet<QString> seen;
  QHash<QString, QString> logged;   ///< what the debug log said of each address last
#endif
};

BleDiscoverer::BleDiscoverer(QObject *parent) :
  Discoverer(parent),
  d(new Private)
{
}

BleDiscoverer::~BleDiscoverer()
{
  delete d;
}

QString BleDiscoverer::unavailable()
{
#ifdef QTDMM_WITH_BLE
  // on Linux the adapters come from BlueZ: none also when bluetoothd is not running
  const QList<QBluetoothHostInfo> hosts = QBluetoothLocalDevice::allDevices();
  if (hosts.isEmpty())
    return QCoreApplication::translate("Discovery", "No Bluetooth adapter found (or the Bluetooth service is not running).");
  for (const QBluetoothHostInfo &host : hosts)
    if (QBluetoothLocalDevice(host.address()).hostMode() != QBluetoothLocalDevice::HostPoweredOff)
      return QString();
  return QCoreApplication::translate("Discovery", "Bluetooth is switched off.");
#else
  return QCoreApplication::translate("Discovery", "This build of QtDMM has no Bluetooth.");
#endif
}

void BleDiscoverer::start()
{
#ifdef QTDMM_WITH_BLE
  // one scan for everything: two agents at once get in each other's way
  d->agent = new QBluetoothDeviceDiscoveryAgent(this);
  // long enough for meters that advertise seldom (the HP-570C-APP showed
  // after up to 15 s in a user's bluetoothctl scans)
  d->agent->setLowEnergyDiscoveryTimeout(20000);
  auto take = [this](const QBluetoothDeviceInfo &info)
  {
    const QString address = info.address().toString().toUpper();
    if (d->seen.contains(address))
      return;
    // every device as Qt sees it, when it changes (QT_LOGGING_RULES
    // "qtdmm.ble.debug=true"): why a meter is not offered
    if (lcBle().isDebugEnabled())
    {
      QStringList uuids, companies;
      for (const QBluetoothUuid &u : info.serviceUuids())
        uuids << u.toString(QUuid::WithoutBraces);
      for (quint16 id : info.manufacturerIds())
        companies << QString("0x%1: %2").arg(id, 4, 16, QChar('0')).arg(QString(info.manufacturerData(id).toHex(' ')));
      const QString line = QString("name \"%1\" services [%2] manufacturer data [%3]")
                             .arg(info.name(), uuids.join(", "), companies.join("; "));
      if (d->logged.value(address) != line)
      {
        d->logged.insert(address, line);
        qCDebug(lcBle).noquote() << "scan:" << address << line;
      }
    }
    Candidate c;
    c.kind = Candidate::Bluetooth;
    c.key = address;
    if (info.manufacturerIds().contains(VictronBle::CompanyId))
    {
      c.title = info.name().isEmpty() ? QCoreApplication::translate("Discovery", "Victron device") : QCoreApplication::translate("Discovery", "Victron %1").arg(info.name());
      c.hint = QCoreApplication::translate("Discovery", "The device key is in the VictronConnect app: Product info, Instant readout.");
      c.models = Families::preferByName(Families::models(FrameFormat::VictronBLE, false), info.name());
    }
    else
    {
      // the GATT meters, by service or name
      for (int f = 0; f < FrameFormat::EndOfList; ++f)
      {
        const auto profile = BleGattDevice::profile(FrameFormat::DataFormat(f));
        if (!profile)
          continue;
        if (profile->matches(info.name(), info.serviceUuids(), info.manufacturerIds()))
          c.models += Families::models(FrameFormat::DataFormat(f), false);
      }
      c.models = Families::preferByName(c.models, info.name());
      if (c.models.isEmpty())
        return;
      // several meters of one name whose frames tell them apart (ZOTEK's
      // "Bluetooth DMM"): the first frame chooses
      FrameFormat::DataFormat format = FrameFormat::Invalid;
      for (const DmmDecoder::DMMInfo &m : DmmDecoder::getDeviceConfigurations())
        if (m.name == c.models.first())
          format = m.protocol;
      const auto decoder = DmmDecoder::getInstance(format);
      if (c.models.size() > 1 && decoder && decoder->tellsModel())
        c.probe = format;
      c.title = info.name().isEmpty() ? QCoreApplication::translate("Discovery", "Bluetooth meter") : info.name();
      c.hint = QCoreApplication::translate("Discovery", "Switch Bluetooth on at the meter.");
    }
    d->seen.insert(address);
    c.detail = info.rssi() != 0 ? QString("%1 · %2 dBm").arg(address).arg(info.rssi()) : address;
    c.keys.insert("Port settings/ble-address", QString("%1 %2").arg(address, info.name()).trimmed());
    Q_EMIT found(c);
  };
  connect(d->agent, &QBluetoothDeviceDiscoveryAgent::deviceDiscovered, this, take);
  connect(d->agent, &QBluetoothDeviceDiscoveryAgent::deviceUpdated, this,
          [take](const QBluetoothDeviceInfo &info, QBluetoothDeviceInfo::Fields) { take(info); });
  connect(d->agent, &QBluetoothDeviceDiscoveryAgent::finished, this, &Discoverer::finished);
  connect(d->agent, &QBluetoothDeviceDiscoveryAgent::canceled, this, &Discoverer::finished);
  connect(d->agent, &QBluetoothDeviceDiscoveryAgent::errorOccurred, this, [this]
  {
    // said, not only a search that found nothing
    m_error = d->agent->errorString();
    qCWarning(lcBle) << "search failed:" << m_error;
    Q_EMIT finished();
  });
  d->agent->start(QBluetoothDeviceDiscoveryAgent::LowEnergyMethod);
  // the search for all kinds: with the helper while a protocol wants it
  // (HP-570C-APP), it cannot know which meter is looked for
  for (int f = 0; f < FrameFormat::EndOfList; ++f)
    if (const auto p = BleGattDevice::profile(FrameFormat::DataFormat(f)); p && p->helperScan)
    {
      bleHelperScan(d->agent, d->agent->lowEnergyDiscoveryTimeout());
      break;
    }
#else
  QTimer::singleShot(0, this, &Discoverer::finished);
#endif
}

void BleDiscoverer::stop()
{
#ifdef QTDMM_WITH_BLE
  if (d->agent && d->agent->isActive())
    d->agent->stop();
#endif
}

// ---------------------------------------------------------------- probe

FrameProbe::FrameProbe(QIODevice *port, FrameFormat::DataFormat format, QObject *parent) :
  QObject(parent),
  m_port(port),
  m_decoder(DmmDecoder::getInstance(format))
{
  if (m_port)
    m_port->setParent(this);
  m_timeout.setSingleShot(true);
  connect(&m_timeout, &QTimer::timeout, this, [this] { finish({}); });
}

FrameProbe::~FrameProbe()
{
  if (m_reader)
    m_reader->setHandle(nullptr);
  if (m_port)
    m_port->close();
}

void FrameProbe::start(int timeoutMs)
{
  if (!m_port || !m_decoder || (!m_port->isOpen() && !m_port->open(QIODevice::ReadWrite)))
  {
    QTimer::singleShot(0, this, [this] { finish({}); });
    return;
  }
#ifdef QTDMM_WITH_BLE
  if (auto *gatt = qobject_cast<BleGattDevice *>(m_port))
  {
    connect(gatt, &BleGattDevice::finished, this, [this] { finish({}); });
    // a meter whose device information tells the model (OWON) tells it
    // before the first frame
    connect(gatt, &BleGattDevice::infoRead, this, [this](const QByteArray &info)
    {
      const QStringList models = m_decoder->modelsFromInfo(info);
      if (!models.isEmpty())
        finish(models);
    });
  }
#endif
  m_reader = new FrameReader(this);
  m_reader->setDecoder(m_decoder);
  m_reader->setHandle(m_port);
  connect(m_reader, &FrameReader::readEvent, this, [this](const QByteArray &frame)
  {
    finish(m_decoder->modelsFromFrame(frame));
  });
  m_timeout.start(timeoutMs);
}

void FrameProbe::finish(const QStringList &models)
{
  if (m_done)
    return;
  m_done = true;
  m_timeout.stop();
  Q_EMIT finished(models);
}

QIODevice *FrameProbe::blePort(const Candidate &candidate)
{
#ifdef QTDMM_WITH_BLE
  for (const DmmDecoder::DMMInfo &m : DmmDecoder::getDeviceConfigurations())
    if (!candidate.models.isEmpty() && m.name == candidate.models.first())
      return new BleGattDevice(m, candidate.keys.value("Port settings/ble-address").toString());
#else
  Q_UNUSED(candidate);
#endif
  return nullptr;
}
