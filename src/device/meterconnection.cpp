//======================================================================
// File:		dmm.cpp
// Author:	Matthias Toussaint
// Created:	Tue Apr 10 15:10:29 CEST 2001
//----------------------------------------------------------------------
// This file is part of QtDMM.
//
// QtDMM is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
//
// QtDMM is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with QtDMM.  If not, see <http://www.gnu.org/licenses/>.
//----------------------------------------------------------------------
// Copyright (c) 2001 Matthias Toussaint
//======================================================================

#include <QtGui>
#include <QFile>
#include <QFileInfo>
#include <QMessageBox>

#include "device/meterconnection.h"
#include "device/transports/hidserial.h"
#include "device/transports/hidholtek.h"
#include "device/transports/calc.h"
#ifdef QTDMM_WITH_BLE
#include "device/transports/ble.h"
#include "device/transports/blegatt.h"
#endif
#include "device/transports/rfc2217serial.h"
#include "device/transports/sigrok.h"
#include <QSerialPort>
#include "decoders.h"

#include <stdio.h>
#include <QTime>
#include <iostream>

#if defined(Q_OS_UNIX) && !defined(Q_OS_MAC)
namespace
{
// Distro-appropriate serial-group fallback, used only when the device file's
// own group can't be determined. Arch/CachyOS use "uucp" instead of Debian's
// "dialout" for serial port access; FreeBSD uses "dialer".
QString distroSuggestedSerialGroup()
{
#ifdef Q_OS_FREEBSD
  return QStringLiteral("dialer");
#endif
  QFile file("/etc/os-release");
  if (!file.open(QIODevice::ReadOnly | QIODevice::Text))
    return QStringLiteral("dialout");

  QString id;
  QString idLike;
  while (!file.atEnd())
  {
    const QString line = QString::fromUtf8(file.readLine()).trimmed();
    if (line.startsWith("ID="))
      id = line.mid(3).remove('"');
    else if (line.startsWith("ID_LIKE="))
      idLike = line.mid(8).remove('"');
  }

  if ((id + ' ' + idLike).toLower().contains("arch"))
    return QStringLiteral("uucp");

  return QStringLiteral("dialout");
}

QString serialPermissionHintForDevice(const QString &device)
{
  QFileInfo deviceInfo(device);
  const QString deviceGroup = deviceInfo.group();
  const QString suggestedGroup = !deviceGroup.isEmpty() && deviceGroup != QLatin1String("root")
    ? deviceGroup
    : distroSuggestedSerialGroup();

  QString message = QObject::tr("No permission to access %1.").arg(device);

  if (!suggestedGroup.isEmpty())
  {
    message += QObject::tr("\n\nOn this system the device is typically accessible via group '%1'.")
      .arg(suggestedGroup);
#ifdef Q_OS_FREEBSD
    message += QObject::tr("\nAdd your user with:\n\nsudo pw groupmod %1 -m $USER")
      .arg(suggestedGroup);
#else
    message += QObject::tr("\nAdd your user with:\n\nsudo usermod -aG %1 $USER")
      .arg(suggestedGroup);
#endif
    message += QObject::tr("\n\nThen log out and back in so the new group membership becomes active.");
  }

  return message;
}
}
#endif

MeterConnection::MeterConnection(QObject *parent)
  : QObject(parent),
    m_device(""),
    m_consoleLogging(false),
    m_delayTimer(0),
    m_decoder(nullptr)
{
  m_portHandler  = new Transport(this);
  m_readerThread = new FrameReader(this);

  connect(m_readerThread, &FrameReader::readEvent, this, &MeterConnection::readEventSLOT);

  m_readerThread->start();

}

void MeterConnection::setFormat(FrameFormat::DataFormat format)
{
  initDecoder(format);
  m_readerThread->setFormat(format);
}

bool MeterConnection::isOpen() const
{
  return m_portHandler->isOpen();
}


void MeterConnection::setDevice(const QString &device)
{
  QStringList deviceList = device.split( " " );
  m_portType = Transport::str2portType(deviceList.first());
  // "calc <unit> <formula>": the formula may contain spaces, so keep
  // everything after the type token; the other types take the last token
  // "ble <address> <key>" likewise
  // "hid 0xvvvv:0xpppp <path>": the USB ids find the cable again when the
  // path has changed (HidPath)
  if (m_portType == Transport::PortType::Calc || m_portType == Transport::PortType::Ble
      || m_portType == Transport::PortType::Hid || m_portType == Transport::PortType::HidHoltek)
    m_device = device.section(' ', 1).trimmed();
  else
    m_device = deviceList.last();
}

