#include "device/transports/hidserial.h"
#include "device/transports/hidpath.h"

// The chips this driver knows, by USB id. The Hoitek HE2325U is the CH9325's
// predecessor with the same protocol.
namespace
{
struct KnownCable { unsigned short vid, pid; HIDSerialDevice::Chip chip; };
const KnownCable kCables[] = {
  { 0x04fa, 0x2490, HIDSerialDevice::Chip::CH9325 },   // Hoitek HE2325U
  { 0x1a86, 0xe008, HIDSerialDevice::Chip::CH9325 },   // WCH CH9325 (UT-D04 and friends)
  { 0x10c4, 0xea80, HIDSerialDevice::Chip::CP2110 },   // SiLabs CP2110 (UT-D09 first revision)
  { 0x1a86, 0xe429, HIDSerialDevice::Chip::CH9329 },   // WCH CH9329 custom-HID (UT-D09 second revision)
  { 0x0820, 0x0001, HIDSerialDevice::Chip::BU86X },    // Brymen BU-86X IR adapter
};
}

HIDSerialDevice::Chip HIDSerialDevice::chipFor(unsigned short vendorId, unsigned short productId)
{
  for (const KnownCable &c : kCables)
    if (c.vid == vendorId && c.pid == productId)
      return c.chip;
  return Chip::CH9325;
}

HIDSerialDevice::Chip HIDSerialDevice::chipForEntry(const QString &entry)
{
  // "0x1a86:0xe429" somewhere in the entry; MeterConnection::setDevice() hands us only
  // the path, so the ids are looked up from the enumeration in that case
  static const QRegularExpression ids("0x([0-9a-fA-F]{4}):0x([0-9a-fA-F]{4})");
  const auto m = ids.match(entry);
  if (m.hasMatch())
    return chipFor(m.captured(1).toUShort(nullptr, 16), m.captured(2).toUShort(nullptr, 16));
  return Chip::CH9325;
}

QString HIDSerialDevice::pathForEntry(const QString &entry)
{
  const QString e = entry.trimmed();
  return e.contains(' ') ? e.section(' ', -1) : e;
}

int HIDSerialDevice::unpackReport(Chip chip, const unsigned char *report, int reportLen, unsigned char *out)
{
  if (reportLen < 1)
    return -1;
  if (chip == Chip::BU86X)
  {
    // the whole report is UART data
    memcpy(out, report, reportLen);
    return reportLen;
  }
  if (chip == Chip::CH9329 || chip == Chip::CP2110)
  {
    // @0 count (0..63; on the CP2110 this is the report id), @1.. raw UART bytes
    const int count = report[0];
    if (count > 63 || count > reportLen - 1)
      return -1;
    memcpy(out, report + 1, count);
    return count;
  }
  // CH9325: @0 = 0xF0 | count (count in the low 3 bits), @1.. bytes with
  // the top bit always set
  const int count = report[0] & 0x07;
  if (count > reportLen - 1)
    return -1;
  for (int i = 0; i < count; i++)
    out[i] = report[1 + i] & 0x7f;
  return count;
}

QByteArray HIDSerialDevice::packWrite(Chip chip, const QByteArray &data)
{
  QByteArray r;
  switch (chip)
  {
    case Chip::BU86X:
      // report id placeholder, then the bytes as they are
      r.append('\0');
      r.append(data);
      break;
    case Chip::CH9329:
      // (@-1 report id placeholder) @0 count @1.. data, 64-byte report
      r.append('\0');
      r.append(static_cast<char>(qMin(63, data.size())));
      r.append(data.left(63));
      r.append(QByteArray(65 - r.size(), '\0'));
      break;
    case Chip::CP2110:
      // the report id is the byte count
      r.append(static_cast<char>(qMin(63, data.size())));
      r.append(data.left(63));
      break;
    case Chip::CH9325:
      // (@-1 report id placeholder) @0 count - without the 0xF0 of the
      // input reports - @1.. up to seven bytes, an 8-byte report (libsigrok
      // serial_hid_ch9325.c). Most of these cables only receive; the UT325
      // needs its start byte.
      r.append('\0');
      r.append(static_cast<char>(qMin(7, data.size())));
      r.append(data.left(7));
      r.append(QByteArray(9 - r.size(), '\0'));
      break;
  }
  return r;
}

