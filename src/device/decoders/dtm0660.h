// Copyright (c) 2025 The QtDMM developers
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "device/dmmdecoder.h"

/// DTM0660 chip (generic 4000/6000/8000 count meters): 15 bytes, VC820-style
/// nibbles with a seven-segment digit encoding.
class DecoderDTM0660 : public DmmDecoder
{
  Q_OBJECT
public:
  DecoderDTM0660(FrameFormat::DataFormat df) : DmmDecoder(df) {}

  std::optional<DmmDecoder::DmmResponse> decode(const QByteArray &data, int id) override;
  bool checkFormat(const char* data, size_t idx) override;
  size_t getPacketLength() override;

private:
  const char* digit(int byte);
};