void MeterConnection::setCalcInputs(CalcDevice::Inputs inputs)
{
  m_portHandler->setCalcInputs(std::move(inputs));
}

void MeterConnection::initDecoder( FrameFormat::DataFormat df)
{
  if (m_decoder == nullptr || m_decoder->getType() != df)
  {
    m_decoder = DmmDecoder::getInstance(df);
    m_readerThread->setDecoder(m_decoder);
  }
  m_decoder->setModel(m_dmmInfo.model);
}


bool MeterConnection::sendKey(const QString &key)
{
  const QByteArray frame = m_decoder ? m_decoder->keyRequest(key) : QByteArray();
  QIODevice *port = m_portHandler->port();
  if (frame.isEmpty() || !port || !port->isOpen())
    return false;
  if (m_consoleLogging)
    consoleLine("key " + key.toUtf8() + ": " + frame.toHex(' '));
#ifdef QTDMM_WITH_BLE
  // a GATT link repeats what is written as its poll: a key goes once
  if (auto *gatt = dynamic_cast<BleGattDevice *>(port))
    return gatt->sendCommand(frame);
#endif
  return port->write(frame) == frame.size();
}

QStringList MeterConnection::keys() const
{
  QStringList keys;
  if (m_decoder)
    for (const QString &k : DmmDecoder::keyNames())
      if (m_decoder->hasKey(k, m_dmmInfo.model))
        keys << k;
  return keys;
}

bool MeterConnection::open(bool waitForDevice)
{
  m_wanted = true;
  if (openPort())
    return true;
  if (!waitForDevice || m_reconnectSeconds <= 0)
  {
    m_wanted = false;
    return false;
  }
  // not there yet: waited for as one that went away
  closePort();
  m_secondsInError = 0;
  setState(LinkState::Error, m_error + "\n" + tr("Retrying every %1 s.").arg(m_reconnectSeconds));
  if (m_delayTimer == 0)
    m_delayTimer = startTimer(1000);
  return false;
}

bool MeterConnection::openPort()
{
  if (!m_portHandler->create(m_dmmInfo, m_portType, m_device))
  {
    const bool bluetooth = m_portType == Transport::PortType::Ble || m_portType == Transport::PortType::BleGatt;
    m_error = bluetooth ? tr("This QtDMM was built without Bluetooth support.")
                        : tr("Error creating port %1.").arg(deviceName());
    Q_EMIT error(m_error);
    return false;
  }

  if (m_portHandler->port() && !m_portHandler->port()->open(QIODevice::ReadWrite))
  {
    if (m_portType == Transport::PortType::Calc || m_portType == Transport::PortType::RFC2217
        || m_portType == Transport::PortType::HidHoltek || m_portType == Transport::PortType::Ble || m_portType == Transport::PortType::BleGatt
        || m_portType == Transport::PortType::Hid)
    {
      // the formula did not parse / the address or key is malformed; the device says where
      m_error = m_portHandler->port()->errorString();
      Q_EMIT error(m_error);
      m_portHandler->close();
      return false;
    }
    switch (m_portHandler->error())
    {
      case QSerialPort::PermissionError:
        m_error = permissionHint();
        QMessageBox::critical(nullptr, tr("Missing Permission"), m_error);
        break;
      case QSerialPort::DeviceNotFoundError:
        m_error = tr("No such device %1.").arg(deviceName());
        break;
      default:
        m_error = tr("Error opening %1.\nDMM connected and switched on?").arg(deviceName());
        break;
    }
    Q_EMIT error(m_error);
    m_portHandler->close();
    return false;
  }

  if (!m_dmmInfo.externalSetup)
  {
    if (!m_portHandler->init())
    {
      m_error = tr("Error configuring serial port %1.").arg(deviceName());
      Q_EMIT error(m_error);
      m_portHandler->close();
      return false;
    }
  }
  QIODevice *port = m_portHandler->port();
  if (auto *calc = dynamic_cast<CalcDevice *>(port))
  {
    // which input meter is missing or silent; shown in the status bar
    // instead of "Connected" until all inputs deliver
    connect(calc, &CalcDevice::status, this, [this](const QString &message)
    {
      if (m_state == LinkState::Connected || m_state == LinkState::Connecting)
        setState(m_state, message.isEmpty() ? connectedMessage() : message);
    });
  }
  // the port devices tell when they are gone: socket closed or refused,
  // USB cable unplugged, sigrok-cli exited, serial device vanished
  if (auto *rfc = dynamic_cast<RFC2217SerialDevice *>(port))
    connect(rfc, &RFC2217SerialDevice::finished, this, [this, rfc] { portLost(rfc, rfc->errorString()); });
  else if (auto *hid = dynamic_cast<HIDSerialDevice *>(port))
    connect(hid, &HIDSerialDevice::finished, this, [this, hid] { portLost(hid, QString()); });
  else if (auto *htk = dynamic_cast<HidHoltekDevice *>(port))
    connect(htk, &HidHoltekDevice::finished, this, [this, htk] { portLost(htk, QString()); });
  else if (auto *sig = dynamic_cast<SigrokDevice *>(port))
    connect(sig, &SigrokDevice::finished, this, [this, sig] { portLost(sig, sig->errorString()); });
#ifdef QTDMM_WITH_BLE
  else if (auto *ble = dynamic_cast<BleAdvertisementDevice *>(port))
    connect(ble, &BleAdvertisementDevice::finished, this, [this, ble](const QString &reason) { portLost(ble, reason); });
  else if (auto *gatt = dynamic_cast<BleGattDevice *>(port))
    connect(gatt, &BleGattDevice::finished, this, [this, gatt](const QString &reason) { portLost(gatt, reason); });
#endif
  else if (auto *serial = dynamic_cast<QSerialPort *>(port))
    connect(serial, &QSerialPort::errorOccurred, this, [this, serial](QSerialPort::SerialPortError e)
    {
      if (e == QSerialPort::ResourceError || e == QSerialPort::DeviceNotFoundError
          || e == QSerialPort::PermissionError || e == QSerialPort::ReadError)
        portLost(serial, serial->errorString());
    });

  m_readerThread->setHandle(port);
  m_lastFrame.start();
  m_secondsInError = 0;
  setState(LinkState::Connecting, tr("Connecting ..."));
  timerEvent(0);

  if (m_delayTimer == 0)
    m_delayTimer = startTimer(1000);
  return true;
}