QByteArray HIDSerialDevice::ch9325ConfigReport(int baud, int bits)
{
  const unsigned int bps = baud > 0 ? static_cast<unsigned int>(baud) : 19200;
  const int b = (bits >= 5 && bits <= 8) ? bits : 8;
  QByteArray r(6, '\0');
  r[1] = static_cast<char>(bps);
  r[2] = static_cast<char>(bps >> 8);
  r[3] = static_cast<char>(bps >> 16);
  r[4] = static_cast<char>(bps >> 24);
  r[5] = static_cast<char>(b - 5);
  return r;
}

QString HIDSerialDevice::chipName(Chip chip)
{
  switch (chip)
  {
    case Chip::CH9325: return "CH9325";
    case Chip::CP2110: return "CP2110";
    case Chip::CH9329: return "CH9329";
    case Chip::BU86X:  return "BU86X";
  }
  return "CH9325";
}

HIDSerialDevice::Chip HIDSerialDevice::chipFromName(const QString &name, bool *ok)
{
  for (Chip c : {Chip::CH9325, Chip::CP2110, Chip::CH9329, Chip::BU86X})
    if (chipName(c) == name)
    {
      if (ok) *ok = true;
      return c;
    }
  if (ok) *ok = false;
  return Chip::CH9325;
}

QByteArray HIDSerialDevice::cp2110ConfigReport(int baud, int bits, int parity, int stopBits)
{
  // (@-1 report id 0x50) @0 baud big endian, @4 parity (0 none, 1 even,
  // 2 odd), @5 flow control (0 none), @6 data bits as (bits - 5), @7 stop
  // bits (0 = 1 bit, 1 = 2 bits)
  QByteArray r(9, '\0');
  const unsigned int b = static_cast<unsigned int>(qBound(300, baud > 0 ? baud : 9600, 1000000));
  r[0] = 0x50;
  r[1] = static_cast<char>(b >> 24);
  r[2] = static_cast<char>(b >> 16);
  r[3] = static_cast<char>(b >> 8);
  r[4] = static_cast<char>(b);
  r[5] = static_cast<char>(qBound(0, parity, 2));
  r[6] = 0;
  r[7] = static_cast<char>((bits >= 5 && bits <= 8 ? bits : 8) - 5);
  r[8] = static_cast<char>(stopBits >= 2 ? 1 : 0);
  return r;
}

