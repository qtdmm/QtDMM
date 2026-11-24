// Copyright (c) 2026 The QtDMM developers
// SPDX-License-Identifier: GPL-3.0-or-later
//
// UsbTmcDevice against a pseudo terminal standing in for /dev/usbtmcN (the
// ioctl of the usbtmc driver fails there, so the device waits with poll()):
// a question is written and its answer read, an answer without a line end
// gets an LF, a command without "?" is only written, two lines are two
// messages, the instrument going away ends the device with finished().
// The port entry, its type, and the udev rule's USB-TMC line.

#include <QCoreApplication>
#include <QFile>

#include <fcntl.h>
#include <poll.h>
#include <pty.h>
#include <termios.h>
#include <unistd.h>

#include "device/discovery/discovery.h"
#include "device/transport.h"
#include "device/transports/usbtmc.h"
#include "testutil.h"

namespace
{
/// What the instrument side received within @p ms, as text.
QByteArray received(int master, int ms = 1000)
{
  QByteArray got;
  QElapsedTimer t;
  t.start();
  while (t.elapsed() < ms)
  {
    pollfd p { master, POLLIN, 0 };
    if (::poll(&p, 1, 20) > 0)
    {
      char buf[256];
      const ssize_t n = ::read(master, buf, sizeof(buf));
      if (n > 0)
        got.append(buf, n);
      if (got.endsWith('\n'))
        break;
    }
    QCoreApplication::processEvents(QEventLoop::AllEvents, 5);
  }
  return got;
}

void answer(int master, const QByteArray &bytes)
{
  check(::write(master, bytes.constData(), size_t(bytes.size())) == bytes.size(), "the instrument answers");
}
}

int main(int argc, char **argv)
{
  QCoreApplication app(argc, argv);

  // the port entry and its type
  check(UsbTmcDevice::pathForEntry("USBTMC /dev/usbtmc0") == "/dev/usbtmc0", "entry: the path after USBTMC");
  check(UsbTmcDevice::pathForEntry("/dev/usbtmc1") == "/dev/usbtmc1", "entry: a bare path");
  check(Transport::str2portType("usbtmc") == Transport::PortType::UsbTmc, "port type usbtmc");
  check(udevRules().contains("KERNEL==\"usbtmc*\""), "udev rules: the USB-TMC line");
  QStringList ports;
  UsbTmcDevice::availablePorts(ports);   // whatever this machine has; must not fail
  for (const QString &p : ports)
    check(p.startsWith("USBTMC /dev/usbtmc"), "listed as USBTMC /dev/usbtmcN: " + p);

  int master = -1, slave = -1;
  char name[256] = {};
  if (::openpty(&master, &slave, name, nullptr, nullptr) != 0)
  {
    qWarning() << "no pseudo terminal";
    return 1;
  }
  termios raw {};
  ::tcgetattr(slave, &raw);
  ::cfmakeraw(&raw);
  ::tcsetattr(slave, TCSANOW, &raw);

  UsbTmcDevice device(QStringLiteral("USBTMC ") + QString::fromLocal8Bit(name));
  bool gone = false;
  QObject::connect(&device, &UsbTmcDevice::finished, [&gone] { gone = true; });
  check(device.open(QIODevice::ReadWrite), "opens the pseudo terminal: " + device.errorString());

  // a question and its answer, which gets an LF
  device.write("MEAS:ALL:INFO?\n");
  check(received(master) == "MEAS:ALL:INFO?\n", "the question is written with an LF");
  answer(master, "1.000 2.000 2.000 0 0 0 1");
  check(waitFor([&] { return device.bytesAvailable() > 0; }), "the answer arrives");
  waitFor([&] { return device.bytesAvailable() >= 26; }, 300);
  check(device.readAll() == "1.000 2.000 2.000 0 0 0 1\n", "the answer with an LF added");

  // a command: written, no answer awaited; the question after it is answered
  device.write("SYST:LOC\n");
  check(received(master) == "SYST:LOC\n", "a command is written");
  device.write("CONF:ALL?\n");
  check(received(master) == "CONF:ALL?\n", "the next question follows at once");
  answer(master, "VOLT:DC,+0.0011V,AUTO,2V\n");
  check(waitFor([&] { return device.bytesAvailable() > 0; }), "its answer arrives");
  waitFor([&] { return device.bytesAvailable() >= 25; }, 300);
  check(device.readAll() == "VOLT:DC,+0.0011V,AUTO,2V\n", "an answer with its own LF stays as it is");

  // two questions at once: two messages, each answered in turn
  device.write("*IDN?\nOUTP?\n");
  check(received(master) == "*IDN?\n", "the first line alone");
  answer(master, "OWON,SPM6103,1,FV:V1.0\n");
  check(received(master) == "OUTP?\n", "the second line after the first answer");
  answer(master, "1\n");
  check(waitFor([&] { return device.bytesAvailable() >= 25; }), "both answers arrive");
  check(device.readAll() == "OWON,SPM6103,1,FV:V1.0\n1\n", "both answers in order");

  // no answer: nothing arrives, the device stays open
  device.write("MEAS?\n");
  check(received(master) == "MEAS?\n", "an unanswered question is written");
  waitFor([] { return false; }, 1300);
  check(device.isOpen() && !gone && device.bytesAvailable() == 0, "no answer within the timeout: still open, nothing read");

  // a silent instrument: requests do not pile up behind the one waiting
  for (int i = 0; i < 10; ++i)
    device.write("MEAS?\n");
  check(received(master, 400) == "MEAS?\n", "silent: the first request is written");
  waitFor([] { return false; }, 1200);
  check(received(master, 400) == "MEAS?\n", "then one queued request");
  waitFor([] { return false; }, 1200);
  check(received(master, 300).isEmpty(), "and no more: the others were dropped");

  // the instrument goes away
  ::close(master);
  device.write("MEAS?\n");
  check(waitFor([&] { return gone; }, 3000), "the instrument gone: finished()");
  check(!device.isOpen() && !device.errorString().isEmpty(), "closed, with the reason: " + device.errorString());
  ::close(slave);

  // a path that is no device
  UsbTmcDevice none(QStringLiteral("USBTMC /dev/usbtmc-none"));
  check(!none.open(QIODevice::ReadWrite) && none.errorString().contains("/dev/usbtmc-none"), "no such device: its path in the error");

  if (failed == 0)
    qInfo() << "All USB-TMC tests passed.";
  return failed == 0 ? 0 : 1;
}
