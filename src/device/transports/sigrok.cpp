// Copyright (c) 2025 The QtDMM developers
// SPDX-License-Identifier: GPL-3.0-or-later

#include "device/transports/sigrok.h"

#include <QCoreApplication>
#include <QTimer>
#include <utility>

//#define SIGROK_DEBUG

SigrokDevice::SigrokDevice(const DmmDecoder::DMMInfo &info, QString device, QObject *parent)
  : QIODevice(parent)
  , m_dmmInfo(info)
  , m_device(device)
{
  m_sigrok = info.sigrokExe.isEmpty() ? "sigrok-cli" : info.sigrokExe;
}

SigrokDevice::~SigrokDevice()
{
  close();
}


bool SigrokDevice::init() {
  if (m_process != nullptr)
    return false;

  QStringList args;
  args << "--driver" << m_device << "--continuous";

  // nothing here waits for sigrok-cli: one that cannot start or ends later
  // reports it through finished(), like a lost port
  m_process = new QProcess(this);
  connect(m_process, &QProcess::readyReadStandardOutput, this, &SigrokDevice::onProcessReadyRead);
  connect(m_process, &QProcess::errorOccurred, this, [this](QProcess::ProcessError e)
  {
    if (e == QProcess::FailedToStart)
      lost(m_process->errorString());
  });
  connect(m_process, &QProcess::finished, this, [this]
  {
    lost(QString::fromLocal8Bit(m_process->readAllStandardError()).trimmed());
  });
  m_process->start(m_sigrok, args);

  open(QIODevice::ReadOnly);
  return true;
}

void SigrokDevice::lost(const QString &reason)
{
  setErrorString(reason.isEmpty() ? m_sigrok : QString("%1: %2").arg(m_sigrok, reason));
  Q_EMIT finished();
}


qint64 SigrokDevice::bytesAvailable() const
{
  qint64 avail = m_outLine.size();
  if (m_buffer.indexOf('\n') >= 0)
    avail += m_fixedLineLength;
  return avail;
}


qint64 SigrokDevice::readData(char *data, qint64 maxSize)
{
  // pads sigrok line to fixed length with spaces.
  // this way it is much easier and faster to decode afterwards.
  // m_outLine holds the not-yet-delivered remainder of the current padded
  // line, since callers (FrameReader) may read as little as one byte at a time.
  if (m_outLine.isEmpty())
  {
    int newlineIndex = m_buffer.indexOf('\n');
    if (newlineIndex < 0)
      return 0; // Not a complete line yet

    QByteArray line = m_buffer.left(newlineIndex);
    m_buffer.remove(0, newlineIndex + 1);

    line = line.trimmed();
    if (line.length() >= m_fixedLineLength - 1)
      line = line.left(m_fixedLineLength - 1);
    else
      line.prepend(QByteArray(m_fixedLineLength - 1 - line.length(), ' '));
    line.append('\n');

    m_outLine = line;
  }

  qint64 len = qMin(maxSize, qint64(m_outLine.size()));
  memcpy(data, m_outLine.constData(), len);
  m_outLine.remove(0, len);

#ifdef SIGROK_DEBUG
  qInfo() << QString::fromUtf8(data, len);
#endif

  return len;
}


qint64 SigrokDevice::writeData(const char *, qint64)
{
  return -1; // Read-only
}

void SigrokDevice::close()
{
  // Keyed off the process state rather than isOpen(), so a process that was
  // started by init() but never reached QIODevice::open() still gets reaped.
  if (m_process != nullptr)
  {
    QProcess *p = std::exchange(m_process, nullptr);
    p->disconnect(this);
    // later: close() may run from inside one of the process's own signals
    if (p->state() == QProcess::NotRunning)
      p->deleteLater();
    else
    {
      // sigrok-cli ends in its own time instead of blocking the event loop
      // (and every other meter); the application owns it until then
      p->setParent(QCoreApplication::instance());
      connect(p, &QProcess::finished, p, &QObject::deleteLater);
      p->terminate();
      QTimer::singleShot(2000, p, [p] { p->kill(); });
    }
  }

  m_buffer.clear();
  m_outLine.clear();

  if (QIODevice::isOpen())
    QIODevice::close();
}


void SigrokDevice::onProcessReadyRead() {
  m_buffer.append(m_process->readAllStandardOutput());
  Q_EMIT readyRead();
}


