// Copyright (c) 2026 The QtDMM developers
// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "device/transports/buffereddevice.h"

#include <QStringList>
#include <QThread>
#include <atomic>

/// The blocking writes and reads of a UsbTmcDevice, in its own thread. It
/// owns the file descriptor and closes it when it is deleted (after its
/// thread has ended).
class UsbTmcWorker : public QObject
{
  Q_OBJECT
public:
  /// @p usbtmc: the descriptor is the kernel's usbtmc driver (its read
  /// returns after the driver's timeout); otherwise a stream such as a
  /// pseudo terminal, read with poll() (the tests).
  UsbTmcWorker(int fd, bool usbtmc) : m_fd(fd), m_usbtmc(usbtmc) {}
  ~UsbTmcWorker() override;
  /// Ends a wait for an answer at its next slice (100 ms); thread-safe.
  void stop() { m_stop.store(true); }
  /// Messages handed over and not done yet; the device counts up before it
  /// hands one over, send() counts down when done. Thread-safe.
  std::atomic<int> pending{0};

public Q_SLOTS:
  /// Writes @p message, one USBTMC message per line; reads the answer of
  /// each line that asks something ("?").
  void send(const QByteArray &message);

Q_SIGNALS:
  /// The answer to one question, ending with LF.
  void answer(const QByteArray &bytes);
  /// Writing or reading failed for good (the instrument is gone).
  void failed(const QString &what);

private:
  bool    writeLine(const QByteArray &line);
  /// The answer, empty when none came within the timeout.
  QByteArray readAnswer(bool *gone);

  int     m_fd;
  bool    m_usbtmc;
  std::atomic<bool> m_stop{false};
};

/// An instrument on USB-TMC (USB Test & Measurement Class): oscilloscopes,
/// bench meters, generators that speak SCPI over USB without a serial port.
/// Linux only for now: the kernel's usbtmc driver makes each one a
/// /dev/usbtmcN; every write() is one message, and after a question one
/// read() returns its whole answer or fails after the driver's timeout
/// (set to 1 s here). Port entry: "USBTMC /dev/usbtmc0".
///
/// FrameReader writes the decoder's poll requests; a UsbTmcWorker in its
/// own thread writes each line and reads the answer when the line is a
/// question - reading without a question pending confuses some
/// instruments, so a command without "?" is only written. An answer
/// without a line end gets an LF, so the line decoders (OpenOSC, OwonSPM)
/// frame it as from a serial port. A question that gets no answer is
/// nothing to report: FrameReader asks again, the connection's watchdog
/// notices silence. While one message waits for its answer, at most one
/// more is queued: an instrument that does not answer must not pile up
/// the poll requests. A failed write or read (the cable unplugged) ends the
/// device with finished(), MeterConnection reopens it later.
///
/// Elsewhere (macOS, Windows, FreeBSD) open() fails with a message: there
/// is no driver to talk to without libusb or VISA.
class UsbTmcDevice : public BufferedDevice
{
  Q_OBJECT
public:
  /// @p device: "USBTMC /dev/usbtmc0" or just the path.
  explicit UsbTmcDevice(QString device, QObject *parent = nullptr);
  ~UsbTmcDevice() override;

  bool open(OpenMode mode) override;
  void close() override;
  bool isSequential() const override { return true; }

  /// The path of a port entry ("USBTMC /dev/usbtmc0" -> "/dev/usbtmc0").
  static QString pathForEntry(const QString &entry);
  /// The instruments the usbtmc driver has, as port entries.
  static void availablePorts(QStringList &portlist);
  /// What sysfs says of /dev/usbtmcN: manufacturer, product, vid, pid
  /// (empty / 0 when not known).
  struct Info
  {
    QString manufacturer, product;
    quint16 vid = 0, pid = 0;
  };
  static Info info(const QString &path);
  /// "Linux only" elsewhere, for the message of a device that cannot be
  /// opened on this system.
  static bool supported();

Q_SIGNALS:
  /// The instrument is gone: a write or read failed while open.
  void finished();
  /// To the worker: a message to send (queued).
  void sendRequested(const QByteArray &message);

protected:
  qint64 readData(char *data, qint64 maxSize) override;
  qint64 writeData(const char *data, qint64 len) override;

private:
  void    onAnswer(const QByteArray &bytes);
  void    onFailed(const QString &what);
  void    stopWorker();

  QString m_path;
  bool    m_isOpen = false;
  QThread *m_thread = nullptr;
  UsbTmcWorker *m_worker = nullptr;
};
