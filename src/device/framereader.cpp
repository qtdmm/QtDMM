//======================================================================
// File:		readerthread.cpp
// Author:	Matthias Toussaint
// Created:	Sat Apr 14 12:44:00 CEST 2001
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

#include <QtGui>
#include <QtWidgets>
#include <QtSerialPort>

#include <stdio.h>

#include "device/framereader.h"
#include "core/sampletypes.h"



FrameReader::FrameReader(QObject *receiver) :
  QObject(receiver),
  m_readValue(false),
  m_format(FrameFormat::Invalid),
  m_length(0),
  m_sendRequest(true),
  m_id(0),
  m_framesPerReading(1),
  m_decoder(nullptr)
{
  m_port = nullptr;
}


void FrameReader::setHandle(QIODevice *handle)
{
  // Drop the previous port's connections first: without this they pile up on
  // every reconnect (socketNotifierSLOT would run once per connect), and a
  // still-open old port could fire readyRead after m_port was set to null.
  if (m_port)
    disconnect(m_port, nullptr, this, nullptr);

  m_port = handle;

  m_readValue = false;
  m_gapFrame.clear();
  if (m_gapTimer)
    m_gapTimer->stop();

  if (m_port)
  {
    connect(m_port, &QIODevice::readyRead, this, &FrameReader::socketNotifierSLOT);
    connect(m_port, &QIODevice::aboutToClose, this, &FrameReader::socketClose);
  }
}


void FrameReader::setDecoder(std::shared_ptr<DmmDecoder> decoder)
{
  m_decoder = decoder;
  m_gapFrame.clear();
  if (m_gapTimer)
    m_gapTimer->stop();
}

void FrameReader::socketClose()
{
  m_id = 0;
  m_gapFrame.clear();
  if (m_gapTimer)
    m_gapTimer->stop();
}

void FrameReader::readByGap()
{
  const QByteArray bytes = m_port->readAll();
  if (bytes.isEmpty())
    return;
  if (!m_gapTimer)
  {
    m_gapTimer = new QTimer(this);
    m_gapTimer->setSingleShot(true);
    connect(m_gapTimer, &QTimer::timeout, this, &FrameReader::emitGapFrame);
  }
  for (const char byte : bytes)
  {
    if (m_gapFrame.isEmpty())
    {
      // the time of the frame's first byte: the pause only ends it
      m_gapT = Sample::now();
      m_gapWall = QDateTime::currentMSecsSinceEpoch();
    }
    m_gapFrame.append(byte);
    if (m_gapFrame.size() >= kMaxGapFrame)
      emitGapFrame();
  }
  if (m_gapFrame.isEmpty())
    m_gapTimer->stop();
  else
    m_gapTimer->start(m_decoder->frameGapMs());
}

void FrameReader::emitGapFrame()
{
  if (m_gapFrame.isEmpty())
    return;
  const QByteArray frame = m_gapFrame;
  m_gapFrame.clear();
  m_sendRequest = true;
  Q_EMIT readEvent(frame, m_id, m_gapT, m_gapWall);
  m_id = (m_id + 1) % m_framesPerReading;
}

void FrameReader::start()
{
  QTimer *timer = new QTimer(this);
  connect(timer, &QTimer::timeout, this, &FrameReader::timer);
  timer->start(1000);
}
void FrameReader::timer()
{
  if (m_readValue)
  {
    sendReadRequest();
    m_readValue = false;
  }
}

void FrameReader::startRead()
{
  m_readValue = true;
  m_sendRequest = true;
}


void FrameReader::socketNotifierSLOT()
{
  if (m_decoder != nullptr && m_decoder->frameGapMs() > 0)
  {
    readByGap();
    return;
  }

  int r;
  char byte;

  const int64_t packetLength = (m_decoder == nullptr) ? 0 : m_decoder->getPacketLength();
  // the time of this readyRead(), not of the adaption: frames read together
  // share it, and the event loop may have held the bytes before
  const qint64 t = Sample::now();
  const qint64 wall = QDateTime::currentMSecsSinceEpoch();

  while ((r = m_port->read( &byte, 1)) > 0)
  {
    m_fifo[m_length] = byte;

    if (m_decoder != nullptr && m_decoder->checkFormat(m_fifo,m_length))
    {
      // A fixed-length protocol takes the last packetLength bytes; a
      // variable-length one (packetLength 0, the Fluke QM lines) everything
      // since the previous frame - m_length restarts at 0 after each frame,
      // so the frame begins at index 0.
      const int64_t bytesToRead = packetLength > 0 ? packetLength : m_length + 1;
      m_length = (m_length - bytesToRead + 1 + FIFO_LENGTH) % FIFO_LENGTH;

      for (int i = 0; i < bytesToRead; ++i)
      {
        m_buffer[i] = m_fifo[m_length];
        m_length = (m_length + 1) % FIFO_LENGTH;
      }

      m_sendRequest = true;
      m_length = 0;

      Q_EMIT readEvent( QByteArray(m_buffer,bytesToRead), m_id, t, wall);

      m_id = (m_id + 1) % m_framesPerReading;

      if (m_port->bytesAvailable() < bytesToRead) {
        break;
      }
    }
    else if (packetLength == 0 && m_length + 1 >= FIFO_LENGTH)
      m_length = 0;   // no terminator within the buffer: not a frame, start over
    else
      m_length = (m_length + 1) % FIFO_LENGTH;
  }
  // an empty read is nothing to report here: MeterConnection's watchdog turns silence
  // into Timeout, the port devices report their own loss
}

void FrameReader::sendReadRequest()
{
  // asked only when it is sent: a decoder may count its requests
  if (m_decoder == nullptr || !m_sendRequest)
    return;
  const QByteArray request = m_decoder->pollRequest();
  if (request.isEmpty())
    return;
  m_port->write(request);
  m_sendRequest = false;
}
