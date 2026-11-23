// Copyright (c) 2001 Matthias Toussaint
// Copyright (c) 2025 The QtDMM developers
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "device/dmmdecoder.h"

/// The ASCII protocol family: Metex14 (14 bytes, polled with 'D'),
/// PeakTech10, Voltcraft14Continuous, Voltcraft15Continuous and the
/// sigrok-cli text lines (30 bytes, see SigrokDevice). Frames are readable
/// text "<mode> <value> <unit>\r".
class DecoderAscii : public DmmDecoder
{
  Q_OBJECT
public:
  DecoderAscii(FrameFormat::DataFormat df) : DmmDecoder(df) {}

  std::optional<DmmDecoder::DmmResponse> decode(const QByteArray &data, int id) override;
  /// Metex14 meters answer to "D\n"; the others stream.
  QByteArray pollRequest() const override { return m_type == FrameFormat::Metex14 ? QByteArrayLiteral("D\n") : QByteArray(); }
  bool checkFormat(const char* data, size_t len) override;
  size_t getPacketLength() override;

private:
  bool decodeSigrok(QString str);
};
