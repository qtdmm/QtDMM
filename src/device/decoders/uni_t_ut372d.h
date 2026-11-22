// Copyright (c) 2026 The QtDMM developers
// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "device/dmmdecoder.h"

/// UNI-T UT372D tachometer over Bluetooth LE (UNI-T's "iENV" app), the
/// vendor service 0xFF12 of the UT383BT: QtDMM writes to 0xFF01, the meter
/// streams on 0xFF02 of its own once notifications are on. The frames are
/// the UT381A's family:
///
///     AA BB <length> <type> <payload> <16-bit BE sum>
///
/// The live frame (type 1, or 5 "broadcast", 35 bytes): the probe (mode 0
/// non-contact, 1 contact, 2 0.1 m wheel, 3 6 inch wheel), the unit (modes
/// 0/1: 0 RPM, 1 Hz, 2 REV; modes 2/3: 0 m/min, 1 in/min, 2 ft/min, 3 m,
/// 4 in, 5 ft), a function byte nothing reads, then four displays of six
/// bytes - NOW, MAX, MIN, AVG, each an overflow mark (presumably OL), the
/// decimals and a signed int32 BE that is the value x 100 - the measuring
/// state (0 held, 1 measuring) and the battery (1 low). The four displays
/// are the values 1 to 4: the reading and its MAX, MIN and AVG.
///
/// Keys: HOLD sends "set status" (type 0x32: probe, unit, 1 measure or 0
/// hold, the other state than the meter's last); "unit" sends "set unit"
/// (type 0x31: probe, the next unit of the probe). Frames are found by
/// header and length (the app checks no sum it receives); a frame split
/// over two notifications is put together again.
///
/// Source: the notes decompiled from UNI-T's app,
/// ablage/uni-t/uni-t-protocols/ut372d.md. No meter has been tried.
class DecoderUT372D : public DmmDecoder
{
  Q_OBJECT
public:
  DecoderUT372D(FrameFormat::DataFormat df) : DmmDecoder(df) { m_name = "UniTUT372D"; }

  std::optional<DmmDecoder::DmmResponse> decode(const QByteArray &data, int id) override;
  QByteArray keyRequest(const QString &key) const override;
  bool checkFormat(const char *data, size_t idx) override;
  size_t getPacketLength() override { return 0; }   // variable, framed by the length

  /// A set frame: AA BB 06 @p type @p mode @p unit @p measure and the sum.
  static QByteArray setFrame(quint8 type, quint8 mode, quint8 unit, quint8 measure);
  /// The unit of @p unit under the probe @p mode ("RPM", "m/min"), empty
  /// for codes the notes do not have.
  static QString unitName(int mode, int unit);

private:
  int  m_mode = 0;          ///< the probe the meter last reported
  int  m_unit = 0;          ///< its unit
  bool m_measuring = true;  ///< not held
};
