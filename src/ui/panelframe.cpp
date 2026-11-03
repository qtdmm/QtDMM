//======================================================================
// File:		panelframe.cpp
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

#include "ui/panelframe.h"

#include <QPainter>
#include <QPainterPath>
#include <QLinearGradient>
#include <QRadialGradient>

namespace PanelFrame
{

// The same bezel on every instrument: a share of the height made the
// taller analog meter's frame thicker than the LCD's beside it. Only a
// panel too small for it gets a thinner one.
static double bezelWidth(const QRectF &bezel)
{
  return qBound(4.0, bezel.height() * 0.12, 12.0);
}

static double faceRadius(const QRectF &face)
{
  return qMin(9.0, face.height() * 0.05);
}

QRectF panelRect(const QRectF &available, double minAspect, double maxAspect)
{
  double w = available.width();
  double h = available.height();
  const double aspect = w / h;
  if (aspect > maxAspect)
    w = h * maxAspect;
  else if (aspect < minAspect)
    h = w / minAspect;
  return QRectF(available.center().x() - w / 2.0, available.center().y() - h / 2.0, w, h);
}

static double plateWidth(const QRectF &bezel)
{
  return qBound(13.0, bezel.height() * 0.11, 24.0);
}

QRectF faceRect(const QRectF &bezel, bool plate)
{
  const double w = bezelWidth(bezel);
  return bezel.adjusted(w, plate ? qMax(w, plateWidth(bezel)) : w, -w, -w);
}

void paintPlate(QPainter &p, const QRectF &bezel, const QString &text, const Colors &c)
{
  const QRectF face = faceRect(bezel, true);
  const QRectF band(face.left() + faceRadius(face), bezel.top() + 1, face.width() - 2 * faceRadius(face),
                    face.top() - bezel.top() - 1);
  QFont font = p.font();
  font.setBold(true);
  font.setLetterSpacing(QFont::PercentageSpacing, 108);
  font.setPixelSize(qMax(7, int(band.height() * 0.62)));
  p.save();
  p.setFont(font);
  const QString shown = QFontMetricsF(font).elidedText(text, Qt::ElideRight, band.width());
  // printed on the metal like the lettering of an instrument: light, with
  // a dark edge below, readable on the dark and the light bezels
  p.setPen(QColor(0, 0, 0, 110));
  p.drawText(band.translated(0, 1), Qt::AlignLeft | Qt::AlignVCenter, shown);
  p.setPen(QColor(200, 200, 200));
  p.drawText(band, Qt::AlignLeft | Qt::AlignVCenter, shown);
  p.restore();
}

void clipToFace(QPainter &p, const QRectF &face)
{
  const double r = faceRadius(face);
  QPainterPath clip;
  clip.addRoundedRect(face, r, r);
  p.setClipPath(clip);
}

void paint(QPainter &p, const QRectF &bezel, const Colors &c, const QPointF &glowCenter, double glowRadius, bool plate)
{
  const double radius = bezelWidth(bezel) * 1.2;

  QLinearGradient metal(bezel.topLeft(), bezel.bottomLeft());
  metal.setColorAt(0.0, c.bezelLight);
  metal.setColorAt(0.5, c.bezelDark.lighter(130));
  metal.setColorAt(1.0, c.bezelDark);
  p.setPen(QPen(c.bezelDark.darker(160), 1));
  p.setBrush(metal);
  p.drawRoundedRect(bezel, radius, radius);

  // thin highlight along the top edge
  p.setPen(QPen(QColor(255, 255, 255, 60), 1));
  p.setBrush(Qt::NoBrush);
  p.drawRoundedRect(bezel.adjusted(1, 1, -1, -1), radius, radius);

  // recessed face: base colour, lit from glowCenter, inner shadow
  const QRectF face = faceRect(bezel, plate);
  const double faceR = faceRadius(face);
  p.save();
  clipToFace(p, face);
  p.fillRect(face, c.face);

  if (c.faceGlow.alpha() > 0)
  {
    QRadialGradient glow(glowCenter, glowRadius);
    glow.setColorAt(0.0, c.faceGlow);
    glow.setColorAt(1.0, Qt::transparent);
    p.fillRect(face, glow);
  }

  for (int i = 0; i < 4; ++i)
  {
    p.setPen(QPen(QColor(0, 0, 0, 70 - i * 15), 1));
    p.setBrush(Qt::NoBrush);
    p.drawRoundedRect(face.adjusted(i, i, -i, -i), faceR, faceR);
  }
  p.restore();
}

}

QPixmap PanelFrame::staticPixmap(const QWidget *widget)
{
  const qreal dpr = widget->devicePixelRatioF();
  QPixmap pixmap(widget->size() * dpr);
  pixmap.setDevicePixelRatio(dpr);
  pixmap.fill(Qt::transparent);
  return pixmap;
}

void PanelFrame::smooth(QPainter &p)
{
  p.setRenderHint(QPainter::Antialiasing);
  p.setRenderHint(QPainter::TextAntialiasing);
}
