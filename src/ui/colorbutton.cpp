//======================================================================
// File:		colorbutton.cpp
// Author:	Matthias Toussaint
// Created:	Sam Jan 27 23:30:28 CET 2001
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

#include <QtGui>
#include <QtWidgets>

#include "ui/colorbutton.h"

ColorButton::ColorButton(QWidget *parent) : QPushButton(parent)
{
  m_color = QColor(255, 255, 255);
  setAutoDefault(false);

  connect(this, &QPushButton::clicked, this, &ColorButton::clickedSLOT);
}

QColor ColorButton::color() const
{
  return m_color;
}

void ColorButton::setColor(const QColor &c)
{
  m_color = c;

  QImage img(16, 12, QImage::Format_RGB32);
  img.fill(m_color.rgb());
  for (int i = 0; i < 16; ++i)
  {
    reinterpret_cast<QRgb *>(img.scanLine(0))[i] = 0;
    reinterpret_cast<QRgb *>(img.scanLine(11))[i] = 0;
  }

  for (int i = 0; i < 12; ++i)
  {
    reinterpret_cast<QRgb *>(img.scanLine(i))[0] = 0;
    reinterpret_cast<QRgb *>(img.scanLine(i))[15] = 0;
  }

  QPixmap pix;
  pix.convertFromImage(img);
  setIcon(pix);
}

void ColorButton::clickedSLOT()
{
  QColor c = QColorDialog::getColor(color(), this);

  if (c.isValid())
  {
    setColor(c);

    Q_EMIT valueChanged();
    Q_EMIT valueChanged(c);
  }
}