void MeterConnection::setState(LinkState state, const QString &message)
{
  const bool changed = state != m_state || message != m_error;
  m_state = state;
  m_error = message;
  if (changed)
  {
    Q_EMIT linkStateChanged(state, message);
    Q_EMIT error(message);
  }
}

QString MeterConnection::shortState() const
{
  switch (m_state)
  {
    case LinkState::Closed:     return tr("not connected");
    case LinkState::Connecting: return tr("connecting ...");
    case LinkState::Connected:  return QString();
    case LinkState::Timeout:    return tr("no data");
    case LinkState::Error:      return tr("no connection");
  }
  return QString();
}

QString MeterConnection::timeoutMessage() const
{
  // a HID cable that answers but never carries UART bytes: the meter's
  // serial output is off (UNI-T: the RS232/USB button)
  auto *hid = dynamic_cast<HIDSerialDevice *>(m_portHandler->port());
  if (hid && hid->cableAnswers() && !hid->dataSeen())
    return tr("The USB cable answers, but the meter sends nothing.\n"
              "Switch on the meter's serial output (on UNI-T meters: hold the RS232/USB button).");
  return tr("Timeout on device %1.\nDMM connected and switched on?").arg(deviceName());
}

void MeterConnection::closePort()
{
  m_readerThread->setHandle(nullptr);
  if (m_portHandler->port() && !m_dmmInfo.externalSetup)
    m_portHandler->close();
}

void MeterConnection::portLost(QIODevice *from, const QString &reason)
{
  // finished() also comes when we close the port ourselves, or late from a
  // port that has already been replaced
  if (!m_wanted || !m_portHandler->port() || from != m_portHandler->port())
    return;
  closePort();
  m_secondsInError = 0;
  QString text = tr("Lost connection to %1.").arg(deviceName());
  if (!reason.isEmpty())
    text += "\n" + reason;
  if (m_reconnectSeconds > 0)
    text += "\n" + tr("Retrying every %1 s.").arg(m_reconnectSeconds);
  setState(LinkState::Error, text);
}

// The device as the status bar may show it: a Bluetooth port string
// carries the encryption key, which stays out of messages.
QString MeterConnection::deviceName() const
{
  if (m_portType == Transport::PortType::Ble)
    return m_device.section(' ', 0, 0);
  if (m_portType == Transport::PortType::Hid || m_portType == Transport::PortType::HidHoltek)
    return m_device.section(' ', -1);   // the path, without the USB ids
  return m_device;
}

