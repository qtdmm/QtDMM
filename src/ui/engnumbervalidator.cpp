//======================================================================
// File:		engnumbervalidator.cpp
// Author:	Matthias Toussaint
// Created:	Fri Oct 11 20:32:01 CEST 2002
//----------------------------------------------------------------------
// This file is part of QtDMM.
//
// QtDMM is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
//
// QtDMM is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with QtDMM.  If not, see <http://www.gnu.org/licenses/>.
//----------------------------------------------------------------------
// Copyright (c) 2002 Matthias Toussaint
// SPDX-License-Identifier: GPL-3.0-or-later
//======================================================================

#include <QRegularExpression>

#include <math.h>
#include "ui/engnumbervalidator.h"
#include "core/siprefix.h"

EngNumberValidator::EngNumberValidator(QObject *parent) : QValidator(parent)
{
}

// TODO: RegExp is still a simple hack
//
QValidator::State EngNumberValidator::validate(QString &input, int &pos) const
{
  static const QRegularExpression doubleRe(R"(^-?\d+\.?\d*$)");
  static const QRegularExpression fullRe(R"(^-?\d+\.?\d*[munpkMGT]?$)");

  input = input.trimmed();
  pos = qMin(input.length(), pos);

  if (fullRe.match(input).hasMatch())
    return Acceptable;
  if (doubleRe.match(input).hasMatch())
    return Intermediate;

  return Invalid;
}


double EngNumberValidator::value(const QString &string)
{
  // A trailing prefix letter scales the number: "1.5k" -> 1500. "µ" and "u"
  // are both accepted, so what engValue() writes reads back correctly.
  const QString last = string.right(1);
  const double factor = SiPrefix::factor(last);
  const bool hasPrefix = factor != 1.0 || last == "u";
  return (hasPrefix ? string.chopped(1) : string).toDouble() * factor;
}

QString EngNumberValidator::engValue(double value)
{
  QString prefix;
  const double scaled = SiPrefix::scale(value, &prefix);

  QString str;
  str.setNum((static_cast<int>(qRound(scaled * 10.))) / 10.);
  return str + prefix;
}

QString EngNumberValidator::engText(double value, int significantDigits)
{
  QString prefix;
  const double scaled = SiPrefix::scale(value, &prefix);
  // 'g' drops trailing zeros and never writes an exponent within the range
  // a prefix covers, so this reads back through value() unchanged
  return QString::number(scaled, 'g', significantDigits) + prefix;
}
