//======================================================================
// File:		panelframe.h
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
//======================================================================

#pragma once

#include <QColor>
#include <QPixmap>
#include <QPointF>
#include <QRectF>
#include <QWidget>

class QPainter;

/// @file
/// The bezel and face shared by AnalogMeter and LcdWidget.

/// The instrument housing shared by the analog meter and the digital
/// display: a rounded metal bezel with a recessed face, lit from a point.
namespace PanelFrame
{
  /// What is left of the value's ink while it is stale (StaleRule): the
  /// display and the meter fade it alike.
  constexpr double kStaleOpacity = 0.35;

  /// Colours of the housing; AnalogMeterStyle and LcdWidget provide them.
  struct Colors
  {
    QColor bezelLight;
    QColor bezelDark;
    QColor face;
    QColor faceGlow;    // radial highlight on the face; transparent = none
  };

  /// Largest rect inside `available` whose aspect ratio (w/h) lies within
  /// [minAspect, maxAspect], centred. Keeps an instrument from being
  /// stretched into a strip when its dock is resized far beyond its shape;
  /// the rest of the widget shows the window background.
  QRectF panelRect(const QRectF &available, double minAspect, double maxAspect);

  /// Face rect for a bezel rect (the inset the frame uses); with @p plate
  /// the top of the bezel is wider, room for a name (paintPlate()).
  QRectF faceRect(const QRectF &bezel, bool plate = false);

  /// Paints bezel and face. glowCenter is where the face is lit from.
  void paint(QPainter &p, const QRectF &bezel, const Colors &c, const QPointF &glowCenter, double glowRadius,
             bool plate = false);

  /// @p text engraved in the wide top of the bezel (faceRect() with plate),
  /// like the model name on the front of an instrument.
  void paintPlate(QPainter &p, const QRectF &bezel, const QString &text, const Colors &c);

  /// Clip path of the face (rounded), for drawing that must stay inside.
  void clipToFace(QPainter &p, const QRectF &face);

  /// A transparent pixmap of @p widget's size at its device pixel ratio: the
  /// part of the instrument that is drawn once (renderStatic()).
  QPixmap staticPixmap(const QWidget *widget);

  /// Antialiased lines and text, as the instruments draw.
  void smooth(QPainter &p);
}
