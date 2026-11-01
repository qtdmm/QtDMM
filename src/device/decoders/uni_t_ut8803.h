// Copyright (c) 2026 The QtDMM developers
// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "device/dmmdecoder.h"

/// UNI-T UT8803, UT8803E and UT8803N bench meters (with LCR functions) over
/// their USB port, a CP2110 HID UART at 9600 8N1. The meter streams unasked.
///
/// Frames of 21 bytes: `AB CD`, a byte, the type (0x02), the mode (0x00-
/// 0x16, ACV to duty, with L, Q, R and C, D, R), the range ('0'-based: with
/// the mode the prefix), a byte, the display as 5 ASCII characters (with
/// point, "OL"), two bytes, the status bytes: 14 HOLD, OL, minus; 15 REL,
/// manual range; 16 MIN, MAX; then the test frequency and diode direction
/// (not read), a 16-bit BE sum of bytes 0-18. No remote keys.
///
/// Source: dmm-tools (GPL-3.0-or-later), docs/research/ut8803/
/// reverse-engineered-protocol.md - from UNI-T's UCI SDK (uci.dll), its
/// programming manual and the user manual; no meter read there yet. Spec
/// with vectors in docs/protocols/spec/uni_t_ut8803.yaml.
class DecoderUT8803 : public DmmDecoder
{
  Q_OBJECT
public:
  DecoderUT8803(FrameFormat::DataFormat df) : DmmDecoder(df) { m_name = "UniTUT8803"; }

  std::optional<DmmDecoder::DmmResponse> decode(const QByteArray &data, int id) override;
  bool checkFormat(const char *data, size_t idx) override;
  size_t getPacketLength() override { return 21; }
};
