// Copyright (c) 2026 The QtDMM developers
// SPDX-License-Identifier: GPL-3.0-or-later
//
// Texts of the graph's parts (qcpgraph_*.cpp); not part of its interface.

#pragma once

#include <QString>

#include "core/sampletypes.h"
#include "core/siprefix.h"
#include "recording/recordingseries.h"

/// "1:05:09", "5:09", "2 d 1:05:09"
inline QString durationText(qint64 seconds)
{
  const qint64 d = seconds / 86400, h = seconds / 3600 % 24, m = seconds / 60 % 60, sec = seconds % 60;
  QString text = h || d ? QString("%1:%2:%3").arg(h).arg(m, 2, 10, QChar('0')).arg(sec, 2, 10, QChar('0'))
                        : QString("%1:%2").arg(m).arg(sec, 2, 10, QChar('0'));
  return d ? QString("%1 d %2").arg(d).arg(text) : text;
}

/// What a curve shows: the meter's name of the value or "Temperature (°C)",
/// or the unit alone
inline QString curveName(const RecordingSeries &s, const QString &unit)
{
  const QString u = SiPrefix::displayText(unit);
  if (!s.name.isEmpty())
    return u.isEmpty() ? s.name : QString("%1 (%2)").arg(s.name, u);
  if (s.port.quantity == Quantity::Unknown)
    return u;
  QString text = Quantities::name(s.port.quantity);
  if (s.port.defining & (SampleFlag::AC | SampleFlag::DC))
    text += ' ' + couplingText(s.port.defining);
  return u.isEmpty() ? text : QString("%1 (%2)").arg(text, u);
}
