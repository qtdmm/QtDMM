// Copyright (c) 2001 Matthias Toussaint
// Copyright (c) 2025 The QtDMM developers
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "device/dmmdecoder.h"

/// Digitech QM1537 / Uni-Trend UT61B-D / TekPower TP4000ZC family (FS9922
/// chip): 14 bytes. Spec in docs/protocols/spec/qm1537.yaml.
class DecoderQM1537 : public DmmDecoder
{
    Q_OBJECT
public:
  DecoderQM1537(FrameFormat::DataFormat df) : DmmDecoder(df) {}

  std::optional<DmmDecoder::DmmResponse> decode(const QByteArray &data, int id) override;
  bool checkFormat(const char* data, size_t idx) override;
  size_t getPacketLength() override;
};
