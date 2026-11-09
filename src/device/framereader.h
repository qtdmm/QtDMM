//======================================================================
// File:		readerthread.h
// Author:	Matthias Toussaint
// Created:	Sat Apr 14 12:42:06 CEST 2001
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
#include <QIODevice>
#include "device/frameformat.h"
#include "device/dmmdecoder.h"

class QSerialPort;

/// Collects bytes from the port and cuts them into decoder frames.
///
/// Despite the name this is no thread any more: it listens to
/// QIODevice::readyRead() of the port set with setHandle() and feeds every
/// byte into a ring buffer. After each byte the decoder's
/// DmmDecoder::checkFormat() is asked whether the buffer now ends with a
/// complete frame; if so the frame is emitted through readEvent() and the
/// value id advances (meters that send several lines per reading, see
/// setFramesPerReading()). For polled protocols (Metex14) a request is written to
/// the port once per second (start(), startRead()).
class FrameReader : public QObject
{
  Q_OBJECT
public:
  FrameReader(QObject *receiver);
  /// Starts the one-second timer that sends poll requests (see startRead()).
  void        start();
  /// Arms the next poll request; MeterConnection calls this from its own timer.
  void        startRead();
  /// Switches to a new port (or none). Connections to the old port are dropped.
  void        setHandle(QIODevice *handle);
  /// Protocol of the connected meter; decides whether polling is needed.
  void        setFormat(FrameFormat::DataFormat format) { m_format = format; };
  /// Decoder whose checkFormat()/getPacketLength() delimit the frames.
  void        setDecoder(std::shared_ptr<DmmDecoder> decoder);
  /// For a decoder that cuts frames by time (DmmDecoder::frameGapMs()): a
  /// frame is never longer than this, so a meter that never pauses still
  /// gives frames.
  static constexpr int kMaxGapFrame = 256;

  /// Number of frames one reading consists of; the id passed with
  /// readEvent() cycles through 0..num-1.
  void        setFramesPerReading(int num) { m_framesPerReading = num; }

Q_SIGNALS:
  /// A complete frame: exactly DmmDecoder::getPacketLength() bytes, or for a
  /// variable-length protocol (packet length 0) everything received since
  /// the previous frame, terminator included. @p t (Sample::now()) and
  /// @p wall (ms since the epoch) are when readyRead() came, in the main
  /// thread: a busy event loop still shifts them, frames of one read share
  /// them. The arrival itself would need a reader thread or the transport.
  void        readEvent(const QByteArray &, int id, qint64 t, qint64 wall);

protected:
  bool                  m_readValue;
  char                  m_fifo[FIFO_LENGTH];
  char                  m_buffer[FIFO_LENGTH];
  FrameFormat::DataFormat m_format;
  int                   m_length;
  bool                  m_sendRequest;
  int                   m_id;
  int                   m_framesPerReading;
  std::shared_ptr<DmmDecoder> m_decoder;
  void sendReadRequest();

protected Q_SLOTS:
  void socketNotifierSLOT();
  void socketClose();

private:
  QIODevice *m_port;
  QTimer    *m_gapTimer = nullptr;   ///< frameGapMs(): fires after the pause
  QByteArray m_gapFrame;             ///< frameGapMs(): the bytes since the last frame
  qint64     m_gapT = 0;             ///< frameGapMs(): Sample::now() of its first byte
  qint64     m_gapWall = 0;
  void       readByGap();
  void       emitGapFrame();

private Q_SLOTS:
  void timer();
};

