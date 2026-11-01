// Copyright (c) 2026 The QtDMM developers
// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "device/dmmdecoder.h"

/// Brymen BM788BT and BM787BT over their built-in Bluetooth LE
/// (BleGattDevice). Brymen publishes the protocol.
///
/// After the login (login(), written by the transport once the
/// notifications are on: command 0x0151 with the default password 0000)
/// the meter notifies a 152-byte block: a 24-byte device information
/// packet, one 32-byte reading packet and three zero blocks. A frame here
/// is the reading packet, found wherever the notifications split the block:
/// `FF 02 20 05`, the data set and channel, the RTC (6), the status flags
/// (3: CREST, REL, HOLD, AUTO, AUTO-HOLD, ASCII text; sign, OL, MAX, MIN,
/// AVG), the device type, main and sub function (AC, DC, AC+DC are subs),
/// the count (24-bit LE signed), the digits before the point, the prefix
/// (a signed power of ten), the unit code and the digit count, a
/// CRC-16/MODBUS of bytes 2-27 (low byte first) and `FF 03`. Low battery
/// is only in the information packet: not read. No remote keys.
///
/// Source: dmm-tools (GPL-3.0-or-later), docs/research/bm78xbt/
/// reverse-engineered-protocol.md - from Brymen's protocol document r4,
/// its app and the manuals; the login as a community SDK uses it with real
/// meters. Spec with vectors in docs/protocols/spec/brymen_bm78xbt.yaml.
class DecoderBM78xBT : public DmmDecoder
{
  Q_OBJECT
public:
  DecoderBM78xBT(FrameFormat::DataFormat df) : DmmDecoder(df) { m_name = "BrymenBM78xBT"; }

  std::optional<DmmDecoder::DmmResponse> decode(const QByteArray &data, int id) override;
  bool checkFormat(const char *data, size_t idx) override;
  size_t getPacketLength() override { return 32; }

  /// The login for the meter at @p address ("AA:BB:..."): Verify Connection
  /// Password 0x0151 with 0000, the address least significant octet first
  /// (zeros where the platform gives none, macOS).
  static QByteArray login(const QString &address);
  /// CRC-16/MODBUS of @p n bytes (spec §4).
  static quint16 crc(const unsigned char *b, int n);
};
