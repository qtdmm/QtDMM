// Copyright (c) 2001 Matthias Toussaint
// Copyright (c) 2025 The QtDMM developers
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "device/dmmdecoder.h"

/// Cyrustek ES51986 chip (Uni-Trend UT803, Iso-Tech IDM 73, Tenma 72-1016):
/// 11 bytes, sent twice per measurement, only the second copy is decoded.
/// Spec in docs/protocols/spec/cyrustek_es51986.yaml.
class DecoderCyrusTekES51986 : public DmmDecoder
{
  Q_OBJECT
public:
  DecoderCyrusTekES51986(FrameFormat::DataFormat df) : DmmDecoder(df) {}

  std::optional<DmmDecoder::DmmResponse> decode(const QByteArray &data, int id) override;
  bool checkFormat(const char* data, size_t idx) override;
  size_t getPacketLength() override;
};
