// Copyright (c) 2026 The QtDMM developers
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <QString>

/// @file
/// SI prefix handling shared by decoders, recorder and display.

/// The one place that knows SI prefixes. Decoders, the recorder's CSV
/// import/export, the numeric display and the threshold editors all used to
/// carry their own copies, which disagreed on "µ" vs "u" and on which prefixes
/// exist at all.
namespace SiPrefix
{
  /// A unit taken apart into prefix and base unit.
  struct Split
  {
    QString prefix;     ///< "k", "m", "µ", ... or empty
    QString baseUnit;   ///< "V", "Ohm", "Hz", ...
  };

  /// "k" -> 1e3. Accepts both "µ" and "u" for micro; anything unknown -> 1.0.
  double factor(const QString &prefix);

  /// "kOhm" -> {"k", "Ohm"}, "Ohm" -> {"", "Ohm"}. A leading prefix letter only
  /// counts when something follows it, so a bare "m" stays a unit; units of
  /// their own such as "ppm", "min" and "mol" stay whole.
  Split split(const QString &unit);

  /// Scales value into [1, 1000) and reports the prefix that goes with it:
  /// 0.00123 -> 1.23 with prefix "m"; 0 stays 0 with no prefix. Micro is "µ".
  double scale(double value, QString *prefixOut);

  /// scale() rendered as text with up to 12 significant digits.
  QString format(double value, QString *prefixOut = nullptr);

  /// Whether a value in @p baseUnit gets an SI prefix: not without a unit,
  /// not °C/°F ("C", "dF" from the decoders) or %, nor the units of their
  /// own that split() keeps whole (ppm, %RH, dB, m/s, cosphi ...) - 1200 ppm,
  /// not 1.2 kppm.
  bool takesPrefix(const QString &baseUnit);
  /// scale() and format() for a value in @p baseUnit: one that takes no
  /// prefix stays as it is, with an empty prefix.
  double scale(double value, const QString &baseUnit, QString *prefixOut);
  QString format(double value, const QString &baseUnit, QString *prefixOut);

  /// A unit as the displays show it: "kOhm" -> "kΩ", "dF" -> "°F",
  /// "C" -> "°C", "cosphi" -> "cosφ"; others unchanged.
  QString displayText(const QString &unit);

  /// A meter's fixed-width reading without its leading zeros, as a meter
  /// shows it: "000.00" -> "0.00", "-029.30" -> "-29.30", "00012" -> "12".
  /// One zero before the decimal point stays; text like "OL" is unchanged.
  QString withoutLeadingZeros(const QString &text);
}
