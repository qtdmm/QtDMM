// Copyright (c) 2026 The QtDMM developers
// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <QByteArray>
#include <QHostAddress>
#include <QList>
#include <QMap>
#include <QObject>
#include <QPointer>
#include <QString>

#include <functional>

#include "core/sampletypes.h"
#include "service/scpidevice.h"
#include "service/scpiendpoint.h"

class QTcpServer;
class QTcpSocket;

/// A small SCPI instrument on a raw TCP socket (port 5025 by default), the
/// way bench meters and lxi-tools speak it: one program message per line,
/// commands separated by ';', queries answered in the same order. It turns
/// the meter QtDMM reads into a network instrument, so anything that talks
/// SCPI - lxi-tools, sigrok's scpi-dmm, a Python script - can read a
/// consumer multimeter.
///
/// The server only reports; it never writes to the meter. What it knows of
/// a meter - the readings, whether it is connected, whether the recorder
/// runs - is in its ScpiDevice, which also passes on the requests for the
/// recorder / connection. With several devices (addDevice()) INSTrument
/// chooses the one the commands go to. process() is the whole command
/// interpreter and is public so the test can drive it without sockets.
class ScpiServer : public QObject
{
  Q_OBJECT
public:
  /// The unit as every answer spells it: plain ASCII, one spelling per
  /// unit whatever the meter's protocol writes - "C", "F", "Ohm" for °C,
  /// °F ("dF") and Ω. Scripts compare against it.
  static QString unit(const QString &baseUnit);
  using Reading = ScpiDevice::Reading;
  using Stats = ScpiDevice::Stats;
  using RecordingState = ScpiDevice::RecordingState;
  using Point = ScpiDevice::Point;
  using AlarmState = ScpiDevice::AlarmState;
  using Sources = ScpiDevice::Sources;

  explicit ScpiServer(QObject *parent = nullptr);
  ~ScpiServer() override;

  /// Listens on @p address:@p port; when the port is taken the next
  /// @p tries ports are tried (several QtDMM instances on one machine).
  /// Returns false when none could be bound; errorString() says why.
  bool start(const QHostAddress &address, quint16 port, int tries = 10);
  void stop();
  bool isListening() const;
  quint16 port() const;
  QHostAddress address() const;
  int clientCount() const { return m_clients.size(); }
  QString errorString() const { return m_error; }

  /// The meters the commands can go to (INSTrument): @p device until it is
  /// removed or destroyed. The first one is chosen at first, and again when
  /// the chosen one goes; without any the server answers as for no meter.
  void addDevice(ScpiDevice *device);
  void removeDevice(ScpiDevice *device);
  /// The meter the commands go to now (INSTrument:SELect).
  ScpiDevice *selectedDevice() const;
  /// The SCPI names of the meters, in their order (INSTrument:CATalog?): the
  /// device's name in upper case, other characters than letters, digits and
  /// '_' as '_', "_2" for the second of a name.
  QStringList instrumentNames() const;

  /// One of "My devices" for QTDMM:DEVice:CATalog?.
  struct DeviceEntry
  {
    QString id;
    QString name;
    bool    open = false;   ///< a meter of this process uses it
  };
  /// What QTDMM:DEVice opens and closes the meters with (DeviceSession).
  /// open/close return an empty text or the error "<code>,<text>"; without
  /// them the commands are refused.
  struct DeviceControl
  {
    std::function<QList<DeviceEntry>()> catalog;
    std::function<QString(const QString &id)> open;
    std::function<QString(const QString &id)> close;
  };
  void setDeviceControl(DeviceControl control) { m_deviceControl = std::move(control); }

  /// Largest QTDMM:RECord:DATA? answer, in readings.
  static constexpr int kMaxDataPoints = 10000;

  /// Supplies the screen dump for HCOPy:SDUMp:DATA?: the main window
  /// encoded in @p format ("PNG", "BMP", "JPG"); empty when not possible.
  using ScreenshotSource = std::function<QByteArray(const QByteArray &format)>;
  void setScreenshotSource(ScreenshotSource source) { m_screenshot = std::move(source); }
  /// Wraps @p data as an IEEE 488.2 definite-length block ("#6123456...").
  static QByteArray block(const QByteArray &data);

