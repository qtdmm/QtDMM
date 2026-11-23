// Copyright (c) 2026 The QtDMM developers
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "device/dmmdecoder.h"

/// Voltcraft GDM 703/704/705 bench meters (WENS98A chip): 26 bytes ASCII, continuous,
/// STX ... ETX, two readings per frame (main and secondary display).
/// Layout per Matthias Toussaint's CDMM (ablage/CDMM/src/gdm703.h):
/// `S mm vvvvvv uuuu _ B VVVVVV UUUU E` - S = 0x02, mm = mode, v = value
/// with sign, uuuu = prefix + unit, B = 'B', V/U = second value, E = 0x03.
/// No capture available; the models are marked unconfirmed.
class DecoderGDM703 : public DmmDecoder
{
  Q_OBJECT
public:
  DecoderGDM703(FrameFormat::DataFormat df) : DmmDecoder(df) { m_name = "GDM703"; }

  std::optional<DmmDecoder::DmmResponse> decode(const QByteArray &data, int id) override;
  bool checkFormat(const char* data, size_t idx) override;
  size_t getPacketLength() override;

private:
  /// Splits "1.2345" + " kV" style fields into val/dval/unit; returns false
  /// when the value field is not numeric (overload).
  bool parseValue(const QString &value, const QString &prefixUnit, QString &val, double &dval, QString &unit);
};
