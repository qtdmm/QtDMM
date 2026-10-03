// Copyright (c) 2026 The QtDMM developers
// SPDX-License-Identifier: GPL-3.0-or-later
#include "device/transports/hidholtek.h"

HidHoltekDevice::HidHoltekDevice(const DmmDecoder::DMMInfo & /*info*/, const QString &device, QObject *p)
  : QIODevice(p)
{
  if (device.isNull())
    return;

  const QString path = pathForEntry(device);
  m_handle = hid_open_path(path.toUtf8().constData());
  if (!m_handle)
  {
    m_openError = tr("Cannot open %1: %2").arg(path, QString::fromWCharArray(hid_error(nullptr)));
#ifdef Q_OS_LINUX
    m_openError += tr("\nNo permission? The sensor needs a udev rule for USB ID %1:%2.")
                       .arg(VendorId, 4, 16, QLatin1Char('0')).arg(ProductId, 4, 16, QLatin1Char('0'));
#endif
    qWarning() << "HID:" << m_openError;
    return;
  }
  qCDebug(lcHid) << "opened" << path << "(Holtek sensor)";

  // Same wake-up as the original tool for this sensor: a two-byte feature
  // report, report id 0. Without it the sensor does not start sending.
  const unsigned char init[] = { 0x00, 0x00 };
  if (hid_send_feature_report(m_handle, init, sizeof(init)) < 0)
  {
    m_openError = tr("Cannot initialise the sensor: %1").arg(QString::fromWCharArray(hid_error(m_handle)));
    qWarning() << "HID:" << m_openError;
    hid_close(m_handle);
    m_handle = Q_NULLPTR;
    return;
  }
  m_isOpen = true;

  // The blocking read loop runs in its own thread, exactly as for
  // HIDSerialDevice: this object stays in the caller's thread, the reports
  // arrive as queued signals.
  m_thread = new QThread(this);
  m_reader = new HidReader(m_handle);
  m_reader->moveToThread(m_thread);
  connect(m_thread, &QThread::started, m_reader, &HidReader::run);
  connect(m_reader, &HidReader::report, this, &HidHoltekDevice::onReport);
  connect(m_reader, &HidReader::readError, this, &HidHoltekDevice::onReadError);
  // direct: stopReader() blocks this thread's event loop while it waits for the quit
  connect(m_reader, &HidReader::finished, m_thread, &QThread::quit, Qt::DirectConnection);
  m_thread->start();
}

HidHoltekDevice::~HidHoltekDevice()
{
  close();
  stopReader();
}

bool HidHoltekDevice::availablePorts(QStringList &portlist)
{
  const qsizetype before = portlist.size();
  struct hid_device_info *devs = hid_enumerate(VendorId, ProductId);
  for (struct hid_device_info *d = devs; d; d = d->next)
    portlist << QString("HIDHOLTEK 0x%1:0x%2 %3")
                   .arg(d->vendor_id, 4, 16, QLatin1Char('0'))
                   .arg(d->product_id, 4, 16, QLatin1Char('0'))
                   .arg(QString::fromLatin1(d->path));
  hid_free_enumeration(devs);
  return portlist.size() > before;
}

QString HidHoltekDevice::pathForEntry(const QString &entry)
{
  const QString e = entry.trimmed();
  return e.contains(' ') ? e.section(' ', -1) : e;
}

bool HidHoltekDevice::parseReport(const QByteArray &report, Record &out)
{
  if (report.size() < 3)
    return false;
  const auto b = [&report](int i) { return static_cast<quint8>(report[i]); };
  // the checksum when the report is long enough to carry one; the end
  // marker (@4 = 0x0d) is not insisted on
  if (report.size() >= 4 && b(3) != static_cast<quint8>(b(0) + b(1) + b(2)))
    return false;
  out.address = b(0);
  out.raw = static_cast<quint16>((b(1) << 8) | b(2));
  return true;
}

QByteArray HidHoltekDevice::formatLine(quint8 address, quint16 raw)
{
  return QByteArray::number(address, 16).rightJustified(2, '0') + ' ' + QByteArray::number(raw) + '\n';
}

bool HidHoltekDevice::open(OpenMode mode)
{
  if (!m_isOpen)
  {
    setErrorString(m_openError);
    return false;
  }
  return QIODevice::open(mode);
}

void HidHoltekDevice::close()
{
  if (!m_isOpen)
    return;
  m_isOpen = false;
  stopReader();
  // Without this the QIODevice base keeps reporting isOpen() == true, which
  // is what Transport::isOpen() actually queries.
  QIODevice::close();
}

void HidHoltekDevice::stopReader()
{
  if (!m_thread)
    return;
  if (m_reader)
    m_reader->stop();
  m_thread->quit();   // in case the loop already ended
  if (m_thread->isRunning() && !m_thread->wait(2000))
  {
    // hid_read_timeout() did not return - should not happen; better a
    // leaked thread than a crash in it
    qWarning() << "HID: reader thread did not stop";
    m_thread->setParent(nullptr);
  }
  else
  {
    delete m_reader;
    delete m_thread;
  }
  m_thread = Q_NULLPTR;
  m_reader = Q_NULLPTR;
  m_handle = Q_NULLPTR;   // closed by the reader
}

void HidHoltekDevice::onReport(const QByteArray &raw)
{
  if (!m_isOpen)
    return;
  qCDebug(lcHid) << "report" << raw.toHex(' ');

  Record r;
  if (!parseReport(raw, r))
  {
    qCDebug(lcHid) << "ignored (short or bad checksum)" << raw.toHex(' ');
    return;
  }

  switch (r.address)
  {
    case Co2:         m_co2 = r.raw;         m_haveCo2 = true;         break;
    case Temperature: m_temperature = r.raw; m_haveTemperature = true; break;
    case Humidity:    m_humidity = r.raw;    m_haveHumidity = true;    break;
    default:          return;   // other records (not needed)
  }

  if (m_haveCo2 && m_haveTemperature && m_haveHumidity)
  {
    // a complete set: one line per quantity, in the order of the decoder's ids
    m_rx.append(formatLine(Co2, m_co2));
    m_rx.append(formatLine(Temperature, m_temperature));
    m_rx.append(formatLine(Humidity, m_humidity));
    m_haveCo2 = m_haveTemperature = m_haveHumidity = false;
    Q_EMIT readyRead();
  }
}

void HidHoltekDevice::onReadError(const QString &what)
{
  if (!m_isOpen)
    return;   // our own close() ends the loop without an error
  qWarning() << "HID: read failed:" << what;
  m_isOpen = false;
  stopReader();
  QIODevice::close();
  Q_EMIT finished();
}

qint64 HidHoltekDevice::bytesAvailable() const
{
  return QIODevice::bytesAvailable() + m_rx.size();
}

qint64 HidHoltekDevice::readData(char *data, qint64 maxSize)
{
  if (!m_isOpen)
    return -1;
  const qint64 len = qMin(maxSize, qint64(m_rx.size()));
  memcpy(data, m_rx.constData(), len);
  m_rx.remove(0, len);
  return len;
}

qint64 HidHoltekDevice::writeData(const char * /*data*/, qint64 len)
{
  return len;   // the sensor only talks; DecoderTfaAirControl has no poll request
}
