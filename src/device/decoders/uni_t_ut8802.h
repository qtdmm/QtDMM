// Copyright (c) 2026 The QtDMM developers
// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "device/dmmdecoder.h"

/// UNI-T UT8802 and UT8802N bench meters over their USB port, a CP2110 HID
/// UART at 9600 8N1. The meter streams unasked.
///
/// Frames of 8 bytes without a sum: `AC`, the dial position (function and
/// range in one code, 0x01-0x2D; the range gives the prefix), five BCD
/// digits (byte 4 low nibble the first, then bytes 3 and 2, high nibble
/// first; 0x0C "L"), the decimal point (byte 5 low nibble, digits after
/// it), a byte not read, the status: bit 0 MIN, 1 MAX, 2 manual range,
/// 3 REL, 4 HOLD, 6 OL, 7 minus. No remote keys.
///
/// Source: dmm-tools (GPL-3.0-or-later), docs/research/uci-bench-family/
/// reverse-engineered-protocol.md §3 - from UNI-T's UCI SDK (uci.dll) and
/// programming manual; no meter read there yet. Spec with vectors in
/// docs/protocols/spec/uni_t_ut8802.yaml.
class DecoderUT8802 : public DmmDecoder
{
  Q_OBJECT
public:
  DecoderUT8802(FrameFormat::DataFormat df) : DmmDecoder(df) { m_name = "UniTUT8802"; }

  std::optional<DmmDecoder::DmmResponse> decode(const QByteArray &data, int id) override;
  bool checkFormat(const char *data, size_t idx) override;
  size_t getPacketLength() override { return 8; }
};
