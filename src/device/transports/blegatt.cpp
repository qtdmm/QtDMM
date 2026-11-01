// Copyright (c) 2026 The QtDMM developers
// SPDX-License-Identifier: GPL-3.0-or-later
#include "device/transports/blegatt.h"
#include "device/transports/ble.h"

#include <QBluetoothAddress>
#include <QBluetoothDeviceDiscoveryAgent>
#include <QBluetoothDeviceInfo>
#include <QCoreApplication>
#include <QElapsedTimer>
#include <QLoggingCategory>
#include <QLowEnergyController>
#include <QLowEnergyDescriptor>
#include <QLowEnergyService>
#include <QSet>
#include <QTimer>

Q_DECLARE_LOGGING_CATEGORY(lcBle)

namespace
{
// Microchip/ISSC "Transparent UART" (BM70/RN4870 modules): UNI-T UT60BT,
// UT161 and the UT-D07B adapter
const QBluetoothUuid kIsscService(QStringLiteral("49535343-fe7d-4ae5-8fa9-9fafd205e455"));
const QBluetoothUuid kIsscNotify(QStringLiteral("49535343-1e4d-4bd9-ba61-23c647249616"));
const QBluetoothUuid kIsscWrite(QStringLiteral("49535343-8841-43f4-a8d4-ecbe34729bb3"));
const QBluetoothUuid kIsscWriteFallback(QStringLiteral("49535343-6daa-4d02-abf6-19569aca69fe"));

// UNI-T's iENV meters (UT383BT, UT353BT): a vendor service with 16-bit UUIDs
const QBluetoothUuid kIenvService(quint16(0xFF12));
const QBluetoothUuid kIenvNotify(quint16(0xFF02));
const QBluetoothUuid kIenvWrite(quint16(0xFF01));

// APPA's Bluetooth meters (S0-S3, 150B, 170B, A17N, sFlex, 208B, 506B) and
// their BENNING, KPS, MEGGER, METRAVI, RS PRO and Sefram twins: the 0xFFF0
// service of many BLE UART modules (libsigrok serial_bt.c,
// SER_BT_CONN_APPADMM: notify 0xFFF1, write 0xFFF2)
const QBluetoothUuid kFff0Service(quint16(0xFFF0));
const QBluetoothUuid kFff0Notify(quint16(0xFFF1));
const QBluetoothUuid kFff0Write(quint16(0xFFF2));
// ZOTEK's meters (ZOYI, BSIDE, ANENG): the same service, notify and write on
// 0xFFF4 (dmm-tools, ZOTEK spec §2, after ZOTEK's apps)
const QBluetoothUuid kFff4(quint16(0xFFF4));
// OWON's meters: notify on 0xFFF4, keys to 0xFFF3, the device information
// (model code, firmware) in 0xFFF2 (dmm-tools, OWON spec §2, §4)
const QBluetoothUuid kFff2(quint16(0xFFF2));
const QBluetoothUuid kFff3(quint16(0xFFF3));

// AiCare's module in the HoldPeak HP-570C-APP clamp: the FS9721 frame on
// 0xFFB2 of service 0xFFB0; 0xFFB1 is never written. It advertises the
// service and manufacturer data of company 0xFFAC (bytes AC FF, its address
// reversed at the end), which AiCare's app checks both (AiCare's app)
const QBluetoothUuid kFfb0Service(quint16(0xFFB0));
const QBluetoothUuid kFfb0Notify(quint16(0xFFB2));
const QBluetoothUuid kFfb0Write(quint16(0xFFB1));
constexpr quint16 kAicareCompany = 0xFFAC;

// connecting and finding the characteristics takes a second or two; a
// meter that is switched off or out of range never answers
constexpr int kConnectTimeoutMs = 20000;
// connected, but the service not set up by then: give up, the reconnect
// after a loss comes sooner (a UT60BT now and then stays in the discovery
// of the service details)
constexpr int kSetupTimeoutMs = 10000;
// the scan for the meter before connecting: most meters show within a second
// or two, the HP-570C-APP now and then only after 15 s (a user's bluetoothctl
// scans); BlueZ connects only to a device it has seen lately, so the scan has
// to wait for it. It stops as soon as the meter is seen
constexpr int kFindMs = 30000;
}

