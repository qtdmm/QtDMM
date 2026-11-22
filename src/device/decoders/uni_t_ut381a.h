// Copyright (c) 2026 The QtDMM developers
// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "device/dmmdecoder.h"

/// UNI-T UT381A luxmeter over Bluetooth LE (UNI-T's "iENV" app), the vendor
/// service 0xFF12 of the UT383BT: QtDMM writes to 0xFF01, the meter answers
/// on 0xFF02. Its frames are of another family than the UT383BT's:
///
///     AA BB <length> <type> <payload> <16-bit BE sum>
///
/// with the length counting the type, the payload and the sum (a frame is
/// length + 3 bytes). Live frames flow only after "Start live" (type 0x39),
/// which BleGattDevice writes once on connecting; there is no poll.
///
/// Received types: 1 live (15 bytes: mode 0 live, 1 MAX, 2 MIN, 3 AVG; a
/// flag byte, bit 7 overload, bits 0-2 the battery level; the value as
/// float32 big-endian, always in lux), 2 the settings (bit 7 of byte 4 the
/// unit the meter shows, 0 lx, 1 fc), 3 HOLD (byte 4: 1 held). The decoder
/// keeps unit and HOLD from 2 and 3 for the live frames: fc is lux x 0.093,
/// as UNI-T's app converts it; the decimals are the app's too (two below
/// 100, else one). Other types are skipped.
///
/// The key HOLD sends type 0x31 with 1 or 0, the other state than the last
/// one the meter reported. MAX/MIN/AVG and lx/fc are one settings block
/// (type 0x30) the meter wants whole; colour correction, zero calibration
/// and the 99 stored readings are left out.
///
/// The frames are found by header and length alone: UNI-T's app checks no
/// sum of a received frame, and no capture shows whether the meter's are
/// right. Source: the notes decompiled from UNI-T's app,
/// ablage/uni-t/uni-t-protocols/ut381a.md. No meter has been tried.
class DecoderUT381A : public DmmDecoder
{
  Q_OBJECT
public:
  DecoderUT381A(FrameFormat::DataFormat df) : DmmDecoder(df) { m_name = "UniTUT381A"; }

  std::optional<DmmDecoder::DmmResponse> decode(const QByteArray &data, int id) override;
  QByteArray keyRequest(const QString &key) const override;
  bool checkFormat(const char *data, size_t idx) override;
  size_t getPacketLength() override { return 0; }   // variable, framed by the length

  /// The frame of command @p type with @p payload: header, length, type,
  /// payload and the 16-bit BE sum.
  static QByteArray commandFrame(quint8 type, const QByteArray &payload = QByteArray());
  /// "Start live" (type 0x39): the meter streams live frames after it.
  static QByteArray startLive() { return commandFrame(0x39); }

private:
  bool m_fc = false;     ///< the meter shows foot-candles (settings frame)
  bool m_hold = false;   ///< HOLD as the meter last reported it
};
