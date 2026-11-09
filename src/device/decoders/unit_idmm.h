// Copyright (c) 2026 The QtDMM developers
// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "device/dmmdecoder.h"

/// UNI-T "iDMM" protocol of the Bluetooth meters (UT60BT, UT161A-E, UT202BT) and of the
/// UT61B+/D+/E+: `AB CD`-framed packets, over Bluetooth LE a Microchip/ISSC
/// "Transparent UART" GATT service (BleGattDevice; UT60BT built in, UT-D07B
/// adapter for the UT61x+), over USB the UT-D09 HID UART cable at 9600 8N1.
///
/// Three protocol rows share this class: UniTiDMM (UT60BT), UniTUT61Plus
/// (UT61B+/D+/E+, UT161) and UniTUT202BT (clamp), which differ in the range
/// tables and keys - see the tables in the .cpp.
///
/// QtDMM polls: every request `AB CD 03 5E 01 D9` ('^') is answered by one
/// 19-byte measurement frame
///
///     AB CD 10 | fn | range | 7 x ASCII display | bar hi | bar lo |
///     flags A | flags B | flags C | checksum (16 bit BE sum of bytes 0..16)
///
/// fn selects the function (DCV, OHM, ...), range an ASCII digit that, with
/// fn, gives unit and prefix; the display string carries sign and decimal
/// point exactly as on the LCD. Bit 7 of fn marks a frame of the second
/// display (DmmChannel::id + 1, no flags). Other frames (the 11-byte name
/// answer) fail the length/checksum test and are skipped.
///
/// The meter's keys are frames of the same kind, `AB CD 03 <cmd> <16 bit BE
/// sum>` (keyRequest()): 0x46 RANGE, 0x47 AUTO, 0x48 REL, 0x4A HOLD, 0x4B
/// LIGHT, 0x4C SELECT; the UT61x+ also 0x41 MAX/MIN, 0x42 leave MAX/MIN,
/// 0x49 Hz/%, 0x4D PEAK and 0x4E leave PEAK. HOLD, REL, MAX/MIN and PEAK
/// come back in flags A and C (DmmChannel::states).
///
/// Spec with vectors in docs/protocols/spec/unit_idmm.yaml. Sources:
/// ble-multimeter (docs/protocols/uni-t.md, hardware-verified on a UT60BTk)
/// and ut61xpy (adapters/ut61xp.py), both in ablage/.
class DecoderUniTiDMM : public DmmDecoder
{
  Q_OBJECT
public:
  DecoderUniTiDMM(FrameFormat::DataFormat df) : DmmDecoder(df) { m_name = "UniTiDMM"; }

  std::optional<DmmDecoder::DmmResponse> decode(const QByteArray &data, int id) override;
  QByteArray pollRequest() const override;
  QByteArray keyRequest(const QString &key) const override;
  /// The keys of keyRequest(), but no PEAK on the UT61B+ and UT161B: their
  /// manual gives them a plain MAX/MIN key (dmm-tools f2dea93).
  bool hasKey(const QString &key, const QString &model) const override;
  /// The frame for command byte @p cmd: AB CD 03 cmd and the 16-bit BE sum.
  static QByteArray commandFrame(quint8 cmd);
  bool checkFormat(const char *data, size_t idx) override;
  size_t getPacketLength() override { return kFrameLength; }

  static constexpr int kFrameLength = 19;
  /// True when @p frame (kFrameLength bytes) has the header, the length byte
  /// and a matching checksum.
  static bool frameValid(const unsigned char *frame);

private:
  /// The display text of @p frame into m_result: val, and dval with @p prefix.
  DmmResponse displayed(const unsigned char *frame, const char *prefix);
  bool m_hold = false;   ///< HOLD of the last main frame, for the second display's frames
};
