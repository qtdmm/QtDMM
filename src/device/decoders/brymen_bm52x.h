// Copyright (c) 2026 The QtDMM developers
// Copyright (c) 2014 Aurelien Jacobs <aurel@gnuage.org> (libsigrok bm52x.c)
// Copyright (c) 2019-2020 Gerhard Sittig <gerhard.sittig@gmx.net> (libsigrok bm52x.c)
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "device/dmmdecoder.h"

/// Brymen BM52x (BM525s) and BM82x (BM829s): 24 bytes binary over the
/// BU-86X adapter, polled with "00 52 66" / "00 82 66"; bytes 16..19 carry
/// the model id (0x52 / 0x82). Two displays: main digits in bytes 3..6
/// (indicators in 2, 13, 14), secondary in 8..11 (indicators in 7, 11, 12,
/// 13). LCD segments are bgcpafed with the decimal point in bit 4. Ported
/// from the live-reading part of libsigrok's src/dmm/bm52x.c (the meters'
/// recording memory is not read); no capture yet.
class DecoderBrymenBM52x : public DmmDecoder
{
  Q_OBJECT
public:
  DecoderBrymenBM52x(FrameFormat::DataFormat df) : DmmDecoder(df) { m_name = df == FrameFormat::BrymenBM82x ? "BM82x" : "BM52x"; }

  std::optional<DmmDecoder::DmmResponse> decode(const QByteArray &data, int id) override;
  bool checkFormat(const char* data, size_t idx) override;
  size_t getPacketLength() override;
  QByteArray pollRequest() const override;

  /// Character of one segment byte (bit 4 ignored), '\0' for blank/unknown.
  static char digit(unsigned char b);
  /// Text of a display block of four digits: @p pkt[0] holds the sign flag,
  /// digits follow; a C/F in the last digit goes to @p tempUnit.
  static QString digits(const unsigned char *pkt, unsigned char signFlag, QChar *tempUnit);

private:
  unsigned char modelId() const { return m_type == FrameFormat::BrymenBM82x ? 0x82 : 0x52; }
};
