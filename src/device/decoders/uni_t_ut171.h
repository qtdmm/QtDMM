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
/// A measurement (payload type 0x02, as UNI-T's UT171C software's frame
/// handler reads it, spec §5): status A (bit 0 aux block, 1 auto-save with
/// its minutes, 2 low battery (gulux only), 3 bar float, 4 REL, 5 MAX/MIN
/// running, 6 Peak, 7 HOLD),
/// status B (bit 0 AUTO, 1 high-voltage alarm, bits 6/5 MAX, AVG, both MIN),
/// the mode (§6: 01 LoZ, 02-04 V DC/AC/AC+DC, 05-07 mV, 08/09 °C/°F,
/// 0A Ω, 0B capacitance, 0C continuity, 0D diode, 0E nS, 0F Hz, 11-19 µA,
/// mA, A, 1A NCV, 1B/1C 600 A clamp, 1D square wave out, 1E VFC, 1F % of
/// 4-20 mA), the range row (1-based), the value as float32 LE already in
/// its unit, a byte with the decimals (high nibble) and the display state
/// (low: 0 the value, 1 OL, 2 -OL, 3 dashes, 4 LEAD, 5 DISC, 6 Lo, 7 Hi),
/// the unit code (§5.6: VDC ... kΩ, MΩ, kHz, nF, µF, mF, °C ... ms). Then,
/// by the bits of status A, the aux block (float, decimals/state, unit:
/// the second value), the bar float and the auto-save minutes (not read).
/// The square wave (1D) has its own layout: frequency, duty, width.
///
/// Keys (keyRequest(), the commands the software sends, §4.7): HOLD `07
/// 5A`; RANGE `02 n` - from auto the row shown, in manual the next one;
/// AUTO `02 00`; REL `04 01` with the float shown (the meter's own key
/// takes the display) or `04 00` to leave; MAX/MIN `05 01`/`05 00`; Peak
/// `06 01`/`06 00`; Hz% `03 5A`. The meter answers OK, ER or NO.
///
/// Source: dmm-tools (GPL-3.0-or-later), docs/research/ut171/
/// reverse-engineered-protocol.md, re-read 07.11. from the software's frame
/// handler (the earlier reading came from its database engine); no meter
/// has been read or sent a command there or here. Spec with vectors in
/// docs/protocols/spec/uni_t_ut171.yaml.
class DecoderUT171 : public DecoderUT181A
{
  Q_OBJECT
public:
  DecoderUT171(FrameFormat::DataFormat df) : DecoderUT181A(df) { m_name = "UniTUT171"; }

  std::optional<DmmDecoder::DmmResponse> decode(const QByteArray &data, int id) override;
  QByteArray pollRequest() const override;
  QByteArray keyRequest(const QString &key) const override;
  bool hasKey(const QString &key, const QString &model) const override;

private:
  /// What the keys need of the last measurement, written by decode() in the
  /// reader's thread, read by keyRequest() in the GUI's.
  mutable QMutex m_keyLock;
  bool       m_seen = false;   ///< a measurement came
  quint8     m_mode = 0;
  quint8     m_range = 0;
  bool       m_auto = true;
  bool       m_rel = false;
  bool       m_peak = false;
  QByteArray m_shown;          ///< the main float's 4 bytes while it is a number
};
