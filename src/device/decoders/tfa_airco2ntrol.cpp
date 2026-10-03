// Copyright (c) 2026 The QtDMM developers
// SPDX-License-Identifier: GPL-3.0-or-later
#include "device/decoders/tfa_airco2ntrol.h"

#include <cmath>

static const bool registered = []() {
  // numValues 3: CO2, temperature, humidity; 5000 ppm is the sensor's range
  DmmDecoder::addConfig({"TFA Dostmann", "AIRCO2NTROL Mini *", "", 0, FrameFormat::TfaAirControl, 8, 1, 3, 0, 5000, 0, 0, 0});
  return true;
}();

std::optional<DmmDecoder::DmmResponse> DecoderTfaAirControl::decode(const QByteArray &data, int /*id*/)
{
  const QList<QByteArray> parts = data.trimmed().split(' ');
  if (parts.size() != 2)
    return std::nullopt;
  bool addrOk = false, rawOk = false;
  const uint address = parts[0].toUInt(&addrOk, 16);
  const uint raw = parts[1].toUInt(&rawOk);
  if (!addrOk || !rawOk || raw > 0xffff)
    return std::nullopt;

  m_result = {};
  m_result.hold = false;
  m_result.showBar = false;
  m_result.range = "";

  double value = 0;
  int decimals = 0;
  switch (address)
  {
    case 0x50:   // CO2
      m_result.id = 0;
      m_result.showBar = true;   // a bar for the main value only
      value = raw;
      m_result.unit = "ppm";
      m_result.special = "CO2";
      break;
    case 0x42:   // temperature, 1/16 K
      m_result.id = 1;
      value = std::round((raw / 16.0 - 273.15) * 100.0) / 100.0;
      decimals = 2;
      m_result.unit = "C";   // shown as degC (SiPrefix::displayText)
      m_result.special = "TE";
      break;
    case 0x41:   // relative humidity, 1/100 %
      m_result.id = 2;
      value = std::round(raw / 100.0 * 100.0) / 100.0;
      decimals = 2;
      m_result.unit = "%";
      m_result.special = "RH";
      break;
    default:
      return std::nullopt;
  }
  m_result.dval = value;   // already in base units: no SI prefix involved
  m_result.val = QString::number(value, 'f', decimals);
  return m_result;
}
