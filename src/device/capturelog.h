// Copyright (c) 2026 The QtDMM developers
// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <QByteArray>
#include <QDateTime>
#include <QList>
#include <QString>
#include <QStringList>
#include <optional>

#include "device/dmmdecoder.h"
#include "device/transport.h"

/// The frames of one meter for a report of what QtDMM reads wrong: the last
/// ones as they came (MeterConnection::frame()), and those kept with what the
/// meter showed then. Written as YAML in the form of the test vectors in
/// docs/protocols/spec, so a frame and its display go into a spec as they are.
///
/// Only the frames go into the file, never the port, a Bluetooth address or
/// key: the frames are what the decoder sees, after the transport.
class CaptureLog
{
public:
  /// One frame, with what the decoder made of it (none: not understood).
  struct Frame
  {
    qint64     wall = 0;   ///< ms since the epoch
    QByteArray bytes;
    std::optional<DmmDecoder::DmmResponse> response;
  };
  /// A frame kept, with what the meter showed (typed in by the user).
  struct Kept
  {
    Frame   frame;
    QString shown;
    QString step;       ///< the guided step it was kept in (CaptureSteps), or empty
    bool    unexpected = false;   ///< QtDMM read something else than the step wants
  };
  /// The meter and how it is read, for the file's head.
  struct Header
  {
    QString vendor;
    QString model;
    FrameFormat::DataFormat protocol = FrameFormat::Invalid;
    Transport::PortType connection = Transport::PortType::None;
    int     framesPerReading = 1;
    QString version;     ///< QtDMM's
    QDateTime date;
  };

  /// Frames kept in the list; the oldest go.
  static constexpr int kFrames = 500;

  void        add(Frame frame);
  const QList<Frame> &frames() const { return m_frames; }
  void        clear() { m_frames.clear(); }
  /// Keeps frame @p index of frames() (-1: the newest) with @p shown; false
  /// when there is none.
  bool        keep(const QString &shown, int index = -1);
  /// Keeps @p frame (one that was in the list) with @p shown.
  void        keep(const Frame &frame, const QString &shown);
  /// Marks the frame kept last as kept in guided step @p step; @p unexpected
  /// when QtDMM read something else than the step wants.
  void        setKeptStep(const QString &step, bool unexpected);
  void        removeKept(int index);
  const QList<Kept> &kept() const { return m_kept; }
  /// The guided steps skipped (the meter has no such function), for the file.
  void        setSkipped(const QStringList &steps) { m_skipped = steps; }
  const QStringList &skipped() const { return m_skipped; }

  /// The file: head, the kept frames as vectors and with @p allFrames the
  /// list too.
  QString     yaml(const Header &header, bool allFrames) const;

  /// What QtDMM read in @p frame, read as @p format reads it, in a line as
  /// a display shows it ("0.270 nF AUTO", "-1.234 V DC AUTO HOLD"): the
  /// value without surplus leading zeros, the unit with its prefix and the
  /// coupling, none of the decoders' codes (CA, OH, DI ...); empty when it
  /// did not understand it.
  static QString readingText(const Frame &frame, FrameFormat::DataFormat format);
  /// "qtdmm-capture-uni-t-ut61e-2026-11-09-1403.yaml".
  static QString fileName(const QString &vendor, const QString &model, const QDateTime &date);
  /// The kind of connection in a word ("serial", "Bluetooth LE"), no address.
  static QString connectionName(Transport::PortType type);

private:
  QList<Frame> m_frames;
  QList<Kept>  m_kept;
  QStringList  m_skipped;
};
