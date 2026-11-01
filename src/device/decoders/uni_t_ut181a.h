// Copyright (c) 2026 The QtDMM developers
// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "device/dmmdecoder.h"

#include <QElapsedTimer>
#include <QMutex>

/// UNI-T UT181A, over USB the UT-D09 HID UART cable (CP2110 or CH9329) at
/// 9600 8N1, over Bluetooth LE the UT-D07B adapter (ISSC "Transparent
/// UART", BleGattDevice).
///
/// Frames of any length: `AB CD`, a 16-bit LE length (payload + 2), the
/// payload, a 16-bit LE sum of the length bytes and the payload. The meter
/// streams a measurement (payload type 0x02) every 100 ms once asked with
/// SET_MONITOR (`05 01`); pollRequest() asks while none came for two
/// seconds, so a meter switched off and on again starts again. Every
/// command is answered with a type 0x01 frame ("OK"/"ER"), skipped here.
///
/// A measurement: misc (bits 1-2 aux values present, 3 bargraph, 4-6 format
/// normal/REL/MIN-MAX/PEAK, 7 HOLD), misc2 (bit 0 AUTO), the mode word
/// (family in the high byte), the range, then the values: each a float32
/// LE in the displayed unit, a precision byte (bit 0 OL, bit 1 -OL, both
/// over 0.0 no reading, bits 4-7 the decimals) and, in most formats, the
/// unit string (`VDC`, `mVac+dc`, `k~` for kΩ, `\xB0C`). Normal: main,
/// aux1, aux2 (the Hz and period beside V AC, the AC and DC parts of AC+DC,
/// the other thermocouple, dBm's voltage and 600 Ω). REL: relative,
/// reference, absolute. MIN/MAX: current, max, average, min sharing one
/// unit. PEAK: max and min. The bargraph (the fast sample) and the COMP
/// limits are not read.
///
/// Keys (keyRequest()): HOLD `12 5A`, MAX/MIN on and off (SET_MIN_MAX),
/// REL (SET_MODE with the mode word's last nibble 1 <-> 2), AUTO
/// (SET_RANGE 0); the meter answers ER where its own keys would not work.
///
/// Source: dmm-tools (GPL-3.0-or-later), docs/research/ut181/
/// reverse-engineered-protocol.md - from UNI-T's PC software and
/// community work, confirmed on two meters (issue #5, PR #8). Spec with the
/// meters' frames in docs/protocols/spec/uni_t_ut181a.yaml.
class DecoderUT181A : public DmmDecoder
{
  Q_OBJECT
public:
  DecoderUT181A(FrameFormat::DataFormat df) : DmmDecoder(df) { m_name = "UniTUT181A"; }

  std::optional<DmmDecoder::DmmResponse> decode(const QByteArray &data, int id) override;
  bool checkFormat(const char *data, size_t idx) override;
  size_t getPacketLength() override { return 0; }   // variable, framed by the length
  QByteArray pollRequest() const override;
  QByteArray keyRequest(const QString &key) const override;
  bool hasKey(const QString &key, const QString &model) const override;

  /// A command frame: @p opcode and its @p data, framed and summed.
  static QByteArray command(quint8 opcode, const QByteArray &data = QByteArray());

protected:
  /// The payload of the frame in @p data (bytes before it skipped); empty
  /// when there is none.
  static QByteArray payloadOf(const QByteArray &data);
  /// True while no measurement came for two seconds: the stream is to be
  /// asked for (again).
  bool silent() const;
  /// A measurement came: the stream runs.
  void heard();

private:
  /// What the keys need of the last measurement, written by decode() in
  /// the reader's thread, read by pollRequest()/keyRequest() in the GUI's.
  mutable QMutex m_lock;
  QElapsedTimer  m_lastMeasurement;   ///< invalid: none yet, ask for the stream
  quint16        m_mode = 0;          ///< the last mode word
  bool           m_minMax = false;    ///< in the MIN/MAX format
};