std::optional<BleGattDevice::Profile> BleGattDevice::profile(FrameFormat::DataFormat format)
{
  switch (format)
  {
    case FrameFormat::UniTiDMM:
      return Profile{ kIsscService, kIsscNotify, { kIsscWrite, kIsscWriteFallback }, { "UT60BT" } };
    case FrameFormat::UniTUT61Plus:   // UT161 built in; UT61B+/D+/E+ through the UT-D07B adapter
    case FrameFormat::UniTUT181A:     // through the UT-D07B adapter
      return Profile{ kIsscService, kIsscNotify, { kIsscWrite, kIsscWriteFallback }, { "UT161", "UT-D07" } };
    case FrameFormat::UniTiENV:
      // the UT353BT advertises as "UT353BT" (esphome_uni_trend_sound_meter);
      // the UT383BT's name is assumed alike
      return Profile{ kIenvService, kIenvNotify, { kIenvWrite }, { "UT383BT", "UT353BT" } };
    case FrameFormat::UniTUT202BT:
      // UNI-T's app asks the name (0x5F) before anything else, and a UT60BT
      // is reported to need it (dmm-tools, UT61 family spec §6.4; our UT60BT
      // answers the poll without). UNVERIFIED on the UT202BT: whether it
      // answers the 0x5E poll at all, or only streams after 0x5D as the app
      // has it
      return Profile{ kIsscService, kIsscNotify, { kIsscWrite, kIsscWriteFallback }, { "UT202BT" },
                      QByteArray::fromHex("abcd035f01da") };
    case FrameFormat::AppaDmm:
      // the names the meters advertise, from libsigrok's scan list; ZOTEK and
      // OWON meters have the same service, so the name decides
      return Profile{ kFff0Service, kFff0Notify, { kFff0Write },
                      { "APPA ", "BENNING ", "KPS DMM", "KPS DCM", "MEGGER DCM", "MEGGER DPM", "METRAVI PRO",
                        "RS PRO ", "Sefram 72", "Sefram 7352B", "Sefram MW35" },
                      QByteArray(), true };
    case FrameFormat::ZotekBLE:
      // every model advertises as "Bluetooth DMM" (ZOTEK's apps also take
      // "ZY", seen on no meter); the first packet tells which one it is
      return Profile{ kFff0Service, kFff4, { kFff4 }, { "Bluetooth DMM" }, QByteArray(), true };
    case FrameFormat::VC820BLE:
      // AiCare's app finds the meter by the service and the manufacturer
      // data it advertises; a user's meter advertises as "FS9721-LP3" (the
      // chip's name, bluetoothctl)
      // Qt's search hears it on some machines only while bluetoothctl scans
      return Profile{ kFfb0Service, kFfb0Notify, { kFfb0Write }, { "FS9721" }, QByteArray(), false, true, kAicareCompany };
    case FrameFormat::OwonBLE:
    case FrameFormat::OwonBLE15:
      // "BDM" unless the user renamed the meter; the model is in 0xFFF2
      return Profile{ kFff0Service, kFff4, { kFff3 }, { "BDM" }, QByteArray(), true, false, 0, kFff2 };
    default:
      return std::nullopt;
  }
}

bool BleGattDevice::Profile::matches(const QString &name, const QList<QBluetoothUuid> &services,
                                     const QList<quint16> &manufacturers) const
{
  if (!byNameOnly && services.contains(service))
    return true;
  if (manufacturer != 0 && manufacturers.contains(manufacturer))
    return true;
  for (const QString &prefix : namePrefixes)
    if (name.startsWith(prefix, Qt::CaseInsensitive))
      return true;
  return false;
}

BleGattDevice::BleGattDevice(const DmmDecoder::DMMInfo &info, const QString &device, QObject *parent)
  : BufferedDevice(parent)
  , m_address(device.simplified().section(' ', 0, 0).toUpper())
  , m_profile(profile(info.protocol))
{
}

BleGattDevice::~BleGattDevice()
{
  close();
}