HIDSerialDevice::HIDSerialDevice(const DmmDecoder::DMMInfo &info, QString device, QObject *p)
  : BufferedDevice(p)
  , m_dmmInfo(info)
{
  if (device.isNull())
    return;
  // the path may have changed since the entry was saved: the cable again by its ids
  const HidPath::Choice where = HidPath::resolve(device);
  if (where.ambiguous)
  {
    m_openError = tr("%1 is gone, and several cables with its USB ids are plugged in: choose the port in the "
                     "device settings.").arg(pathForEntry(device));
    qWarning() << "HID:" << m_openError;
    return;
  }
  const QString path = where.path;
  if (!where.moved.isEmpty())
    qCInfo(lcHid) << "the cable of" << pathForEntry(device) << "is now" << path;
  m_chip = chipForEntry(device);
  if (!device.contains(':'))
  {
    // only the path: find its ids in the enumeration
    struct hid_device_info *devs = hid_enumerate(0, 0);
    for (struct hid_device_info *d = devs; d; d = d->next)
      if (path == QString::fromLatin1(d->path))
        m_chip = chipFor(d->vendor_id, d->product_id);
    hid_free_enumeration(devs);
  }
  m_handle = hid_open_path(path.toUtf8().data());
  if (!m_handle)
  {
    m_openError = tr("Cannot open %1: %2").arg(path, QString::fromWCharArray(hid_error(nullptr)));
#ifdef Q_OS_LINUX
    m_openError += tr("\nCable plugged in? No permission? The cable needs the udev rule of QtDMM.");
#endif
    qWarning() << "HID:" << m_openError;
    return;
  }
  qCDebug(lcHid) << "opened" << path
                 << (m_chip == Chip::CH9329 ? "(CH9329)" : m_chip == Chip::CP2110 ? "(CP2110)"
                     : m_chip == Chip::BU86X ? "(BU-86X)" : "(CH9325)");
  if (!configureCable())
  {
    m_openError = tr("Cannot set up the cable at %1.").arg(path);
    hid_close(m_handle);
    m_handle = nullptr;
    return;
  }
  m_isOpen = true;

  // the blocking read loop runs in its own thread (HidReaderThread)
  m_reader.start(m_handle, this, &HIDSerialDevice::onReport, &HIDSerialDevice::onReadError);
}

// ---------------------------------------------------------------------------

HIDSerialDevice::~HIDSerialDevice()
{
  close();
  stopReader();
}


bool HIDSerialDevice::availablePorts(QStringList &portlist)
{
  qint64 portlistLen = portlist.size();
  for (const KnownCable &c : kCables)
    HIDSerialDevice::availablePorts(portlist, c.vid, c.pid);

  return portlist.size() > portlistLen;
}

bool HIDSerialDevice::availablePorts(QStringList &portlist,unsigned short vendorId, unsigned short productId)
{
  int devCount;
  struct hid_device_info *devs, *curDev;


  devs = hid_enumerate(vendorId, productId); // all chips this SW belongs to...
  for (devCount = 0, curDev = devs; curDev != nullptr; curDev = curDev->next) {
    devCount++;
  }

  for (curDev = devs; curDev != nullptr; curDev = curDev->next) {
    // a udev link (/dev/qtdmm/...) stays the same across plugs and restarts
    const QString path = QString::fromLatin1(curDev->path);
    const QString link = HidPath::linkFor(path);
    portlist << QString("HID 0x%1:0x%2 %3")
    .arg(vendorId, 4, 16, QLatin1Char('0'))
    .arg(productId, 4, 16, QLatin1Char('0'))
    .arg(link.isEmpty() ? path : link);
  }
  hid_free_enumeration(devs);

  return (devCount > 0);
}

bool HIDSerialDevice::open(OpenMode mode)
{
  if (!m_isOpen)
  {
    setErrorString(m_openError.isEmpty() ? tr("Cannot open the cable.") : m_openError);
    return false;
  }
  return QIODevice::open(mode);
}

void HIDSerialDevice::close()
{
  if (!m_isOpen)
    return;
  m_isOpen = false;
  stopReader();
  // Without this the QIODevice base keeps reporting isOpen() == true, which
  // is what Transport::isOpen() actually queries.
  QIODevice::close();
}

void HIDSerialDevice::stopReader()
{
  m_reader.stop();
  m_handle = nullptr;   // closed by the reader
}

