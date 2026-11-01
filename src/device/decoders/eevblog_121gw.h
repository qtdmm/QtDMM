// Copyright (c) 2026 The QtDMM developers
// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "device/dmmdecoder.h"

/// EEVblog 121GW over its built-in Bluetooth LE (BleGattDevice; the meter
/// indicates on its one characteristic once BT is on: hold 1ms PEAK).
///
/// A packet is 19 bytes: `F2`, the identity (4), the main mode (bits 7-6
/// the value's bits 17-16), range with OFL, sign, °C and °F, the main
/// value (16 bits BE), the sub mode (0-24 as the main, 100+ settings and
/// figures such as the internal temperature), sub range (OFL, sign, k, Hz,
/// point), sub value, the bar (2, not read), three annunciator bytes and
/// an XOR of bytes 0-17. The meter is seen sending bytes 1-18 alone, so a
/// frame here is those 18 bytes, whose XOR is `F2`; a leading `F2` is
/// dropped. Main value: the count with the range's decimals, the prefix by
/// the range. Secondary: the frequency, internal temperature, battery,
/// burden voltage, dBm; in the VA modes the voltage and the current, each
/// its own value. Annunciators: AUTO, low battery, REL, MIN/MAX/AVG,
/// 1 ms peak, HOLD, the DC/AC/DC+AC coupling, the 1 kHz filter.
///
/// Keys `F4` and the code twice as ASCII hex: RANGE 01, HOLD 02, REL 03,
/// 1ms PEAK 04, MODE 05, MIN/MAX 06; held (bit 7): MIN/MAX leaves 86,
/// light 85. Not 84: held 1ms PEAK switches Bluetooth off.
///
/// Source: dmm-tools (GPL-3.0-or-later), docs/research/121gw/
/// reverse-engineered-protocol.md - from EEVblog's packet documents, its and
/// UEi's apps and the manual; two community packets from real meters. Spec
/// with vectors in docs/protocols/spec/eevblog_121gw.yaml.
class Decoder121GW : public DmmDecoder
{
  Q_OBJECT
public:
  Decoder121GW(FrameFormat::DataFormat df) : DmmDecoder(df) { m_name = "Eevblog121GW"; }

  std::optional<DmmDecoder::DmmResponse> decode(const QByteArray &data, int id) override;
  bool checkFormat(const char *data, size_t idx) override;
  size_t getPacketLength() override { return 18; }
  QByteArray keyRequest(const QString &key) const override;
};
