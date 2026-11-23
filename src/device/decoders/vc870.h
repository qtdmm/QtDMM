// Copyright (c) 2025 The QtDMM developers
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "device/dmmdecoder.h"

/// Voltcraft VC870: 23 bytes; function and range codes in the first bytes
/// select per-range scale factors, main and secondary value as ASCII digits.
class DecoderVC870 : public DmmDecoder
{
    Q_OBJECT
public:
  DecoderVC870(FrameFormat::DataFormat df) : DmmDecoder(df)  {}

  std::optional<DmmDecoder::DmmResponse> decode(const QByteArray &data, int id) override;
  bool checkFormat(const char* data, size_t idx) override;
  size_t getPacketLength() override;
};