bool HIDSerialDevice::configureCable()
{
  int res = 0;
  if (m_chip == Chip::CH9325)
  {
    // per sigrok's CH9325 driver; the two bytes before the data bits are
    // unknown (parity/stop bits?) and left at zero there too
    const QByteArray report = ch9325ConfigReport(m_dmmInfo.baud, m_dmmInfo.bits);
    res = hid_send_feature_report(m_handle, reinterpret_cast<const unsigned char *>(report.constData()), report.size());
    qCDebug(lcHid) << "feature report" << report.toHex(' ') << "baud" << m_dmmInfo.baud << "->" << res;
  }
  else if (m_chip == Chip::CP2110)
  {
    // enable the UART, then set the line coding
    unsigned char enable[2] = { 0x41, 0x01 };
    res = hid_send_feature_report(m_handle, enable, 2);
    qCDebug(lcHid) << "CP2110 uart enable ->" << res;
    if (res >= 0)
    {
      const QByteArray cfg = cp2110ConfigReport(m_dmmInfo.baud, m_dmmInfo.bits, m_dmmInfo.parity, m_dmmInfo.stopBits);
      res = hid_send_feature_report(m_handle, reinterpret_cast<const unsigned char *>(cfg.constData()), cfg.size());
      qCDebug(lcHid) << "CP2110 uart config" << cfg.toHex(' ') << "->" << res;
    }
  }
  else if (m_chip == Chip::BU86X)
  {
    qCDebug(lcHid) << "BU-86X: fixed speed, nothing to configure";
  }
  else
  {
    // CH9329: the line coding is persistent chip configuration (9600 8N1
    // as shipped), nothing to negotiate
    qCDebug(lcHid) << "CH9329: no feature report, fixed 9600 8N1";
    if (m_dmmInfo.baud > 0 && m_dmmInfo.baud != 9600)
      qWarning() << "HID: this cable runs at 9600 baud, the meter is configured for" << m_dmmInfo.baud;
  }

  if (res < 0)
  {
    qCritical() << "HID: unable to send the feature report:" << QString::fromWCharArray(hid_error(m_handle));
    return false;
  }
  return true;
}

void HIDSerialDevice::onReport(const QByteArray &raw)
{
  if (!m_isOpen)
    return;
  qCDebug(lcHid) << "report" << raw.toHex(' ');
  m_reportsSeen++;
  unsigned char payload[64];
  const int len = unpackReport(m_chip, reinterpret_cast<const unsigned char *>(raw.constData()), raw.size(), payload);
  if (len < 0)
  {
    qWarning() << "HID: malformed report" << raw.toHex(' ');
    return;
  }
  if (len > 0)
  {
    if (!appendToBuffer(reinterpret_cast<const char *>(payload), len))
    {
      onReadError(tr("Receive buffer limit exceeded."));
      return;
    }
    m_dataSeen = true;
    Q_EMIT readyRead();
  }
}

void HIDSerialDevice::onReadError(const QString &what)
{
  if (!m_isOpen)
    return;   // our own close() ends the loop without an error
  setErrorString(what);
  qWarning() << "HID: read failed:" << what;
  m_isOpen = false;
  stopReader();
  QIODevice::close();
  Q_EMIT finished();
}

qint64 HIDSerialDevice::readData(char *data, qint64 maxSize)
{
  if (!m_isOpen)
    return -1;
  return BufferedDevice::readData(data, maxSize);
}


qint64 HIDSerialDevice::writeData(const char *data, qint64 len)
{
  // poll requests (DmmDecoder::pollRequest()); on a CH9325 cable without
  // a transmitter they go nowhere, the meters stream anyway
  if (!m_isOpen || !m_handle)
    return -1;
  // one report carries seven bytes on a CH9325, 63 on the others
  const qint64 step = m_chip == Chip::CH9325 ? 7 : 63;
  for (qint64 pos = 0; pos < len; pos += step)
  {
    const QByteArray report = packWrite(m_chip, QByteArray(data + pos, static_cast<int>(qMin(step, len - pos))));
    if (report.isEmpty())
      return len;   // nothing to send on this cable, but not an error
    const int res = hid_write(m_handle, reinterpret_cast<const unsigned char *>(report.constData()), report.size());
    qCDebug(lcHid) << "write" << report.toHex(' ') << "->" << res;
    if (res < 0)
    {
      qWarning() << "HID: write failed:" << QString::fromWCharArray(hid_error(m_handle));
      return -1;
    }
  }
  return len;
};

