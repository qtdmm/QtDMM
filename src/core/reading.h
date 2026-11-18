// Copyright (c) 2026 The QtDMM developers
// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <QMetaType>
#include <QString>

#include "core/sampletypes.h"

/// One reading as the views get it from the MeterController: made once,
/// right behind the decoder, from what ReadingAdapter makes of a value.
///
/// What a value measures and how - quantity, AC/DC, diode, overload, the
/// range - is in typed fields; nobody reads the decoders' mode codes or
/// unit spellings any more. The display strings (text, unit with the
/// decoder's prefix) stay for the views that show them as the meter does.
struct Reading
{
  double  value = 0;       ///< SI base units ("0.0123" for 12.3 mV)
  QString text;            ///< as the meter shows it: "12.30", "OL", "-OL"
  QString unit;            ///< as shown, prefix included: "mV", "kOhm"
  QString prefix;          ///< the SI prefix of the unit: "m", "k", "µ", ""
  QString baseUnit;        ///< the unit without prefix, the decoder's spelling: "V", "Ohm"
  QString range;           ///< "AUTO", "MANU" or the meter's range text
  bool    hold = false;    ///< the display is frozen (HOLD): an old value again
  bool    showBar = false; ///< the views draw a bar graph (the decoder has one, and the quantity a range)
  bool    overload = false;///< no number: OL, EFLO and the like
  int     id = 0;          ///< 0 = main value, 1..3 = secondary values
  qint64  msecs = 0;       ///< when it arrived (ms since the epoch)
  qint64  t = 0;           ///< when it arrived on the core clock (Sample::now()), ms, monotonic

  PortKey port;            ///< which port of the meter: quantity, AC/DC/diode, slot
  Unit    portUnit;        ///< the port's unit in one spelling: "Ω", "°C"
  quint32 flags = 0;       ///< SampleFlag: AC, DC, Diode, Hold, Autorange, LowBattery, ...
  Quality quality = Quality::Valid;
  Range   measuringRange;  ///< autorange and full scale (NaN when not known)
  QString name;            ///< the meter's own name of the value ("Battery voltage"), empty when none
  Battery battery = Battery::Unknown;   ///< the meter's battery, of the frame this value came in
  QString source;          ///< which meter it comes from (MeterController::source()): "device:<id>", "instance:<id>"

  /// The meter shows no number, or a fault word (text): no value, no overload.
  bool noNumber() const { return quality == Quality::NoValue || quality == Quality::Fault; }
  bool fault() const { return quality == Quality::Fault; }
  bool temperature() const { return port.quantity == Quantity::Temperature; }
  bool ac() const { return flags & SampleFlag::AC; }
  bool dc() const { return flags & SampleFlag::DC; }
  bool diode() const { return flags & SampleFlag::Diode; }
  bool continuity() const { return port.quantity == Quantity::Continuity; }
};

Q_DECLARE_METATYPE(Reading)
