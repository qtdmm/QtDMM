//======================================================================
// File:		colorbutton.h
// Author:	Matthias Toussaint
// Created:	Sam Jan 27 23:28:24 CET 2001
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
// Copyright (c) 2001 Matthias Toussaint
// SPDX-License-Identifier: GPL-3.0-or-later
//======================================================================

#pragma once

#include <qpushbutton.h>
#include <qcolor.h>

class QPainter;

/// A push button showing a colour swatch; clicking it opens QColorDialog.
class ColorButton : public QPushButton
{
  Q_OBJECT
public:
  ColorButton(QWidget *parent = nullptr);


  QColor	color() const;
  void		setColor(const QColor &);

Q_SIGNALS:
  /// The user picked a new colour.
  void		valueChanged();
  void		valueChanged(const QColor &);

protected Q_SLOTS:
  void		clickedSLOT();

protected:
  QColor	m_color;
};
