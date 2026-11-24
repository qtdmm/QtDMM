// Copyright (c) 2026 The QtDMM developers
// SPDX-License-Identifier: GPL-3.0-or-later
#include "device/transports/usbtmc.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>

#ifdef Q_OS_LINUX
#include <cerrno>
#include <cstring>
#include <fcntl.h>
#include <linux/usb/tmc.h>
#include <poll.h>
#include <sys/ioctl.h>
#include <unistd.h>
#endif

namespace
{
constexpr int kTimeoutMs = 1000;   ///< for an answer
constexpr int kSliceMs = 100;      ///< a wait for an answer looks at stop() this often (not on usbtmc)
constexpr int kChunk = 65536;      ///< read at once; a longer answer takes more reads
}

// ---------------------------------------------------------------- worker

UsbTmcWorker::~UsbTmcWorker()
{
#ifdef Q_OS_LINUX
  if (m_fd >= 0)
    ::close(m_fd);
#endif
}

void UsbTmcWorker::send(const QByteArray &message)
{
  struct Done
  {
    std::atomic<int> &count;
    ~Done() { --count; }
  } done { pending };
  for (const QByteArray &part : message.split('\n'))
  {
    const QByteArray line = part.trimmed();
    if (line.isEmpty())
      continue;
    if (m_stop.load())
      return;
    if (!writeLine(line))
    {
#ifdef Q_OS_LINUX
      Q_EMIT failed(QString::fromLocal8Bit(std::strerror(errno)));
#endif
      return;
    }
    // a command without "?" has no answer, and a read would wait for one
    if (!line.contains('?'))
      continue;
    bool gone = false;
    QByteArray bytes = readAnswer(&gone);
    if (gone)
    {
#ifdef Q_OS_LINUX
      Q_EMIT failed(bytes.isEmpty() ? QString::fromLocal8Bit(std::strerror(errno)) : QString::fromLocal8Bit(bytes));
#endif
      return;
    }
    if (bytes.isEmpty())
      continue;   // no answer within the timeout: asked again later
    if (!bytes.endsWith('\n'))
      bytes.append('\n');
    Q_EMIT answer(bytes);
  }
}

bool UsbTmcWorker::writeLine(const QByteArray &line)
{
#ifdef Q_OS_LINUX
  const QByteArray out = line + '\n';
  qsizetype done = 0;
  while (done < out.size())
  {
    const ssize_t n = ::write(m_fd, out.constData() + done, size_t(out.size() - done));
    if (n < 0 && errno == EINTR)
      continue;
    if (n <= 0)
      return false;
    done += n;
  }
  return true;
#else
  Q_UNUSED(line)
  return false;
#endif
}

QByteArray UsbTmcWorker::readAnswer(bool *gone)
{
  *gone = false;
  QByteArray bytes;
#ifdef Q_OS_LINUX
  char buffer[kChunk];
  if (m_usbtmc)
  {
    // one read is the whole answer, or as much as fits; the driver ends a
    // read without an answer after its timeout (ETIMEDOUT)
    for (;;)
    {
      const ssize_t n = ::read(m_fd, buffer, sizeof(buffer));
      if (n < 0 && errno == EINTR)
        continue;
      if (n < 0)
      {
        // no answer, or a stalled pipe the driver clears: not gone
        *gone = errno != ETIMEDOUT && errno != EPIPE && errno != EAGAIN;
        return bytes;
      }
      bytes.append(buffer, n);
      if (n < qsizetype(sizeof(buffer)))
        return bytes;
    }
  }
  // a stream: what comes until the line ends or the time is up
  for (int waited = 0; waited < kTimeoutMs && !m_stop.load(); waited += kSliceMs)
  {
    pollfd p { m_fd, POLLIN, 0 };
    const int ready = ::poll(&p, 1, kSliceMs);
    if (ready < 0 && errno == EINTR)
      continue;
    if (ready < 0 || (p.revents & (POLLERR | POLLNVAL)) || (p.revents & POLLHUP && !(p.revents & POLLIN)))
    {
      *gone = true;
      errno = EIO;
      return {};
    }
    if (ready == 0)
      continue;
    const ssize_t n = ::read(m_fd, buffer, sizeof(buffer));
    if (n <= 0)
    {
      *gone = true;
      if (n == 0)
        errno = EIO;
      return {};
    }
    bytes.append(buffer, n);
    if (bytes.endsWith('\n'))
      break;
  }
#endif
  return bytes;
}

// ---------------------------------------------------------------- device

UsbTmcDevice::UsbTmcDevice(QString device, QObject *parent) : BufferedDevice(parent), m_path(pathForEntry(device))
{
}

UsbTmcDevice::~UsbTmcDevice()
{
  close();
}

QString UsbTmcDevice::pathForEntry(const QString &entry)
{
  const QString e = entry.trimmed();
  return e.section(' ', 0, 0).compare(QLatin1String("usbtmc"), Qt::CaseInsensitive) == 0 ? e.section(' ', 1).trimmed() : e;
}

bool UsbTmcDevice::supported()
{
#ifdef Q_OS_LINUX
  return true;
#else
  return false;
#endif
}

