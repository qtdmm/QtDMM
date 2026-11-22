// Copyright (c) 2026 The QtDMM developers
// SPDX-License-Identifier: GPL-3.0-or-later
#include <QCoreApplication>
#include <QDebug>

#include "device/transports/buffereddevice.h"
#include "testutil.h"

class TestBufferedDevice : public BufferedDevice
{
public:
  using BufferedDevice::appendToBuffer;
  using BufferedDevice::replaceBuffer;

protected:
  qint64 writeData(const char *, qint64) override { return -1; }
};

int main(int argc, char **argv)
{
  QCoreApplication app(argc, argv);
  TestBufferedDevice device;
  device.open(QIODevice::ReadOnly);

  const QByteArray full(BufferedDevice::kMaxBufferedBytes, 'x');
  check(device.appendToBuffer(full), "buffer accepts data up to its limit");
  check(device.bytesAvailable() == BufferedDevice::kMaxBufferedBytes, "all buffered bytes are available");
  check(!device.appendToBuffer("x", 1), "buffer refuses data beyond its limit");
  check(device.bytesAvailable() == BufferedDevice::kMaxBufferedBytes, "overflow leaves buffered data unchanged");
  check(!device.replaceBuffer(QByteArray(BufferedDevice::kMaxBufferedBytes + 1, 'y')),
        "buffer refuses an oversized replacement");
  check(device.bytesAvailable() == BufferedDevice::kMaxBufferedBytes, "failed replacement leaves buffered data unchanged");

  QByteArray received(full.size(), Qt::Uninitialized);
  check(device.read(received.data(), received.size()) == received.size(), "all accepted data can be read");
  check(received == full, "accepted bytes stay intact");
  check(device.appendToBuffer("y", 1), "buffer accepts new data after being drained");
  return failed ? 1 : 0;
}
