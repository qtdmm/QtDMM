// Copyright (c) 2026 The QtDMM developers
// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "device/scpi/decoderscpi.h"

/// Agilent/Keysight U1230/U1240/U1250/U1270/U1280 handheld meters over their
/// IR-USB cable (U1173A/U1173B, a serial port at 9600 8N1). Polled with SCPI
/// short forms: `CONF?` says what the meter measures, `FETC?` the value.
///
/// `CONF?` comes in two shapes:
/// - U123x/U127x: `"V,0,DC"`, `"MV,1,AC"`, `"MA,0,ACDC"`, `"RES,2"`,
///   `"DIOD"`, `"TEMP"` - function, resolution, coupling; `FETC?` then
///   counts in the unit the function names (mV, mA, µA).
/// - U124x/U125x/U128x: `"VOLT:AC +6.000000E+00,+1.000000E-03"` (range,
///   resolution), `"RES +6.000000E+02,..."`, `"TEMP:K CEL"`, `"DIOD"` -
///   `FETC?` in base units.
///
/// `FETC?` answers an NR3 number, `±9.90000000E+37` for overload. A turn of
/// the rotary switch arrives unasked as `*<n>`; the function is unknown
/// until the next `CONF?`.
///
/// A frame is the line with the value, not every line (isFrame()): the
/// `CONF?` answer before it stays in the reader's buffer, so decode() gets
/// both and reads them in order; the answers tell what they are (a quoted
/// `CONF?`, an NR3 `FETC?`). The plan (DecoderScpi) asks `CONF?` first,
/// then every 50th round and once more after a switch (askSoon()),
/// otherwise `FETC?`. Polled every 200 ms (pollInterval()): the meters
/// measure 5 times a second (the U128x up to 40 in its high-speed mode,
/// not used); after `CONF?`, which has no frame of its own, FrameReader
/// asks `FETC?` two ticks later.
///
/// Not read: STAT? (hold, min/max, the U123x temperature and continuity
/// modes, which `CONF?` reports as MV and RES), the second display and
/// logged data. Source: libsigrok `agilent-dmm` (GPL-3.0-or-later).
class DecoderKeysightU12xx : public DecoderScpi
{
  Q_OBJECT
public:
  DecoderKeysightU12xx(FrameFormat::DataFormat df);

  int pollInterval() const override { return 200; }

protected:
  bool isFrame(QByteArrayView line) const override;
  std::optional<DmmResponse> parseAnswer(const Asked &asked, const QString &line, int id) override;
  /// "*3", the rotary switch turned; "*E", an error.
  bool isUnasked(const QString &line) const override { return line.startsWith('*'); }

private:
  /// What `CONF?` said; empty unit: not known (yet), no value is shown.
  struct Function
  {
    QString unit;      ///< base unit ("V", "A", "Ohm", "C", ...)
    QString special;   ///< "DC", "AC", "ACDC", "OH", "BUZ", "DI", "TE", ...
    double  scale = 1; ///< FETC? counts in mV, mA, µA on the U123x/U127x
  };
  /// Reads one `CONF?` answer (without the quotes); false when unknown.
  static bool parseConf(const QString &conf, Function *f);

  Function m_function;
};
