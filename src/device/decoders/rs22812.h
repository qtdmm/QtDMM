// Copyright (c) 2001 Matthias Toussaint
// Copyright (c) 2025 The QtDMM developers
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once
#include <QString>
#include <optional>

#include "device/dmmdecoder.h"

/// RadioShack 22-812: 9 bytes, seven-segment encoded digits, last byte is a
/// checksum (sum of the first eight plus 57).
class DecoderRS22812 : public DmmDecoder
{
  Q_OBJECT
public:
  DecoderRS22812(FrameFormat::DataFormat df) : DmmDecoder(df) {}

  std::optional<DmmDecoder::DmmResponse> decode(const QByteArray &data, int id) override;
  bool checkFormat(const char* data, size_t idx) override;
  size_t getPacketLength() override;

private:
std::optional<QString> digit(uint8_t byte);
};