bool BleGattDevice::open(OpenMode mode)
{
  if (QBluetoothAddress(m_address).isNull())
  {
    setErrorString(tr("No Bluetooth address configured."));
    return false;
  }
  if (!m_profile)
  {
    setErrorString(tr("This meter does not use a Bluetooth LE connection."));
    return false;
  }
  if (!lcBle().isDebugEnabled())
    QLoggingCategory::setFilterRules(QStringLiteral("qt.bluetooth.bluez.warning=false\nqt.bluetooth.bluez.info=false"));

  m_serviceFound = false;
  m_ready = false;

  m_connectTimeout = new QTimer(this);
  m_connectTimeout->setSingleShot(true);
  connect(m_connectTimeout, &QTimer::timeout, this, [this]
  {
    if (!m_ready)
      fail(tr("No answer from %1 - is the meter on and its Bluetooth switched on?").arg(m_address));
  });
  m_connectTimeout->start(kFindMs + kConnectTimeoutMs);   // connectTo() starts it anew

  m_repoll = new QTimer(this);
  m_repoll->setSingleShot(true);
  m_repoll->setInterval(kRepollMs);
  connect(m_repoll, &QTimer::timeout, this, &BleGattDevice::sendPoll);

  // look for the meter first: BlueZ connects only to a device it has seen
  // lately. Seen, it connects at once; not seen within kFindMs, the connect
  // is tried anyway (another stack, a meter BlueZ still knows)
  QBluetoothDeviceInfo bare(QBluetoothAddress(m_address), QString(), 0);
  bare.setCoreConfigurations(QBluetoothDeviceInfo::LowEnergyCoreConfiguration);
  m_finder = new QBluetoothDeviceDiscoveryAgent(this);
  m_finder->setLowEnergyDiscoveryTimeout(kFindMs);
  auto seen = [this](const QBluetoothDeviceInfo &info)
  {
    if (m_finder && !m_controller && info.address().toString().compare(m_address, Qt::CaseInsensitive) == 0)
    {
      qCDebug(lcBle) << m_address << "seen as" << info.name() << "rssi" << info.rssi();
      m_finder->stop();
      connectTo(info);
    }
  };
  connect(m_finder, &QBluetoothDeviceDiscoveryAgent::deviceDiscovered, this, seen);
  connect(m_finder, &QBluetoothDeviceDiscoveryAgent::deviceUpdated, this,
          [seen](const QBluetoothDeviceInfo &info, QBluetoothDeviceInfo::Fields) { seen(info); });
  auto notSeen = [this, bare]
  {
    if (!m_controller && isOpen())
    {
      qCDebug(lcBle) << m_address << "not seen in the scan, connecting anyway";
      connectTo(bare);
    }
  };
  connect(m_finder, &QBluetoothDeviceDiscoveryAgent::finished, this, notSeen);
  connect(m_finder, &QBluetoothDeviceDiscoveryAgent::errorOccurred, this, [this, notSeen]
  {
    // said always: the connect to the address alone works only when BlueZ
    // knows the meter already (a user's: only after bluetoothctl scan)
    qCWarning(lcBle) << m_address << "search failed:" << m_finder->errorString();
    notSeen();
  });
  qCDebug(lcBle) << m_address << "looking for the meter";
  m_finder->start(QBluetoothDeviceDiscoveryAgent::LowEnergyMethod);
  if (m_profile->helperScan)
    bleHelperScan(m_finder, kFindMs);
  return QIODevice::open(mode | QIODevice::Unbuffered);
}

void BleGattDevice::connectTo(const QBluetoothDeviceInfo &info)
{
  m_controller = QLowEnergyController::createCentral(info, this);
  // the connect has its own time, the scan took some of it
  if (m_connectTimeout)
    m_connectTimeout->start(kConnectTimeoutMs);

  connect(m_controller, &QLowEnergyController::connected, this, [this]
  {
    qCDebug(lcBle) << m_address << "connected, discovering services";
    if (m_connectTimeout)
      m_connectTimeout->start(kSetupTimeoutMs);
    m_controller->discoverServices();
  });
  connect(m_controller, &QLowEnergyController::serviceDiscovered, this, &BleGattDevice::onServiceDiscovered);
  connect(m_controller, &QLowEnergyController::discoveryFinished, this, &BleGattDevice::onDiscoveryFinished);
  connect(m_controller, &QLowEnergyController::disconnected, this, [this]
  {
    fail(tr("The meter %1 closed the Bluetooth connection.").arg(m_address));
  });
  connect(m_controller, &QLowEnergyController::errorOccurred, this, [this](QLowEnergyController::Error)
  {
    fail(tr("Bluetooth: %1").arg(m_controller->errorString()));
  });
  m_controller->connectToDevice();
}

