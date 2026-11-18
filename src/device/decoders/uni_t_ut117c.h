// Copyright (c) 2026 The QtDMM developers
// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "device/dmmdecoder.h"

/// UNI-T UT117C multimeter over Bluetooth LE (UNI-T's "smartmeasure"
/// multimeter app), the ISSC service of the UT60BT: QtDMM writes to
/// ...8841..., the meter answers on ...9616.... Frames of UniTAbcd16:
/// `AB CD <length BE16> <command> <data> <16-bit BE sum>`, the sum checked;
/// a reading's length as the app reads it (0x14) or as libreble (0x12).
///
/// The meter streams nothing: pollRequest() asks a reading (command 0x05),
/// the answer is command 0x02, 24 bytes: the function (1-19, the app's
/// table: AC V, AC V low-pass, AC mV, DC V, DC mV, Ω, continuity, diode,
/// capacitance, LoZ V, Hz on V, Hz low-pass, NCV, DC A, AC A, Hz on A, the
/// clamp's AC A, Hz and DC A), the range, the value and the unit as ASCII
/// (7 and 3 bytes, "o" for Ω), an OL flag ('1' OL, '2' -OL), a MAX/MIN byte
/// whose values are unknown (not shown [E 18.11.]) and two flag bytes: A
/// bit 3 low battery, bit 1 HOLD, bit 0 REL; B bit 1 AC (LoZ). NCV shows
/// "HI" or "LO" (the range byte) without a number. Command 0x01 is the
/// answer to a key.
///
/// Keys: SELECT, RANGE, AUTO, REL, MIN/MAX on and off, HOLD, LPF (the
/// low-pass filter, a key of its own [E 18.11.]) and LIGHT, as the app
/// sends them; keyWorks() greys them out in the functions the app does.
/// Source: the notes decompiled from UNI-T's app,
/// ablage/uni-t/uni-t-protocols/ut117c.md, and libreble's ut117c.md. No
/// meter has been tried.
class DecoderUT117C : public DmmDecoder
{
  Q_OBJECT
public:
  DecoderUT117C(FrameFormat::DataFormat df) : DmmDecoder(df) { m_name = "UniTUT117C"; }

  std::optional<DmmDecoder::DmmResponse> decode(const QByteArray &data, int id) override;
  QByteArray pollRequest() const override;
  QByteArray keyRequest(const QString &key) const override;
  bool keyWorks(const QString &key, const QString &model, const Reading &reading) const override;
  bool checkFormat(const char *data, size_t idx) override;
  size_t getPacketLength() override { return 0; }   // variable, framed by the length

private:
  int m_function = 0;   ///< of the last reading; 0: none yet
};
