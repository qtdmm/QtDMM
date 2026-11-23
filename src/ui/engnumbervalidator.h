//======================================================================
// File:		engnumbervalidator.h
// Author:	Matthias Toussaint
// Created:	Fri Oct 11 20:29:37 CEST 2002
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

#pragma once

#include <QtCore>
#include <QtGui>

/// Validator for line edits taking numbers with an SI prefix ("1.5k", "22u").
class EngNumberValidator : public QValidator
{
public:
  EngNumberValidator(QObject *parent = nullptr);
  QValidator::State	validate(QString &, int &) const Q_DECL_OVERRIDE;

  /// "1.5k" -> 1500.0
  static double   value(const QString &);
  /// 1500.0 -> "1.5k", rounded to one decimal of the scaled value. For
  /// display only: 1234 becomes "1.2k".
  static QString  engValue(double);
  /// 1234.0 -> "1.234k": the same spelling, but with enough digits to read
  /// the value back unchanged. Use this wherever the text goes into an edit
  /// field the user confirms, or into a message that names a threshold.
  static QString  engText(double, int significantDigits = 9);

};

