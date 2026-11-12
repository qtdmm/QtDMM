// Copyright (c) 2026 The QtDMM developers
// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <QIODevice>

#include <cstring>

/// A read-only stream whose bytes arrive by signal (a HID report, a Bluetooth
/// notification, a timer, a socket) and wait in the bounded receive buffer
/// until FrameReader reads them. Subclasses append through appendToBuffer()
/// and emit readyRead().
class BufferedDevice : public QIODevice
{
public:
  static constexpr qsizetype kMaxBufferedBytes = 1 << 20;

  using QIODevice::QIODevice;

  /// Must stay const: QIODevice's version is const and virtual, so a
  /// non-const one would hide instead of override it and FrameReader
  /// (holding a QIODevice*) would get the base version.
  qint64 bytesAvailable() const override { return m_rx.size() + QIODevice::bytesAvailable(); }
  bool hasBufferedData() const { return !m_rx.isEmpty(); }

protected:
  void clearBuffer() { m_rx.clear(); }

  bool replaceBuffer(const QByteArray &bytes)
  {
    if (bytes.size() > kMaxBufferedBytes)
      return false;
    m_rx = bytes;
    return true;
  }

  bool appendToBuffer(const QByteArray &bytes)
  {
    if (bytes.size() > kMaxBufferedBytes - m_rx.size())
      return false;
    m_rx.append(bytes);
    return true;
  }

  bool appendToBuffer(const char *data, qsizetype size)
  {
    if (size < 0 || size > kMaxBufferedBytes - m_rx.size())
      return false;
    m_rx.append(data, size);
    return true;
  }

  qint64 readData(char *data, qint64 maxSize) override
  {
    const qint64 len = qMin(maxSize, qint64(m_rx.size()));
    if (len <= 0)
      return 0;
    std::memcpy(data, m_rx.constData(), size_t(len));
    m_rx.remove(0, len);
    return len;
  }

private:
  QByteArray m_rx;   ///< received, not read yet
};
