// Copyright (c) 2026 The QtDMM developers
// Copyright (c) 2014 Janne Huttunen <jahuttun@gmail.com> (libsigrok bm25x.c)
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "device/dmmdecoder.h"

/// Brymen BM25x (BM250/BM251/BM252/BM257 ...): 15 bytes binary, continuous,
/// 9600 8N1 over the IR cable, RTS and DTR driven. Byte 0 is 0x02, bytes
/// 1..14 carry their index in the high nibble and four data bits in the
/// low nibble (like the FS9721 frame): seven-segment digits, decimal point,
/// sign, SI prefix, function and the AUTO/DC/AC/REL/HOLD/MIN/MAX flags.
/// Ported from libsigrok's src/dmm/bm25x.c; no capture available yet.
class DecoderBrymenBM25x : public DmmDecoder
{
  Q_OBJECT
public:
  DecoderBrymenBM25x(FrameFormat::DataFormat df) : DmmDecoder(df) { m_name = "BM25x"; }

  std::optional<DmmDecoder::DmmResponse> decode(const QByteArray &data, int id) override;
  bool checkFormat(const char* data, size_t idx) override;
  size_t getPacketLength() override;

  /// Character for digit @p num (0..3) of the frame, '?' for an unknown
  /// segment pattern. Digits are 0-9, blank, '-', 'L', 'C', 'F', 'E', 'n', 'r'.
  static QChar digit(const unsigned char *buf, int num);
};
