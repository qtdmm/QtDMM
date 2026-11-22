// Copyright (c) 2026 The QtDMM developers
// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "device/dmmdecoder.h"

/// The protocol of a meter QtDMM does not know yet ("Unknown (capture
/// only)"): it reads no value, it only cuts the bytes into frames at a pause
/// (frameGapMs()) for the capture window (CaptureDlg), whose export is the
/// start of a new decoder.
class DecoderRawCapture : public DmmDecoder
{
  Q_OBJECT
public:
  /// A pause this long ends a frame: a meter sends a frame in one go, and at
  /// 9600 baud 14 bytes take about 15 ms.
  static constexpr int kGapMs = 50;

  DecoderRawCapture(FrameFormat::DataFormat df) : DmmDecoder(df) { m_name = "RawCapture"; }

  std::optional<DmmDecoder::DmmResponse> decode(const QByteArray &, int) override { return std::nullopt; }
  bool checkFormat(const char *, size_t) override { return false; }
  size_t getPacketLength() override { return 0; }
  int frameGapMs() const override { return kGapMs; }
};