bool UsbTmcDevice::open(OpenMode mode)
{
  if (m_isOpen)
    return true;
#ifdef Q_OS_LINUX
  const int fd = ::open(QFile::encodeName(m_path).constData(), O_RDWR | O_NOCTTY | O_CLOEXEC);
  if (fd < 0)
  {
    const int err = errno;
    setErrorString(err == EACCES ? tr("No access to %1: it needs the udev rule of QtDMM for USB-TMC instruments.").arg(m_path)
                                 : tr("Cannot open %1: %2").arg(m_path, QString::fromLocal8Bit(std::strerror(err))));
    return false;
  }
  // the usbtmc driver waits 5 s for an answer by default; a pseudo
  // terminal (the tests) does not know the request
  unsigned int timeout = kTimeoutMs;
  const bool usbtmc = ::ioctl(fd, USBTMC_IOCTL_SET_TIMEOUT, &timeout) == 0;

  clearBuffer();
  m_thread = new QThread(this);
  m_worker = new UsbTmcWorker(fd, usbtmc);
  m_worker->moveToThread(m_thread);
  connect(this, &UsbTmcDevice::sendRequested, m_worker, &UsbTmcWorker::send, Qt::QueuedConnection);
  connect(m_worker, &UsbTmcWorker::answer, this, &UsbTmcDevice::onAnswer, Qt::QueuedConnection);
  connect(m_worker, &UsbTmcWorker::failed, this, &UsbTmcDevice::onFailed, Qt::QueuedConnection);
  m_thread->start();
  m_isOpen = true;
  return QIODevice::open(mode);
#else
  Q_UNUSED(mode)
  setErrorString(tr("USB-TMC instruments can only be read on Linux so far (%1).").arg(m_path));
  return false;
#endif
}

void UsbTmcDevice::close()
{
  const bool wasOpen = m_isOpen;
  m_isOpen = false;
  stopWorker();
  clearBuffer();
  if (wasOpen)
    QIODevice::close();
}

void UsbTmcDevice::stopWorker()
{
  if (!m_thread)
    return;
  m_worker->stop();
  m_thread->quit();
  // a read of the usbtmc driver ends within its timeout
  if (m_thread->wait(kTimeoutMs + 2000))
  {
    delete m_worker;   // closes the descriptor
    delete m_thread;
  }
  else
  {
    // should not happen; a leaked thread rather than a crash in it
    qWarning() << "USBTMC: worker thread did not stop";
    m_thread->setParent(nullptr);
  }
  m_worker = nullptr;
  m_thread = nullptr;
}

qint64 UsbTmcDevice::writeData(const char *data, qint64 len)
{
  if (!m_isOpen)
    return -1;
  // one waiting for its answer, one queued: a request more is dropped
  // (FrameReader asks again) instead of piling up behind a silent instrument
  if (m_worker->pending.load() >= 2)
    return len;
  ++m_worker->pending;
  Q_EMIT sendRequested(QByteArray(data, int(len)));
  return len;
}

qint64 UsbTmcDevice::readData(char *data, qint64 maxSize)
{
  if (!m_isOpen)
    return -1;
  return BufferedDevice::readData(data, maxSize);
}

void UsbTmcDevice::onAnswer(const QByteArray &bytes)
{
  if (!m_isOpen)
    return;
  if (!appendToBuffer(bytes))
  {
    onFailed(tr("Receive buffer limit exceeded."));
    return;
  }
  Q_EMIT readyRead();
}

void UsbTmcDevice::onFailed(const QString &what)
{
  if (!m_isOpen)
    return;   // our own close()
  setErrorString(tr("%1: %2").arg(m_path, what));
  qWarning() << "USBTMC:" << errorString();
  close();
  Q_EMIT finished();
}

// ---------------------------------------------------------------- ports

void UsbTmcDevice::availablePorts(QStringList &portlist)
{
#ifdef Q_OS_LINUX
  const QStringList names = QDir(QStringLiteral("/dev")).entryList({ QStringLiteral("usbtmc*") }, QDir::System, QDir::Name);
  for (const QString &name : names)
    portlist << QStringLiteral("USBTMC /dev/") + name;
#else
  Q_UNUSED(portlist)
#endif
}

UsbTmcDevice::Info UsbTmcDevice::info(const QString &path)
{
  Info i;
#ifdef Q_OS_LINUX
  // /sys/class/usbmisc/usbtmc0/device is the interface; the USB device
  // with its ids and strings is the directory above it
  const QString name = QFileInfo(pathForEntry(path)).fileName();
  const QString iface = QFileInfo(QStringLiteral("/sys/class/usbmisc/%1/device").arg(name)).canonicalFilePath();
  if (iface.isEmpty())
    return i;
  const QString dir = QFileInfo(iface).path();
  auto read = [&dir](const char *file)
  {
    QFile f(dir + '/' + QLatin1String(file));
    return f.open(QIODevice::ReadOnly) ? QString::fromUtf8(f.readAll()).trimmed() : QString();
  };
  i.manufacturer = read("manufacturer");
  i.product = read("product");
  i.vid = quint16(read("idVendor").toUInt(nullptr, 16));
  i.pid = quint16(read("idProduct").toUInt(nullptr, 16));
#else
  Q_UNUSED(path)
#endif
  return i;
}
