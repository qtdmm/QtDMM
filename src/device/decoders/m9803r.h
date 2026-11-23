// Copyright (c) 2001 Matthias Toussaint
// Copyright (c) 2025 The QtDMM developers
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "device/dmmdecoder.h"

/// Mastech M9803R and clones (ELV M9803R, McVoice M-980T): 11 binary bytes
/// (sign, four digit bytes, mode, range, ...) terminated by CR LF.
class DecoderM9803R : public DmmDecoder
{
    Q_OBJECT
public:
  DecoderM9803R(FrameFormat::DataFormat df) : DmmDecoder(df) {}

  std::optional<DmmDecoder::DmmResponse> decode(const QByteArray &data, int id) override;
  bool checkFormat(const char* data, size_t idx) override;
  size_t getPacketLength() override;
};
