// Copyright (c) 2026 The QtDMM developers
// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "device/dmmdecoder.h"

/// The OWON SPM series (SPM3051, SPM6053, SPM3103, SPM6103): a one-channel
/// power supply with a 4½-digit multimeter in one box, over its USB port (a
/// CDC serial port, SCPI). Read only: QtDMM asks, it never sets anything and
/// sends no SYST:REM, so the keys on the device stay usable.
///
/// Two questions in turn, one at a time (OWON's SPM programming manual):
///
/// - `MEAS:ALL:INFO?` → `2.000 5.000 10.000 NR1 NR2 NR3 NR4`: the output's
///   voltage, current and power, then over-voltage, over-current and
///   over-temperature (1|ON a fault, 0|OFF none) and the mode (0 standby,
///   1 CV, 2 CC, 3 failure). Values 1 to 3 of the device (ids 0, 1, 2:
///   U in V, I in A, P in W); a protection or a failure is the reading's
///   error ("OVP", "OCP", "OTP", "Failure"). CV/CC are not shown yet (the
///   power supply's front panel will).
/// - `CONF:ALL?` → `VOLT:DC,+0.0011V,AUTO,2V`: the multimeter's function
///   (VOLT:DC/AC, CURR:DC/AC, RES, CAP, DIOD, CONT), its value as shown with
///   the unit, AUTO or Manual and the range. Value 4 of the device (id 3).
///
/// Each answer is a frame of its own and carries its values only, the
/// other ones keep theirs (as AirCO2ntrol's frames do).
///
/// Not known before the device is tried (docs/protocols/spec/owon_spm.yaml):
/// whether answers end with LF or CR LF (both taken), what an overload
/// looks like in CONF:ALL? (a value with "OL" is taken as one), how fast it
/// answers.
///
/// Frames are variable-length lines (getPacketLength() 0).
class DecoderOwonSPM : public DmmDecoder
{
  Q_OBJECT
public:
  DecoderOwonSPM(FrameFormat::DataFormat df) : DmmDecoder(df) { m_name = "OwonSPM"; }

  std::optional<DmmDecoder::DmmResponse> decode(const QByteArray &data, int id) override;
  /// MEAS:ALL:INFO? and CONF:ALL? in turn, one per request sent.
  QByteArray pollRequest() const override;
  bool checkFormat(const char *data, size_t idx) override;
  size_t getPacketLength() override { return 0; }
  /// One answer of the two every 250 ms: the supply's values and the
  /// multimeter's each twice a second.
  int pollInterval() const override { return 250; }

  /// The ids of the device's values.
  enum Value { Voltage = 0, Current = 1, Power = 2, Multimeter = 3 };

private:
  std::optional<DmmResponse> decodeSupply(const QStringList &fields);
  std::optional<DmmResponse> decodeMultimeter(const QStringList &fields);

  mutable int m_requests = 0;   ///< sent so far: even ones ask for the supply
};
