//======================================================================
// File:		dmm.h
// Author:	Matthias Toussaint
// Created:	Tue Apr 10 15:10:49 CEST 2001
//----------------------------------------------------------------------
// This file is part of QtDMM.
//
// QtDMM is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
//
// QtDMM is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with QtDMM.  If not, see <http://www.gnu.org/licenses/>.
//----------------------------------------------------------------------
// Copyright (c) 2001 Matthias Toussaint
//======================================================================

#pragma once

#include <QtCore>
#include <QtSerialPort>

#include "device/framereader.h"
#include "device/frameformat.h"
#include "device/dmmdecoder.h"
#include "device/transport.h"

/// The connection to one multimeter: port, frame reader and decoder.
///
/// InstanceWidget configures it from the settings (setDmmInfo(), setDevice(),
/// setFormat(), ...) and calls open(). From then on every decoded
/// reading arrives through value() and every change of the connection state
/// through linkStateChanged() - and, as a human-readable message, through
/// error(), which the main window shows in its status bar: "Connecting",
/// "Connected", "Timeout", the HID hint, ...
///
/// The state is owned here: a one-second watchdog turns silence into
/// Timeout, a vanished port (USB unplugged, bridge gone, socket refused)
/// into Error, and while the user still wants the connection the port is
/// reopened every few seconds. Internally a Transport owns the QIODevice,
/// a FrameReader cuts the byte stream into frames and the DmmDecoder chosen
/// by the format decodes them.
class MeterConnection : public QObject
{
  Q_OBJECT

public:
  /// Where the connection stands; error() carries the matching text.
  enum class LinkState
  {
    Closed,       ///< no port, the user did not ask for one (or close()d it)
    Connecting,   ///< port open, waiting for the first frame
    Connected,    ///< frames arrive
    Timeout,      ///< port open but silent for longer than timeout()
    Error         ///< the port went away; reopened automatically while wanted
  };
  Q_ENUM(LinkState)

  MeterConnection(QObject *parent);
  /// Port to open: a serial device path, an entry from
  /// Transport::availablePorts() or a host:port for RFC2217.
  void    setDevice(const QString &);
  /// Creates, opens and configures the port and starts reading. On failure
  /// error() carries the reason and false is returned.
  bool    open();
  /// Stops reading and closes the port; emits error("Not connected").
  void    close();
  /// Presses the meter's key @p key once (DmmDecoder::keyRequest()). False
  /// when the meter has no such key, or is not connected (a Bluetooth link
  /// still being set up).
  bool    sendKey(const QString &key);
  /// The keys of DmmDecoder::keyNames() the meter has (DmmDecoder::hasKey());
  /// empty for a meter without.
  QStringList keys() const;
  void    setName(const QString &name)  {  m_name = name; }
  /// The meter description; passed on to the port device and the reader.
  /// With DMMInfo::externalSetup the port is used as is and left open on
  /// close(), for setups where another tool configures it.
  void    setDmmInfo(const DmmDecoder::DMMInfo &info)  {  m_dmmInfo = info; }
  const DmmDecoder::DMMInfo &dmmInfo() const  {  return m_dmmInfo; }
  /// The last message emitted through error().
  QString errorString() const  { return m_error; }
  LinkState linkState() const { return m_state; }
  /// linkState() in a word or two ("connecting ...", "no data"), for the
  /// sidebar; empty while frames arrive.
  QString shortState() const;
  /// open() called and not close()d: the connection is wanted, even while
  /// it is lost and being reopened.
  bool    isWanted() const { return m_wanted; }
  /// Silence longer than this (ms) is a Timeout; default 3000.
  void    setTimeout(int ms) { m_timeoutMs = ms; }
  int     timeout() const { return m_timeoutMs; }
  /// Seconds between reopen attempts after the port was lost; 0 disables.
  void    setReconnectInterval(int seconds) { m_reconnectSeconds = seconds; }
  bool    isOpen() const;
  /// The protocol of the current decoder, Invalid before setFormat().
  FrameFormat::DataFormat format() const { return m_decoder ? m_decoder->getType() : FrameFormat::Invalid; }
  /// Selects the decoder (see DmmDecoder::getInstance()).
  void    setFormat(FrameFormat::DataFormat);
  /// Frames per reading, see FrameReader::setFramesPerReading().
  void    setFramesPerReading(int frames);
  /// --debug: dump every frame as hex to stdout.
  void    setConsoleLogging(bool on) { m_consoleLogging = on; }
  /// The inputs of a calculated value (PortType::Calc, CalcDevice).
  void    setCalcInputs(CalcDevice::Inputs inputs);

Q_SIGNALS:
  /// The whole decoded frame, for ReadingAdapter (MeterController); value()
  /// is the same split into its values.
  void    response(const DmmDecoder::DmmResponse &response);
  /// One value of a decoded reading, once per DmmDecoder::DmmChannel of the
  /// frame; the parameters mirror the channel and the frame's annunciators.
  void    value(double dval, const QString &val, const QString &unit, const QString &special,
                const QString &range, bool hold, bool showBar, int id);
  /// Connection state as a message for the status bar; despite the name
  /// also "Connecting ..." and "Connected".
  void    error(const QString &);
  /// The connection state changed; @p detail is the status bar text.
  void    linkStateChanged(MeterConnection::LinkState state, const QString &detail);

protected:
  void                  initDecoder( FrameFormat::DataFormat df);
  /// Platform-specific advice when the port cannot be opened for lack of
  /// permission (the dialout/dialer group on Unix).
  QString               permissionHint() const;
  /// Status bar text while readings arrive: "connected (USB)", the port on a
  /// second line for the tooltip.
  QString               connectedMessage() const;
  /// Status bar text for a silent port (with the HID hint where it applies).
  QString               timeoutMessage() const;
  /// Sets the state and its message; emits linkStateChanged()/error() on change.
  void                  setState(LinkState state, const QString &message);
  /// Creates and opens the port; the common part of open() and a reconnect.
  bool                  openPort();
  /// Drops the port without touching m_wanted.
  void                  closePort();
  /// The port device reports that it is gone (socket closed, USB unplugged).
  void                  portLost(QIODevice *from, const QString &reason);
  Transport          *m_portHandler;
  QString               m_device;
  /// m_device without secrets (the Bluetooth key), for messages.
  QString               deviceName() const;
  QString               m_error;
  FrameReader         *m_readerThread;
  LinkState             m_state = LinkState::Closed;
  bool                  m_wanted = false;       ///< open() called and not close()d
  int                   m_timeoutMs = 3000;
  int                   m_reconnectSeconds = 5;
  int                   m_secondsInError = 0;
  QElapsedTimer         m_lastFrame;            ///< since the last complete frame (or open)
  QString               m_name;
  bool                  m_consoleLogging;
  int                   m_flags;
  int                   m_delayTimer;
  std::shared_ptr<DmmDecoder> m_decoder;
  DmmDecoder::DMMInfo   m_dmmInfo;
  Transport::PortType m_portType;

  void                  timerEvent(QTimerEvent *) Q_DECL_OVERRIDE;

protected Q_SLOTS:
  void readEventSLOT(const QByteArray &str, int id, qint64 t, qint64 wall);

};

