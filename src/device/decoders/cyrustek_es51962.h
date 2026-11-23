// Copyright (c) 2025 The QtDMM developers
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "device/dmmdecoder.h"

/// Cyrustek ES51962 chip (PeakTech 3315, Uni-Trend UT70B): 11 bytes, sent
/// twice per measurement, only the second copy is decoded. Spec in
/// docs/protocols/spec/cyrustek_es51962.yaml.
class DecoderCyrusTekES51962 : public DmmDecoder
{
  Q_OBJECT
public:
  DecoderCyrusTekES51962(FrameFormat::DataFormat df) : DmmDecoder(df) {};

  std::optional<DmmDecoder::DmmResponse> decode(const QByteArray &data, int id) override;
  bool checkFormat(const char* data, size_t idx) override;
  size_t getPacketLength() override;
};
