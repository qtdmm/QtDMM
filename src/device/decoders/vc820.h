// Copyright (c) 2001 Matthias Toussaint
// Copyright (c) 2025 The QtDMM developers
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "device/dmmdecoder.h"

/// Voltcraft VC820/840, Uni-Trend UT60A/E and clones (FS9721 chip): 14
/// bytes, each carrying a 4-bit sequence number in the high nibble and four
/// segment bits in the low nibble. Spec in docs/protocols/spec/vc820.yaml.
/// VC820BLE is the same frame over Bluetooth LE (HoldPeak HP-570C-APP).
class DecoderVC820 : public DmmDecoder
{
    Q_OBJECT
public:
  DecoderVC820(FrameFormat::DataFormat df) : DmmDecoder(df) {m_name="VC820";}

  std::optional<DmmDecoder::DmmResponse> decode(const QByteArray &data, int id) override;
  bool checkFormat(const char* data, size_t idx) override;
  size_t getPacketLength() override;

private:
  static constexpr int kFrameLength = 14;
  const char* vc820Digit(int byte);
};