void BleGattDevice::close()
{
  m_ready = false;
  m_lastPoll.clear();
  for (QTimer **t : { &m_connectTimeout, &m_repoll })
    if (*t)
    {
      (*t)->stop();
      (*t)->deleteLater();
      *t = nullptr;
    }
  if (m_finder)
  {
    m_finder->disconnect(this);
    m_finder->stop();
    m_finder->deleteLater();
    m_finder = nullptr;
  }
  if (m_service)
  {
    m_service->disconnect(this);
    m_service->deleteLater();
    m_service = nullptr;
  }
  if (m_controller)
  {
    m_controller->disconnect(this);
    m_controller->disconnectFromDevice();
    m_controller->deleteLater();
    m_controller = nullptr;
  }
  m_rx.clear();
  QIODevice::close();
}

void BleGattDevice::fail(const QString &reason)
{
  if (!isOpen())
    return;
  qCDebug(lcBle) << m_address << reason;
  setErrorString(reason);
  m_ready = false;
  Q_EMIT finished(reason);
}

void BleGattDevice::onServiceDiscovered(const QBluetoothUuid &uuid)
{
  if (uuid == m_profile->service)
    m_serviceFound = true;
}

void BleGattDevice::onDiscoveryFinished()
{
  qCDebug(lcBle) << m_address << "services discovered, own one" << m_serviceFound;
  if (!m_serviceFound)
  {
    fail(tr("%1 does not offer the expected Bluetooth service - is it the right meter?").arg(m_address));
    return;
  }
  m_service = m_controller->createServiceObject(m_profile->service, this);
  if (!m_service)
  {
    fail(tr("Could not open the Bluetooth service of %1.").arg(m_address));
    return;
  }
  connect(m_service, &QLowEnergyService::stateChanged, this, &BleGattDevice::onServiceState);
  connect(m_service, &QLowEnergyService::characteristicChanged, this,
          [this](const QLowEnergyCharacteristic &c, const QByteArray &value)
  {
    if (c.uuid() != m_profile->notify)
      return;
    qCDebug(lcBle) << m_address << "rx" << value.toHex(' ');
    m_rx += value;
    m_pollSent.invalidate();
    // the answer is in: ask again, but not faster than kRepollMs
    if (!m_lastPoll.isEmpty() && m_repoll && !m_repoll->isActive())
      m_repoll->start();
    Q_EMIT readyRead();
  });
  connect(m_service, &QLowEnergyService::errorOccurred, this, [this](QLowEnergyService::ServiceError e)
  {
    // a failed write of one poll is not the end of the link; the rest is
    if (e == QLowEnergyService::CharacteristicWriteError)
      qCDebug(lcBle) << m_address << "write failed";
    else
      fail(tr("Bluetooth service error %1 on %2.").arg(int(e)).arg(m_address));
  });
  m_service->discoverDetails();
}

void BleGattDevice::onServiceState()
{
  qCDebug(lcBle) << m_address << "service state" << m_service->state();
  if (m_service->state() != QLowEnergyService::RemoteServiceDiscovered)
    return;
  if (!m_profile->info.isNull())
  {
    // discoverDetails() has read the readable values already
    m_info = m_service->characteristic(m_profile->info).value();
    qCDebug(lcBle) << m_address << "info" << m_info.toHex(' ');
    if (!m_info.isEmpty())
      Q_EMIT infoRead(m_info);
  }
  const QLowEnergyCharacteristic notify = m_service->characteristic(m_profile->notify);
  if (!notify.isValid())
  {
    fail(tr("%1 has no notify characteristic.").arg(m_address));
    return;
  }
  for (const QBluetoothUuid &uuid : m_profile->write)
  {
    const QLowEnergyCharacteristic c = m_service->characteristic(uuid);
    if (c.isValid() && (c.properties() & (QLowEnergyCharacteristic::Write | QLowEnergyCharacteristic::WriteNoResponse)))
    {
      m_writeChar = c;
      break;
    }
  }
  if (!m_writeChar.isValid())
  {
    fail(tr("%1 has no writable characteristic.").arg(m_address));
    return;
  }
  const QLowEnergyDescriptor cccd = notify.clientCharacteristicConfiguration();
  if (!cccd.isValid())
  {
    setReady();   // notifies without a CCCD: nothing to switch on
    return;
  }
  // ready once the meter has confirmed the subscription - answers to polls
  // sent before would go nowhere (a failed write ends in errorOccurred)
  connect(m_service, &QLowEnergyService::descriptorWritten, this,
          [this](const QLowEnergyDescriptor &d, const QByteArray &)
  {
    if (d.type() == QBluetoothUuid::DescriptorType::ClientCharacteristicConfiguration && !m_ready)
      setReady();
  });
  qCDebug(lcBle) << m_address << "subscribing";
  m_service->writeDescriptor(cccd, QLowEnergyCharacteristic::CCCDEnableNotification);
}

