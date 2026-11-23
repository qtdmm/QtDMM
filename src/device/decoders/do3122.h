// Copyright (c) 2025 The QtDMM developers
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "device/dmmdecoder.h"

/// Duratool DO3122: 22 bytes, seven-segment encoded digits.
class DecoderDO3122 : public DmmDecoder
{
  Q_OBJECT
public:
  DecoderDO3122(FrameFormat::DataFormat df) : DmmDecoder(df) {}

  std::optional<DmmDecoder::DmmResponse> decode(const QByteArray &data, int id) override;
  bool checkFormat(const char* data, size_t idx) override;
  size_t getPacketLength() override;

private:
  const char* digit(int byte, bool *convOk);
};
