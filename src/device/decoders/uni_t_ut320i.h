// Copyright (c) 2026 The QtDMM developers
// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "device/dmmdecoder.h"

/// UNI-T UT320i pipe clamp thermometer over Bluetooth LE (UNI-T's "iENV"
/// app, "Wireless Temperature Clamp"), the vendor service 0xFF12: QtDMM
/// writes to 0xFF01, the meter pushes on 0xFF02 of its own once
/// notifications are on. The frames are the UT381A's family:
///
///     AA BB <length> <command> <payload> <16-bit BE sum>
///
/// with the length counting command, payload and sum; unlike the other
/// iENV meters the app checks the sum, so the decoder does too.
///
/// Received commands: 0x01 live (23 bytes: two unused floats, T1 and T2 as
/// float32 little-endian in °C, a flag byte - bits 2-3 T1, bits 0-1 T2: 0
/// valid, 1 OL, 2 -OL, 3 no probe), 0x11 live while no probe is chosen on
/// the meter (the serial number, one temperature, its flag), 0x06 the
/// settings (byte 1 the unit the meter shows: 0 °C, 1 °F, 2 K). QtDMM asks
/// the settings once on connecting (settingsRequest()) and converts to that
/// unit, as UNI-T's app does [E 18.11.]. T1 and T2 are the values 1 and 2
/// [E 18.11.]; a missing probe shows "----" without a value.
///
/// No keys: changing unit or probe means writing the whole settings block,
/// a dialog of the app. Left out too: the 10000 stored readings, clock,
/// interval, auto power off, firmware update. Source: the notes decompiled
/// from UNI-T's app, ablage/uni-t/uni-t-protocols/ut320i.md. No meter has
/// been tried.
class DecoderUT320i : public DmmDecoder
{
  Q_OBJECT
public:
  DecoderUT320i(FrameFormat::DataFormat df) : DmmDecoder(df) { m_name = "UniTUT320i"; }

  std::optional<DmmDecoder::DmmResponse> decode(const QByteArray &data, int id) override;
  bool checkFormat(const char *data, size_t idx) override;
  size_t getPacketLength() override { return 0; }   // variable, framed by the length

  /// "Get setting" (0x31 0x06): the meter answers with the unit it shows.
  static QByteArray settingsRequest();

private:
  int m_unit = 0;   ///< the meter's unit: 0 °C, 1 °F, 2 K
};