void BleGattDevice::setReady()
{
  m_ready = true;
  if (m_connectTimeout)
    m_connectTimeout->stop();
  qCDebug(lcBle) << m_address << "ready";
  if (!m_profile->hello.isEmpty())
    sendCommand(m_profile->hello);
  sendPoll();   // the reader's poll from before is still waiting
}

qint64 BleGattDevice::writeData(const char *data, qint64 len)
{
  m_lastPoll = QByteArray(data, int(len));
  sendPoll();
  return len;
}

bool BleGattDevice::sendCommand(const QByteArray &frame)
{
  if (!m_ready || !m_service || frame.isEmpty())
    return false;
  qCDebug(lcBle) << m_address << "command" << frame.toHex(' ');
  const auto mode = (m_writeChar.properties() & QLowEnergyCharacteristic::WriteNoResponse)
                    ? QLowEnergyService::WriteWithoutResponse : QLowEnergyService::WriteWithResponse;
  m_service->writeCharacteristic(m_writeChar, frame, mode);
  // the polls wait for the meter to take the key, then go on
  m_keySent.start();
  QTimer::singleShot(kKeyQuietMs, this, &BleGattDevice::sendPoll);
  return true;
}

void BleGattDevice::sendPoll()
{
  if (!m_ready || !m_service || m_lastPoll.isEmpty())
    return;   // not connected yet: the reader repeats its poll
  if (m_keySent.isValid() && m_keySent.elapsed() < kKeyQuietMs - 50)
    return;   // a key just went out (sendCommand() polls again after)
  // one poll out at a time: Qt queues the writes, polls piling up there
  // held a key back for seconds (UT202BT: 5 to 10 s); an unanswered one
  // counts as lost after a second
  if (m_pollSent.isValid() && m_pollSent.elapsed() < 1000)
    return;
  m_pollSent.start();
  const auto mode = (m_writeChar.properties() & QLowEnergyCharacteristic::WriteNoResponse)
                    ? QLowEnergyService::WriteWithoutResponse : QLowEnergyService::WriteWithResponse;
  m_service->writeCharacteristic(m_writeChar, m_lastPoll, mode);
}

QStringList BleGattDevice::scan(FrameFormat::DataFormat format, int ms, QString *error)
{
  const auto p = profile(format);
  if (!p)
    return {};
  QBluetoothDeviceDiscoveryAgent agent;
  agent.setLowEnergyDiscoveryTimeout(ms);
  QStringList found;
  QSet<QString> seen;
  auto take = [&](const QBluetoothDeviceInfo &info)
  {
    const QString address = info.address().toString().toUpper();
    if (seen.contains(address))
      return;
    if (!p->matches(info.name(), info.serviceUuids(), info.manufacturerIds()))
      return;
    seen.insert(address);
    found << address + ' ' + info.name();
  };
  QObject::connect(&agent, &QBluetoothDeviceDiscoveryAgent::deviceDiscovered, take);
  QObject::connect(&agent, &QBluetoothDeviceDiscoveryAgent::errorOccurred, [&]
  {
    qCWarning(lcBle) << "scan failed:" << agent.errorString();
    if (error)
      *error = agent.errorString();
  });
  QObject::connect(&agent, &QBluetoothDeviceDiscoveryAgent::deviceUpdated,
                   [&](const QBluetoothDeviceInfo &info, QBluetoothDeviceInfo::Fields) { take(info); });
  QElapsedTimer t;
  t.start();
  agent.start(QBluetoothDeviceDiscoveryAgent::LowEnergyMethod);
  if (p->helperScan)
    bleHelperScan(&agent, ms);
  while (agent.isActive() && t.elapsed() < ms + 1000)
    QCoreApplication::processEvents(QEventLoop::AllEvents, 50);
  agent.stop();
  return found;
}
