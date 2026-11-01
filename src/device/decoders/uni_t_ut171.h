// Copyright (c) 2026 The QtDMM developers
// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "device/decoders/uni_t_ut181a.h"

/// UNI-T UT171A, UT171B and UT171C, over USB the UT-D09 cable (CP2110 or
/// CH9329) at 9600 8N1, the UT171B and UT171C also over the UT-D07B adapter.
///
/// The UT181A's framing (DecoderUT181A): `AB CD`, a 16-bit LE length
/// (payload + 2), the payload, a 16-bit LE sum. The meter streams once
/// SETUP > Communication is ON and the connect command `AB CD 04 00 0A 01
/// 0F 00` came; pollRequest() sends it while no measurement came for two
/// seconds. On a UT181A the same bytes would start a recording.
///
/// A measurement (type 0x02, 15 payload bytes; 21 in the extended frame,
/// its third float not read): the flags (bit 7 HOLD, bit 6 manual range,
/// bit 2 low battery), the frame type, the mode (0x01-0x1D, 0x24 NCV), the
/// range (1-based), the main value as float32 LE, two status bytes, an aux
/// float32 LE (the frequency in kHz beside V AC and mV AC). Non-finite: OL.
/// The resistance comes in kOhm on ranges 2-4 and MOhm from 5; the other
/// modes in the base unit (unconfirmed for capacitance, conductance and
/// frequency). No remote keys are known.
///
/// Source: dmm-tools (GPL-3.0-or-later), docs/research/ut171/
/// reverse-engineered-protocol.md - from UNI-T's UT171C PC software, the
/// manual and gulux/Uni-T-CP2110's captures; no meter read there yet
/// (issue #4). Spec with vectors in docs/protocols/spec/uni_t_ut171.yaml.
class DecoderUT171 : public DecoderUT181A
{
  Q_OBJECT
public:
  DecoderUT171(FrameFormat::DataFormat df) : DecoderUT181A(df) { m_name = "UniTUT171"; }

  std::optional<DmmDecoder::DmmResponse> decode(const QByteArray &data, int id) override;
  QByteArray pollRequest() const override;
  QByteArray keyRequest(const QString &) const override { return QByteArray(); }
  bool hasKey(const QString &, const QString &) const override { return false; }
};