  /// Runs one program message (without the terminator) and returns the
  /// response: the query answers joined by ';', with a trailing newline,
  /// or nothing when no command was a query. Errors go to the error
  /// queue (SYST:ERR?).
  QByteArray process(const QByteArray &message);
  /// The interpreter for the other transports (HiSLIP): process() and the
  /// status byte; valid as long as this object lives.
  ScpiEndpoint endpoint();
  /// IEEE 488.2 status byte: bit 2 (4) while the error queue holds errors.
  quint8 statusByte() const { return m_errors.isEmpty() ? 0 : 4; }

  /// Number of readings considered current: older ones answer as stale.
  static constexpr qint64 kStaleMs = 5000;

  /// Puts an error into the queue (SYST:ERR?); also for what the receiver
  /// of a command refuses (INIT ignored).
  void pushError(int code, const QString &text);

Q_SIGNALS:
  void clientsChanged(int count);

private:
  struct Command
  {
    QStringList path;     ///< mnemonics, upper case, numeric suffix stripped
    QList<int> suffix;    ///< numeric suffix per mnemonic (1 when absent)
    bool query = false;
    QString args;
  };

  void onNewConnection();
  void onReadyRead(QTcpSocket *socket);
  /// deleteLater(), or after the command when one is running (see m_running)
  void release(QTcpSocket *socket);
  QByteArray handle(const Command &cmd, bool &isQuery);
  QString handleText(const Command &cmd, bool &isQuery);
  QString handleCommon(const Command &cmd, bool &isQuery);
  QString handleReading(const Command &cmd, bool &isQuery);
  QString handleStatus(const Command &cmd, bool &isQuery);
  /// CALCulate:LIMit (34465A), CALCulate:FUNCtion LIMit and CALCulate:STATe
  /// (34410A): the PASS/FAIL limit test.
  QString handleLimit(const Command &cmd, bool &isQuery);
  QString handleSystem(const Command &cmd, bool &isQuery);
  QString handleInstrument(const Command &cmd, bool &isQuery);
  /// The meter the commands go to: the chosen one, or an empty stand-in.
  ScpiDevice &dev() const;
  /// A refused command: @p code queued, nothing to answer (isQuery false).
  QString refuse(bool &isQuery, int code, const QString &text);
  /// False and -100 queued when @p cmd is not a query, resp. is one.
  bool queryOnly(const Command &cmd, bool &isQuery);
  bool commandOnly(const Command &cmd, bool &isQuery);
  static bool matches(const QString &mnemonic, const char *longForm);
  static QString number(double v);
  QString function(const Reading &r) const;
  QString handleQtdmm(const Command &cmd, bool &isQuery);
  QByteArray recordingData(const Command &cmd, bool &isQuery);
  static QString unquote(const QString &arg);
  /// The arguments split at the commas outside quotes, each unquote()d.
  static QStringList quotedArgs(const QString &args);
  /// The channels of a query: its channel list "(@1,3)", "(@1:3)" in the
  /// arguments, else @p suffix (READ2?). Empty with an error queued when the
  /// list is broken (-104) or names a channel the meter has not (-222).
  QList<int> channels(const Command &cmd, int suffix);

  QTcpServer *m_server;
  QList<QTcpSocket *> m_clients;
  /// A command may spin an event loop (a dialog); a client that goes
  /// meanwhile waits in m_released, deleting its socket there would pull it
  /// from under its own readyRead.
  int m_running = 0;
  QList<QTcpSocket *> m_released;
  QString m_error;
  QList<QPointer<ScpiDevice>> m_devices;
  int m_selected = 0;                ///< index into m_devices (INSTrument:NSELect - 1); -1: the one chosen was closed
  ScpiDevice *m_none;                ///< answers when there is no meter
  DeviceControl m_deviceControl;
  QList<QPair<int, QString>> m_errors;
  ScreenshotSource m_screenshot;
  QByteArray m_screenshotFormat = "PNG";
  QStringList m_lastPath;   ///< for the "compound header" rule of ';'
};
