// Copyright (c) 2026 The QtDMM developers
// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <QByteArray>
#include <QString>
#include <QStringList>

/// @file
/// What every SCPI instrument's text means, without state: numbers with
/// their special values, lists, *IDN? and SYST:ERR? answers, whether a
/// line asks something. Shared by the decoders that poll instruments
/// (OWON SPM, Keysight U12xx, OpenOSC) and later by the SCPI client.

/// The SCPI text rules (IEEE 488.2 and SCPI-99), parsed and formatted.
namespace Scpi
{
  /// A number from an instrument's answer.
  struct Number
  {
    enum class Kind
    {
      Invalid,     ///< no number at the start of the text
      Value,       ///< a finite value
      Overload,    ///< over the range: 9.9E37, INF, "OL" (negative: -9.9E37, NINF, "-OL")
      NotANumber   ///< 9.91E37 or NAN: no value (an open input, nothing measured)
    };
    Kind    kind = Kind::Invalid;
    double  value = 0;     ///< the value; ±infinity on Overload, NaN on NotANumber
    QString suffix;        ///< what follows the number, trimmed ("V" of "1.5V", "MV" of "OL MV")
    bool    isValue() const { return kind == Kind::Value; }
    bool    negative() const { return value < 0; }
  };

  /// The number at the start of @p text: NR1 ("12"), NR2 ("-1.5"),
  /// NR3 ("+1.234E-03"), the SCPI special values and the overload words
  /// of handheld meters ("OL", "+OL.000V", "-OL"). Spaces around are
  /// skipped, what follows is the suffix.
  Number number(QStringView text);

  /// A response's elements, split at commas outside quoted strings and
  /// trimmed. Quoted strings stay quoted (unquote() them).
  QStringList list(QStringView response);

  /// A quoted string's text ("\"a\"\"b\"" -> a"b, also with '); text not
  /// quoted comes back trimmed.
  QString unquote(QStringView text);

  /// The answer to *IDN?.
  struct Identity
  {
    QString vendor, model, serial, firmware;
    bool    isValid() const { return !vendor.isEmpty() && !model.isEmpty(); }
  };
  /// "OWON,SPM6103,2304001,FV:V1.2.0" -> its four fields; fewer than two:
  /// not valid.
  Identity identity(QStringView response);

  /// An entry of the error queue (SYST:ERR?): 0 is "No error".
  struct Error
  {
    int     code = 0;
    QString message;
    bool    isError() const { return code != 0; }
  };
  /// "-113,\"Undefined header\"" -> -113 and its text; nothing readable:
  /// code 0 with the text as it came.
  Error error(QStringView response);

  /// Whether the instrument answers @p line: a header ending in "?",
  /// also after a ";" ("VOLT 1;:MEAS?"), not inside a quoted string.
  bool isQuery(QByteArrayView line);

  /// @p value for a command: the shortest form with up to 12 significant
  /// digits, C locale ("0.001", "12.5", "1e-06"); infinity as ±9.9E37,
  /// NaN as 9.91E37.
  QByteArray format(double value);
}
