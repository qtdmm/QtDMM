// Copyright (c) 2026 The QtDMM developers
// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "device/dmmdecoder.h"

/// UNI-T UT219PV clamp meter for photovoltaics over Bluetooth LE (UNI-T's
/// "smartmeasure" multimeter app), the ISSC service of the UT60BT: QtDMM
/// writes to ...8841..., the meter answers on ...9616.... Frames of
/// UniTAbcd16 (`AB CD <length BE16> <command> <data> <16-bit BE sum>`), a
/// frame length + 4 bytes; the app checks no sum it receives, the decoder
/// neither.
///
/// Polled (pollRequest(), command 0x05); the answer is command 0x02, 38
/// bytes: the function (1-19, the app's table), the main display (range,
/// six ASCII characters, a three-character unit), the second display
/// alike, the stored count, an OL flag for each ('1' OL, '2' -OL), a
/// MAX/MIN byte whose values are unknown (not shown), the battery level
/// (scale unknown, left out) and flags: bit 2 HOLD, bit 1 ZERO. The main
/// display is value 1, the second value 2; functions without a second
/// display leave it empty. Units as ASCII: "oC" °C, "oF" °F, "o" Ω, "u" µ.
///
/// Keys as the app sends them: SET (select1), HOLD, AUTO-HOLD, MIN/MAX on
/// and off, INRUSH, LPF, ZERO [E 18.11.]; LPF only where a function has a
/// low-pass twin. A save key pressed on the meter arrives as command 0x0A
/// or 0x0C; takeCommand() answers it once, as the app does [E 18.11.]. The
/// stored readings are left out. Source: the notes decompiled from UNI-T's
/// app, ablage/uni-t/uni-t-protocols/ut219pv.md. No meter has been tried.
class DecoderUT219PV : public DmmDecoder
{
  Q_OBJECT
public:
  DecoderUT219PV(FrameFormat::DataFormat df) : DmmDecoder(df) { m_name = "UniTUT219PV"; }

  std::optional<DmmDecoder::DmmResponse> decode(const QByteArray &data, int id) override;
  QByteArray pollRequest() const override;
  QByteArray keyRequest(const QString &key) const override;
  bool keyWorks(const QString &key, const QString &model, const Reading &reading) const override;
  QByteArray takeCommand() override;
  bool checkFormat(const char *data, size_t idx) override;
  size_t getPacketLength() override { return 0; }   // variable, framed by the length

private:
  int m_function = 0;     ///< of the last reading; 0: none yet
  QByteArray m_pending;   ///< for takeCommand()
};
