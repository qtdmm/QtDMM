// Copyright (c) 2026 The QtDMM developers
// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "device/dmmdecoder.h"

/// TFA Dostmann AIRCO2NTROL (and other Holtek 04d9:a052 CO2 monitors).
/// HidHoltekDevice (src/device/transports/hidholtek.h) hands over three text
/// lines per set of readings, "<address hex> <raw dec>\n":
///
///   50 <raw>   CO2, id 0:          raw ppm
///   42 <raw>   temperature, id 1:  raw / 16 - 273.15 degC
///   41 <raw>   humidity, id 2:     raw / 100 %rH
///
/// The meter is registered with numValues 3, so the display shows all three:
/// CO2 as the main value, temperature and humidity below it.
class DecoderTfaAirControl : public DmmDecoder
{
  Q_OBJECT
public:
  DecoderTfaAirControl(FrameFormat::DataFormat df) : DmmDecoder(df) { m_name = "TfaAirControl"; }

  std::optional<DmmDecoder::DmmResponse> decode(const QByteArray &data, int id) override;
  bool checkFormat(const char *data, size_t idx) override { return data[idx] == '\n'; }
  size_t getPacketLength() override { return 0; }
};
