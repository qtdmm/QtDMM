// Copyright (c) 2026 The QtDMM developers
// SPDX-License-Identifier: GPL-3.0-or-later
#include "device/transports/ble.h"

#include <QBluetoothAddress>
#include <QBluetoothDeviceDiscoveryAgent>
#include <QBluetoothDeviceInfo>
#include <QCoreApplication>
#include <QElapsedTimer>
#include <QLoggingCategory>
#include <QProcess>
#include <QSet>

#include "device/victronble.h"

Q_LOGGING_CATEGORY(lcBle, "qtdmm.ble", QtWarningMsg)

void bleHelperScan(QBluetoothDeviceDiscoveryAgent *agent, int ms)
{
#ifdef Q_OS_LINUX
  auto *helper = new QProcess(agent);   // killed with the agent at the latest
  helper->setStandardOutputFile(QProcess::nullDevice());
  helper->setStandardErrorFile(QProcess::nullDevice());
  QObject::connect(helper, &QProcess::errorOccurred, helper, [](QProcess::ProcessError e)
  {
    if (e == QProcess::FailedToStart)
      qCDebug(lcBle) << "bluetoothctl not there: no helper scan";
  });
  auto stop = [helper] { helper->kill(); };
  QObject::connect(agent, &QBluetoothDeviceDiscoveryAgent::finished, helper, stop);
  QObject::connect(agent, &QBluetoothDeviceDiscoveryAgent::canceled, helper, stop);
  QObject::connect(agent, &QBluetoothDeviceDiscoveryAgent::errorOccurred, helper, stop);
  helper->start(QStringLiteral("bluetoothctl"),
                { QStringLiteral("--timeout"), QString::number(ms / 1000 + 1), QStringLiteral("scan"), QStringLiteral("le") });
  qCDebug(lcBle) << "helper scan: bluetoothctl scan le for" << ms / 1000 + 1 << "s";
#else
  Q_UNUSED(agent);
  Q_UNUSED(ms);
#endif
}

BleAdvertisementDevice::BleAdvertisementDevice(const DmmDecoder::DMMInfo &, const QString &device, QObject *parent)
  : BufferedDevice(parent)
{
  // "<address> <key> [<main field> <second field> [<third> <fourth>]]"
  const QStringList parts = device.simplified().split(' ');
  m_address = parts.value(0).toUpper();
  m_key = VictronBle::keyFromHex(parts.value(1));
  m_fields = parts.mid(2);
}

BleAdvertisementDevice::~BleAdvertisementDevice()
{
  close();
}

bool BleAdvertisementDevice::open(OpenMode mode)
{
  if (QBluetoothAddress(m_address).isNull())
  {
    setErrorString(tr("No Bluetooth address configured."));
    return false;
  }
  if (m_key.size() != 16)
  {
    setErrorString(tr("The encryption key must be 32 hex digits (VictronConnect: Product info, Instant readout via Bluetooth)."));
    return false;
  }

  // BlueZ warns on every scan that it cannot tell random from public
  // addresses without CAP_NET_ADMIN - harmless for listening, so it stays
  // quiet unless --debug switched our own category on
  if (!lcBle().isDebugEnabled())
    QLoggingCategory::setFilterRules(QStringLiteral("qt.bluetooth.bluez.warning=false\nqt.bluetooth.bluez.info=false"));

  m_agent = new QBluetoothDeviceDiscoveryAgent(this);
  // advertisements keep changing, so every update of a known device counts
  connect(m_agent, &QBluetoothDeviceDiscoveryAgent::deviceDiscovered, this, &BleAdvertisementDevice::onDevice);
  connect(m_agent, &QBluetoothDeviceDiscoveryAgent::deviceUpdated, this,
          [this](const QBluetoothDeviceInfo &info, QBluetoothDeviceInfo::Fields) { onDevice(info); });
  connect(m_agent, &QBluetoothDeviceDiscoveryAgent::errorOccurred, this, [this]
  {
    const QString reason = m_agent->errorString();
    qCWarning(lcBle) << "scan error:" << reason;
    setErrorString(reason);
    Q_EMIT finished(reason);
  });
  // the scan stops itself after its timeout; keep it going while open
  connect(m_agent, &QBluetoothDeviceDiscoveryAgent::finished, this, [this]
  {
    if (isOpen() && m_agent)
    {
          m_agent->start(QBluetoothDeviceDiscoveryAgent::LowEnergyMethod);
    }
  });
  m_agent->setLowEnergyDiscoveryTimeout(60000);
  m_agent->start(QBluetoothDeviceDiscoveryAgent::LowEnergyMethod);
  if (m_agent->error() != QBluetoothDeviceDiscoveryAgent::NoError)
  {
    setErrorString(m_agent->errorString());
    delete m_agent;
    m_agent = nullptr;
    return false;
  }
  m_lastIv = -1;
  m_keyMismatches = 0;
  m_failed = false;
  return QIODevice::open(mode | QIODevice::Unbuffered);
}

