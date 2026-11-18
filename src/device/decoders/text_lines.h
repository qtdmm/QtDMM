// Copyright (c) 2026 The QtDMM developers
// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <QStringList>

#include "device/dmmdecoder.h"

/// A microcontroller (Arduino, ESP32, Pico ...) printing its values as text
/// lines over USB serial, in either of two formats, told apart per line:
///
///  - Arduino Serial Plotter: `512`, `512,300,128`, `voltage:3.31 current:0.12`
///    (numbers or label:number, separated by commas, spaces or tabs); all
///    values of a line are one reading.
///  - Teleplot (github.com/nesnes/teleplot): `name:value`, with a time stamp
///    `name:ms:value`, a unit `name:value§V`, flags `|xy`, `|t`, `|clr`, several
///    points `name:t1:v1;t2:v2`. One variable per line.
///
/// A label that is a unit QtDMM knows (`V`, `mA`, `kOhm`, `°C`, `%RH`; not
/// a letter like `C` alone) is the value's unit too: `V:3.31,A:0.12` is a
/// voltage and a current [E 18.11.]; Teleplot's `§` comes first.
///
/// A line with `§`, `|`, `;` or two colons after the name is Teleplot, any
/// other the Serial Plotter's ("name:value" means the same in both). Each
/// name takes a value of the meter when it first comes, up to four [E 18.11.];
/// unnamed numbers are named by their place ("1", "2"); later names are
/// left out. A Teleplot line gives the one value it has. The time is when the
/// line came: Teleplot's time stamps are not used, of several points the
/// last counts [E 18.11.]. `name:x:y|xy` gives the two values "name.x" and
/// "name.y" [E 18.11.], for the XY view. Text (`|t`), log (`>...`) and 3D
/// lines, `nan`/`inf`, words and broken lines are skipped; `clr` and the
/// other flags are ignored.
///
/// The frame reader holds 100 bytes: a longer line is dropped, and so is
/// the rest of it that follows.
class DecoderTextLines : public DmmDecoder
{
  Q_OBJECT
public:
  DecoderTextLines(FrameFormat::DataFormat df) : DmmDecoder(df) { m_name = "TextLines"; }

  std::optional<DmmDecoder::DmmResponse> decode(const QByteArray &data, int id) override;
  bool checkFormat(const char *data, size_t idx) override;
  size_t getPacketLength() override { return 0; }   // variable, ends with LF

  /// One value of a line: its name, the number as text and as a number, the unit.
  struct Item
  {
    QString key;
    QString text;
    double value = 0;
    QString unit;
  };
  /// The values of @p line (without its line end); empty when it has none
  /// QtDMM takes (a word, a text or log line, a broken one).
  static QList<Item> parse(const QString &line);

private:
  QStringList m_keys;          ///< the names, in the order of their values
  bool m_dropNext = false;     ///< the rest of a line too long for the reader
};
