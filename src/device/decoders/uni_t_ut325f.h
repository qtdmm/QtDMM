// Copyright (c) 2026 The QtDMM developers
// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "device/dmmdecoder.h"

/// UNI-T UT325F four-channel thermocouple thermometer over Bluetooth LE
/// (UNI-T's "iENV" app), the vendor service 0xFF12: QtDMM writes to 0xFF01,
/// the meter answers on 0xFF02. Its frames have a two-byte length:
///
///     AA 55 <length BE16> <type> <payload> <16-bit BE sum>
///
/// with the length counting type, payload and sum (a frame is length + 4
/// bytes); the app checks no sum it receives, the decoder neither.
///
/// The live stream is a switch in the panel settings, a block written whole:
/// QtDMM asks the panel (type 0x35, getPanel()) on connecting; when the
/// answer (type 6) has the live output or BLE off, takeCommand() gives the
/// block back once with both on and everything else as the meter had it.
/// The meter sends type 6 of its own when a key is pressed on it, so HOLD,
/// mode and unit stay current.
///
/// Live frame (type 1, 36 bytes): T1-T4 as float32 little-endian in °C, a
/// status byte each (high nibble 0 valid, 1 -OL, 2 OL, 3 no probe; low
/// nibble the thermocouple type K, J, T, E, R, S, N, B), the ambient
/// temperature (left out: four values are QtDMM's limit), a counter and
/// the MAX/MIN/AVG mode. T1-T4 are the values 1-4, named with their type
/// ("T1 (K)") [E 18.11.], in the unit the meter shows (°C, °F, K) [E 18.11.].
///
/// Keys [E 18.11.]: HOLD, MIN/MAX (normal, MAX, MIN, AVG) and UNIT (°C,
/// °F, K), each the panel block with that field changed. Left out: the
/// 80000 stored readings, thermocouple type and offset, clock, auto power
/// off, mains filter. Source: the notes decompiled from UNI-T's app,
/// ablage/uni-t/uni-t-protocols/ut325f.md. No meter has been tried.
class DecoderUT325F : public DmmDecoder
{
  Q_OBJECT
public:
  DecoderUT325F(FrameFormat::DataFormat df) : DmmDecoder(df) { m_name = "UniTUT325F"; }

  std::optional<DmmDecoder::DmmResponse> decode(const QByteArray &data, int id) override;
  QByteArray keyRequest(const QString &key) const override;
  bool hasKey(const QString &key, const QString &model) const override;
  QByteArray takeCommand() override;
  bool checkFormat(const char *data, size_t idx) override;
  size_t getPacketLength() override { return 0; }   // variable, framed by the length

  /// "Get panel setting" (type 0x35): the meter answers with its panel.
  static QByteArray getPanel();
  /// A frame of @p type with @p payload: header, length, type, payload, sum.
  static QByteArray frame(quint8 type, const QByteArray &payload = QByteArray());

private:
  /// "Set panel" (type 0x36) from the meter's last panel, live output and
  /// BLE on; empty while no panel has come.
  QByteArray setPanel(int field = -1, quint8 value = 0) const;

  QByteArray m_panel;     ///< the 13 bytes of the last panel, as "set panel" wants them
  Battery m_battery = Battery::Unknown;   ///< from the last panel (its BATT_Status)
  QByteArray m_pending;   ///< for takeCommand()
};
