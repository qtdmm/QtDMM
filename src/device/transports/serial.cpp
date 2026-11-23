// Copyright (c) 2001 Matthias Toussaint
// Copyright (c) 2025 The QtDMM developers
// SPDX-License-Identifier: GPL-3.0-or-later

#include "device/transports/serial.h"
#include "device/transports/hidpath.h"
#include <QSerialPortInfo>

#ifdef Q_OS_LINUX
// The kernel registers ttyS0..ttyS31 whether or not a UART exists; only the
// ones with a sysfs node are real.
static QStringList getExistingUartTtyS()
{
  QStringList result;
  QRegularExpression re("ttyS\\d+");

  QDirIterator it("/sys/devices/pnp0", QDirIterator::Subdirectories);
  while (it.hasNext()) {
    const QString path = it.next();
    const QFileInfo fi(path);
    const QString name = fi.fileName();

    if (re.match(name).hasMatch() && (fi.isDir() || fi.isSymLink())) {
      result << "/dev/" + name;
    }
  }

  return result;
}
#endif

bool SerialDevice::availablePorts(QStringList &portlist)
{
#ifdef Q_OS_LINUX
  QStringList validTtyS = getExistingUartTtyS();
#endif
  portlist.clear();

  for (const QSerialPortInfo &port : QSerialPortInfo::availablePorts()) {
    QString portName =
#ifdef Q_OS_WIN
      port.portName();
#else
      port.systemLocation();
#endif

#ifdef Q_OS_LINUX
    if (portName.startsWith("/dev/ttyS")) {
      if (!validTtyS.contains(portName))
        continue;
    }
#endif

    portlist << "SERIAL " + portName;
  }

  return !portlist.isEmpty();
}

QString SerialDevice::stablePortName(const QString &port, const QString &byIdDir)
{
#ifdef Q_OS_LINUX
  const QString target = QFileInfo(port).canonicalFilePath();
  if (target.isEmpty() || port.startsWith(byIdDir + '/'))
    return port;
  const QDir dir(byIdDir);
  for (const QFileInfo &link : dir.entryInfoList(QDir::System | QDir::Files | QDir::NoDotAndDotDot, QDir::Name))
    if (link.isSymLink() && link.canonicalFilePath() == target)
      return link.absoluteFilePath();
#else
  Q_UNUSED(byIdDir)
#endif
  return port;
}

QString SerialDevice::stableDevice(const QString &device, const QString &hidLinkDir)
{
  const QString type = device.section(' ', 0, 0);
  // a USB-HID cable: its udev link (/dev/qtdmm/...) in place of /dev/hidrawN
  if (type.compare(QLatin1String("HID"), Qt::CaseInsensitive) == 0
      || type.compare(QLatin1String("HIDHOLTEK"), Qt::CaseInsensitive) == 0)
  {
    const QString path = device.section(' ', -1);
    const QString link = path.startsWith('/') ? HidPath::linkFor(path, hidLinkDir) : QString();
    return link.isEmpty() ? device : device.section(' ', 0, -2) + ' ' + link;
  }
  if (type.compare(QLatin1String("SERIAL"), Qt::CaseInsensitive) != 0)
    return device;
  return type + ' ' + stablePortName(device.section(' ', 1).trimmed());
}

bool SerialDevice::init()
{
  Parity parity = NoParity;
  switch (m_dmmInfo.parity)
  {
    case 0: parity = NoParity;   break;
    case 1: parity = EvenParity; break;
    case 2: parity = OddParity;  break;
  }

  return setParity(parity)
    && setBaudRate(m_dmmInfo.baud)
    && setStopBits(static_cast<StopBits>(m_dmmInfo.stopBits))
    && setDataBits(static_cast<DataBits>(m_dmmInfo.bits))
    && setDataTerminalReady(m_dmmInfo.dtr)
    && setRequestToSend(m_dmmInfo.rts);
}
