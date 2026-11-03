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

#include "device/dmmdecoder.h"

#include <algorithm>

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

// The manufacturers of the model list, longest first: "UNI-T" must not
// take the name of a "UNI-T UT61E+" apart before a longer one would
static const QStringList &vendors()
{
  static const QStringList list = [] {
    QStringList v;
    for (const auto &cfg : DmmDecoder::getDeviceConfigurations())
      if (!cfg.vendor.isEmpty() && !v.contains(cfg.vendor, Qt::CaseInsensitive))
        v << cfg.vendor;
    std::sort(v.begin(), v.end(), [](const QString &a, const QString &b) { return a.size() > b.size(); });
    return v;
  }();
  return list;
}

QPair<QString, QString> splitName(const QString &name)
{
  for (const QString &vendor : vendors())
    if (name.size() > vendor.size() + 1 && name.startsWith(vendor, Qt::CaseInsensitive) && name.at(vendor.size()) == ' ')
      return { name.left(vendor.size()), name.mid(vendor.size() + 1).trimmed() };
  return { QString(), name };
}

void paintPlate(QPainter &p, const QRectF &bezel, const QString &text, const Colors &c)
{
  const QRectF face = faceRect(bezel, true);
  const QRectF band(face.left() + faceRadius(face), bezel.top() + 1, face.width() - 2 * faceRadius(face),
                    face.top() - bezel.top() - 1);
  // a device's own name: the manufacturer bold on the left, the model on
  // the right as on the front of an instrument; a name given by the user
  // stays whole. Too narrow for both, the manufacturer goes first.
  const auto [vendor, model] = splitName(text);
  QFont bold = p.font();
  // with the panel, but never above the text around it
  bold.setPixelSize(qBound(7, int(band.height() * 0.62), QFontInfo(p.font()).pixelSize()));
  bold.setCapitalization(QFont::AllUppercase);
  QFont plain = bold;
  bold.setBold(true);
  bold.setLetterSpacing(QFont::AbsoluteSpacing, bold.pixelSize() * 0.2);
  plain.setLetterSpacing(QFont::AbsoluteSpacing, plain.pixelSize() * 0.12);
  const QFontMetricsF boldFm(bold);
  const QFontMetricsF plainFm(plain);
  const double gap = boldFm.height();
  const bool both = !vendor.isEmpty() && boldFm.horizontalAdvance(vendor) + gap + plainFm.horizontalAdvance(model) <= band.width();
  // the lettering of the metal: light on a dark bezel with a dark edge
  // below, dark on a light one with a light edge
  const bool light = c.bezelDark.lightness() > 140;
  const QColor ink = light ? QColor(40, 40, 40) : QColor(205, 205, 205);
  const QColor second = light ? QColor(85, 85, 85) : QColor(150, 150, 150);
  const QColor edge = light ? QColor(255, 255, 255, 110) : QColor(0, 0, 0, 110);
  auto engrave = [&](const QFont &font, const QString &str, Qt::Alignment align, const QColor &color) {
    p.setFont(font);
    const QString shown = QFontMetricsF(font).elidedText(str, Qt::ElideRight, band.width());
    p.setPen(edge);
    p.drawText(band.translated(0, 1), align | Qt::AlignVCenter, shown);
    p.setPen(color);
    p.drawText(band, align | Qt::AlignVCenter, shown);
  };
  p.save();
  if (both)
  {
    engrave(bold, vendor, Qt::AlignLeft, ink);
    engrave(plain, model, Qt::AlignRight, second);
  }
  else
    engrave(bold, model, Qt::AlignLeft, ink);
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