void BleAdvertisementDevice::close()
{
  if (m_agent)
  {
    m_agent->disconnect(this);
    m_agent->stop();
    m_agent->deleteLater();
    m_agent = nullptr;
  }
  clearBuffer();
  QIODevice::close();
}

void BleAdvertisementDevice::onDevice(const QBluetoothDeviceInfo &info)
{
  if (m_failed || info.address().toString().toUpper() != m_address)
    return;
  const QByteArray data = info.manufacturerData(VictronBle::CompanyId);
  const auto adv = VictronBle::parse(data);
  if (!adv)
    return;
  if (adv->iv == m_lastIv)
    return;   // the same advertisement, seen again
  const auto plain = VictronBle::decrypt(*adv, m_key);
  if (!plain)
  {
    // one stray record is not proof; three in a row are
    if (++m_keyMismatches >= 3)
    {
      const QString reason = tr("The encryption key does not match %1.").arg(VictronBle::modelName(adv->model));
      setErrorString(reason);
      Q_EMIT finished(reason);
    }
    return;
  }
  m_keyMismatches = 0;
  m_lastIv = adv->iv;
  qCDebug(lcBle) << VictronBle::modelName(adv->model) << "type" << adv->readoutType << "iv" << adv->iv << plain->toHex();
  if (!appendToBuffer(VictronBle::frame(adv->readoutType, *plain, m_fields)))
  {
    m_failed = true;
    const QString reason = tr("Bluetooth receive buffer limit exceeded.");
    setErrorString(reason);
    Q_EMIT finished(reason);
    return;
  }
  Q_EMIT readyRead();
}

qint64 BleAdvertisementDevice::writeData(const char *, qint64 len)
{
  return len;   // nothing to say to a broadcaster
}

QStringList BleAdvertisementDevice::scan(int ms, QString *error)
{
  QBluetoothDeviceDiscoveryAgent agent;
  QObject::connect(&agent, &QBluetoothDeviceDiscoveryAgent::errorOccurred, [&]
  {
    qCWarning(lcBle) << "scan failed:" << agent.errorString();
    if (error)
      *error = agent.errorString();
  });
  QMap<QString, QString> found;
  QObject::connect(&agent, &QBluetoothDeviceDiscoveryAgent::deviceDiscovered, [&](const QBluetoothDeviceInfo &info)
  {
    if (info.manufacturerIds().contains(VictronBle::CompanyId))
      found[info.address().toString()] = info.name();
  });
  agent.setLowEnergyDiscoveryTimeout(ms);
  agent.start(QBluetoothDeviceDiscoveryAgent::LowEnergyMethod);
  QElapsedTimer t;
  t.start();
  while (agent.isActive() && t.elapsed() < ms + 1000)
    QCoreApplication::processEvents(QEventLoop::AllEvents, 50);
  agent.stop();
  QStringList list;
  for (auto it = found.constBegin(); it != found.constEnd(); ++it)
    list << QString("%1 %2").arg(it.key(), it.value());
  return list;
}