QString MeterConnection::connectedMessage() const
{
  // the formula is long and says little: what kind of source it is
  if (m_portType == Transport::PortType::Calc)
    return m_dmmInfo.model == QLatin1String("Simulated meter") ? tr("Simulated") : tr("Calculated");
  // the way in the status bar, the port in its tooltip (the second line)
  QString way;
  switch (m_portType)
  {
    case Transport::PortType::Ble:
    case Transport::PortType::BleGatt:
      way = tr("Bluetooth");
      break;
    case Transport::PortType::RFC2217:
      way = tr("network");
      break;
    case Transport::PortType::Sigrok:
      way = QStringLiteral("sigrok");
      break;
    case Transport::PortType::Serial:
      if (!m_device.contains(QLatin1String("usb"), Qt::CaseInsensitive) && !m_device.contains(QLatin1String("ACM")))
      {
        way = tr("serial port");
        break;
      }
      [[fallthrough]];
    default:
      way = QStringLiteral("USB");
      break;
  }
  return tr("connected (%1)").arg(way) + '\n' + deviceName();
}

QString MeterConnection::permissionHint() const
{
#if defined(Q_OS_UNIX) && !defined(Q_OS_MAC)
  if (m_portType == Transport::PortType::Serial && !m_device.isEmpty())
    return serialPermissionHintForDevice(m_device);
#endif

  return tr("Access denied for %1.").arg(deviceName());
}

void MeterConnection::close()
{
  m_wanted = false;
  if (m_delayTimer > 0)
  {
    killTimer(m_delayTimer);
    m_delayTimer = 0;
  }
  closePort();
  setState(LinkState::Closed, tr("Not connected"));
}

// Once a second: poll request, silence watchdog, reconnect after a loss.
void MeterConnection::timerEvent(QTimerEvent *)
{
  if (!m_wanted)
    return;
  if (!m_portHandler->port())
  {
    // lost: try again every m_reconnectSeconds
    if (m_reconnectSeconds > 0)
      ++m_secondsInError;
    if (m_reconnectSeconds > 0 && m_secondsInError >= m_reconnectSeconds)
    {
      m_secondsInError = 0;
      if (!openPort())
        setState(LinkState::Error, m_error + "\n" + tr("Retrying every %1 s.").arg(m_reconnectSeconds));
    }
    return;
  }
  m_readerThread->startRead();
#ifdef QTDMM_WITH_BLE
  // a Bluetooth GATT link takes seconds to set up after open(); that is not a
  // silent meter yet - the device reports its own failure after 20 s, and the
  // frame timeout counts from the moment it is ready
  auto *gatt = dynamic_cast<BleGattDevice *>(m_portHandler->port());
  if (gatt && !gatt->isReady())
  {
    m_lastFrame.start();
    setState(LinkState::Connecting, tr("Connecting to %1 over Bluetooth ...").arg(deviceName()));
    return;
  }
#endif
  if ((m_state == LinkState::Connecting || m_state == LinkState::Connected || m_state == LinkState::Timeout)
      && m_lastFrame.elapsed() > m_timeoutMs)
    setState(LinkState::Timeout, timeoutMessage());
}

void MeterConnection::consoleLine(const QByteArray &text) const
{
  std::cout << QTime::currentTime().toString("HH:mm:ss.zzz").toStdString() << ' ' << m_consoleName.toStdString() << ": "
            << text.toStdString() << std::endl;
}

// A complete frame from the reader: decode it and publish the reading. Frames
// are the heartbeat of the connection; the watchdog in timerEvent() misses them.
void MeterConnection::readEventSLOT(const QByteArray &data, int id, qint64 t, qint64 wall)
{
  if (m_consoleLogging)
    consoleLine(data.toHex(' ').toUpper());
  if (m_decoder == nullptr)
    return;

  m_lastFrame.restart();
  if (auto r = m_decoder->decode(data, id); r)
  {
    r->received = t;
    r->receivedWall = wall;
    Q_EMIT response(*r);
    for (const DmmDecoder::DmmChannel &c : r->channels)
      Q_EMIT value(c.dval, c.val, c.unit, c.special, r->range, r->hold, r->showBar, c.id);
    // a calculated value's text comes from CalcDevice::status (which input
    // is missing); only the state is ours
    if (m_portType == Transport::PortType::Calc)
      setState(LinkState::Connected, m_error);
    else
      setState(LinkState::Connected, r->error.isEmpty() ? connectedMessage() : QString("%1 %2").arg(r->error, deviceName()));
  }
  else
    setState(LinkState::Connected, tr("Error %1").arg(deviceName()));
}

void MeterConnection::setFramesPerReading(int frames)
{
  m_readerThread->setFramesPerReading(frames);
}
