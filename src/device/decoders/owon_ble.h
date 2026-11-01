// Copyright (c) 2026 The QtDMM developers
// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "device/dmmdecoder.h"

#include <atomic>

/// OWON's Bluetooth LE meters with the 6-byte frame: B33, B33T+, B35T+,
/// B41T+, OW16B, OW18B, OW18E and the CM2100B clamp. They advertise as "BDM" (or a
/// name the user gave them), have the 0xFFF0 service and notify one frame
/// per reading on 0xFFF4, about three a second, unasked.
///
/// A frame is three 16-bit little-endian words, no header, length or
/// checksum: function/range (bits 0-2 decimal point, 6 UL, 7 OL; 3-5 SI
/// prefix; 6-9 function), status (HOLD, REL, AUTO, low battery, MIN, MAX in
/// bits 0-5) and the count, sign and magnitude (bit 15 the sign). The value
/// is count / 10^dp in the function's unit with the prefix.
///
/// The frame does not tell the model; 0xFFF2, read once after connecting,
/// does in its byte 0 (modelsFromInfo(), for both frames: the meters of
/// DecoderOwon15 advertise alike). Keys: `<code> <01 short | 00 long>`
/// on 0xFFF3 (keyRequest()).
///
/// Source: dmm-tools (GPL-3.0-or-later),
/// docs/research/owon/reverse-engineered-protocol.md - from OWON's app, its
/// PC software and manuals, with community captures (§14.5); no meter there.
/// Spec with vectors in docs/protocols/spec/owon_ble.yaml.
class DecoderOwon : public DmmDecoder
{
  Q_OBJECT
public:
  DecoderOwon(FrameFormat::DataFormat df) : DmmDecoder(df) { m_name = "OwonBLE"; }

  std::optional<DmmDecoder::DmmResponse> decode(const QByteArray &data, int id) override;
  bool checkFormat(const char *data, size_t idx) override;
  size_t getPacketLength() override { return kFrameLength; }
  QByteArray keyRequest(const QString &key) const override;
  bool hasKey(const QString &key, const QString &model) const override;
  bool tellsModel() const override { return true; }
  QStringList modelsFromInfo(const QByteArray &info) const override;

private:
  static constexpr int kFrameLength = 6;
};

/// OWON's Bluetooth LE meters with the 15-byte frame: CMS101, CMS061 (UNI-T
/// UT217B, UT217A), OW65B, OW67B, OW69B and Voltcraft's VC871, VC891, VC915
/// and VC925 PV. Advertising, GATT, the model code and the keys as
/// DecoderOwon.
///
/// A frame is five 24-bit little-endian words, no header, length or
/// checksum: main function/range (bits 0-2 decimal point, 3-5 prefix, 6-10
/// function 0-29, 12 sub-display present; bits 16-23 0xF0 in every capture),
/// main reading (bits 0-18 magnitude, 20-22 status: 1 OL, 2 UL, 3 HI, 4 LO,
/// 6-7 none; 23 the sign), the sub-display's two words alike, and the
/// status (DecoderOwon's bits 0-5, 6 AVG, 10 PEAK, 13-15 AC/DC/USB power,
/// 18 scope). Functions 14-29 add power, energy, 4-20 mA, AC+DC, motor,
/// solar and high voltage. On the VC871 under REL, MAX or MIN the
/// sub-display shows the main function (setModel()).
///
/// Source: dmm-tools (GPL-3.0-or-later), docs/research/owon/
/// reverse-engineered-protocol.md §10 - from OWON's and Voltcraft's apps,
/// with VC871 captures (§14.5); no meter there. Spec with vectors in
/// docs/protocols/spec/owon_ble15.yaml.
class DecoderOwon15 : public DecoderOwon
{
  Q_OBJECT
public:
  DecoderOwon15(FrameFormat::DataFormat df) : DecoderOwon(df) { m_name = "OwonBLE15"; }

  std::optional<DmmDecoder::DmmResponse> decode(const QByteArray &data, int id) override;
  bool checkFormat(const char *data, size_t idx) override;
  size_t getPacketLength() override { return kFrameLength15; }
  QByteArray keyRequest(const QString &key) const override;
  bool hasKey(const QString &key, const QString &model) const override;
  void setModel(const QString &model) override;

private:
  static constexpr int kFrameLength15 = 15;
  /// One display from its function/range word @p gear and reading word @p v,
  /// read as @p function (the VC871's sub-display may take the main one's).
  static void fill(DmmChannel &c, quint32 gear, quint32 v, int function, quint32 status);
  std::atomic<bool> m_subFollowsMain{false};   ///< the VC871 (setModel())
};
