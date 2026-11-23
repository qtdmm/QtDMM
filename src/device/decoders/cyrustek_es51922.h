// Copyright (c) 2025 The QtDMM developers
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "device/dmmdecoder.h"

/// Cyrustek ES51922 chip (Uni-Trend UT61E, Wintex TD2200): 14 bytes,
/// digits as ASCII, range/function/status as bit fields. Spec in
/// docs/protocols/spec/cyrustek_es51922.yaml.
class DecoderCyrusTekES51922 : public DmmDecoder
{
  Q_OBJECT
public:
  DecoderCyrusTekES51922(FrameFormat::DataFormat df) : DmmDecoder(df) {};

  std::optional<DmmDecoder::DmmResponse> decode(const QByteArray &data, int id) override;
  bool checkFormat(const char* data, size_t idx) override;
  size_t getPacketLength() override;
};
