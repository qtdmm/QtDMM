// Copyright (c) 2026 The QtDMM developers
// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "device/dmmdecoder.h"

/// Voltcraft VC-880 (and VC650BT) and VC-890 over their USB cable, a CP2110
/// HID UART at 9600 8N1. Two protocol rows share this class: VoltcraftVC880
/// (39-byte frames, streamed once the meter's PC key is pressed) and
/// VoltcraftVC890 (66-byte frames, polled with `5E`; each request goes
/// after a result frame `FF 00`, as Voltcraft's software sends one).
///
/// Frames as the UT61E+'s: `AB CD`, a length (bytes after it), the type
/// (0x01 live data), the payload, a 16-bit BE sum of all bytes before it.
/// Live data: the function (0x00-0x12, numbered differently on the two
/// meters), the range ('0'-based ASCII: with the function the unit), the
/// main display as 7 ASCII characters with sign and point ("OL" or dashes
/// without a number), sub displays not read here, then the status bytes:
/// REL, AVG, MIN, MAX; HOLD, manual range, OL; low battery (VC-880).
///
/// Keys `AB CD 03 <cmd> <sum>`: 0x46 RANGE, 0x47 AUTO, 0x48 REL, 0x49
/// MAX/MIN/AVG, 0x43 leave it, 0x4A HOLD, 0x4B light, 0x4C SHIFT/SETUP.
///
/// Source: dmm-tools (GPL-3.0-or-later), docs/research/vc880/ and
/// docs/research/vc890/reverse-engineered-protocol.md - from Voltcraft's
/// PC software (DMSShare.dll), the manuals and Conrad's VC890 protocol
/// document; no meter there. Spec with vectors in
/// docs/protocols/spec/voltcraft_vc8x0.yaml.
class DecoderVC8x0 : public DmmDecoder
{
  Q_OBJECT
public:
  DecoderVC8x0(FrameFormat::DataFormat df);

  std::optional<DmmDecoder::DmmResponse> decode(const QByteArray &data, int id) override;
  bool checkFormat(const char *data, size_t idx) override;
  size_t getPacketLength() override { return size_t(m_length); }
  QByteArray pollRequest() const override;
  QByteArray keyRequest(const QString &key) const override;

  /// A command frame `AB CD 03 <cmd>` with its sum, or with @p data.
  static QByteArray command(quint8 cmd, const QByteArray &data = QByteArray());

private:
  bool m_vc890;
  int  m_length;     ///< the whole live frame
  int  m_statusAt;   ///< the first status byte in the frame
};
