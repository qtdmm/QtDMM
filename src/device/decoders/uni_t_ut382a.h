// Copyright (c) 2026 The QtDMM developers
// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "device/dmmdecoder.h"

/// UNI-T UT382A luxmeter over Bluetooth LE (UNI-T's "iENV" app), the vendor
/// service 0xFF12 of the UT383BT: QtDMM writes to 0xFF01, the meter answers
/// on 0xFF02. Not the UT382 with its cable (DecoderUniTUT382). The frames
/// are the UT381A's family:
///
///     AA BB <length> <command> <payload> <check>
///
/// QtDMM asks the status (0x40) once on connecting and polls a live frame
/// with 0x39; UNI-T's app does both, the meter is said to stream on its own
/// too. The commands go out as the app's constants: their check byte follows
/// no rule all of them keep.
///
/// Received commands: 1 live (19 bytes: the mode, a flag byte - bit 7
/// overload, bit 6 error, bit 5 the sign in mode 6, bits 0-2 the battery -
/// value 1 and value 2 as float32 big-endian in lux, the elapsed h:m:s),
/// 2 the settings and 0x0C the status (bit 7 of the flags: the meter shows
/// fc; the status has HOLD too), 3 HOLD, 4 the start or stop of a timed
/// mode (its int32 BE is the distance in cm for the luminous intensity).
/// fc is lux x 0.093, as UNI-T's app converts it.
///
/// The modes: 0 the reading, 1/2/3 MAX/MIN/AVG (inferred, the app has no
/// words for them), 4 zero calibration (skipped), 5 countdown, 6 the
/// difference to a start value (signed), 7 the integral: value 2 is the
/// light exposure in lx·h [E 18.11.], 8 the luminous intensity: cd = lux x
/// the distance squared in m once a start frame told the distance, the lux
/// as value 2 [E 18.11.]; without it the lux alone.
///
/// The key HOLD sends 0x31 with 1 or 0, the other state than the last one
/// the meter reported. Modes, zero calibration, timers, the stored readings
/// and the settings are dialogs of the app, left out. Frames are found by
/// header and length alone (the app checks nothing it receives). Source:
/// the notes decompiled from UNI-T's app, ablage/uni-t/uni-t-protocols/
/// ut382a.md. No meter has been tried.
class DecoderUT382A : public DmmDecoder
{
  Q_OBJECT
public:
  DecoderUT382A(FrameFormat::DataFormat df) : DmmDecoder(df) { m_name = "UniTUT382A"; }

  std::optional<DmmDecoder::DmmResponse> decode(const QByteArray &data, int id) override;
  QByteArray pollRequest() const override;
  QByteArray keyRequest(const QString &key) const override;
  bool checkFormat(const char *data, size_t idx) override;
  size_t getPacketLength() override { return 0; }   // variable, framed by the length

  /// "Check status" (0x40): the meter answers with its unit and HOLD.
  static QByteArray checkStatus();

private:
  bool m_fc = false;       ///< the meter shows foot-candles
  bool m_hold = false;     ///< HOLD as the meter last reported it
  int  m_distanceCm = 0;   ///< of the last start frame; 0: none seen
};
