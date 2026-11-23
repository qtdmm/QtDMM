// Copyright (c) 2026 The QtDMM developers
// Copyright (c) 2014 Aurelien Jacobs <aurel@gnuage.org> (libsigrok bm86x.c)
// Copyright (c) 2019 Gerhard Sittig <gerhard.sittig@gmx.net> (libsigrok bm86x.c)
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "device/dmmdecoder.h"

/// Brymen BM86x (BM867s, BM869s): 24 bytes binary over the BU-86X IR/USB
/// adapter (HID). The meter answers to a request with three 8-byte reports;
/// bytes 16..19 are 0x86 (the model id, used for synchronisation). Two
/// displays: main (bytes 2..8, with function/prefix/flag bits in 1, 8, 14,
/// 15) and secondary (bytes 9..13, its own unit bits). LCD segments come in
/// the order bgcdafe in bits 7:1, the decimal point of a digit in bit 0 of
/// the next byte. Ported from libsigrok's src/dmm/bm86x.c; no capture yet.
class DecoderBrymenBM86x : public DmmDecoder
{
  Q_OBJECT
public:
  DecoderBrymenBM86x(FrameFormat::DataFormat df) : DmmDecoder(df) { m_name = "BM86x"; }

  std::optional<DmmDecoder::DmmResponse> decode(const QByteArray &data, int id) override;
  bool checkFormat(const char* data, size_t idx) override;
  size_t getPacketLength() override;
  /// "00 86 66" - the live-reading request the BU-86X forwards to the meter.
  QByteArray pollRequest() const override { return QByteArray::fromHex("008666"); }

  /// Character of one segment byte (bits 7:1), '\0' for blank/unknown.
  static char digit(unsigned char b);
  /// Text of a display block: @p pkt[0] holds indicators (sign = @p signFlag),
  /// the digits follow. A trailing C/F is returned in @p tempUnit, not in the text.
  static QString digits(const unsigned char *pkt, int count, unsigned char signFlag, QChar *tempUnit);
};
