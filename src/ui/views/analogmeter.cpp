//======================================================================
// File:		meterwid.cpp
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

#include "ui/views/analogmeter.h"
#include "core/reading.h"
#include "core/siprefix.h"
#include "ui/panelframe.h"

#include <QPainter>
#include <QPainterPath>
#include <QLinearGradient>
#include <QRadialGradient>
#include <QRegularExpression>
#include <QFontMetricsF>
#include <QtMath>
#include <cmath>
#include <limits>

namespace
{
const double kSweep = 45.0;      // needle travels -kSweep .. +kSweep
const double kStop = 49.5;       // mechanical end stops
const double kNaN = std::numeric_limits<double>::quiet_NaN();

// Ballistics: a slightly under-damped second order system, so the needle
// has the weight of a real moving coil and overshoots by a hair.
const double kOmega = 2.0 * M_PI * 1.5;   // rad/s
const double kZeta = 0.7;
const int kTickMs = 16;

// An alarm's colour over the dial: a third of it, so the scale stays readable
QColor mixed(const QColor &dial, const QColor &alert)
{
  const double k = 0.35;
  return QColor::fromRgbF(dial.redF() * (1 - k) + alert.redF() * k, dial.greenF() * (1 - k) + alert.greenF() * k,
                          dial.blueF() * (1 - k) + alert.blueF() * k);
}

// Point on a circle around the pivot; angle in degrees, 0 = straight up,
// positive = clockwise (to the right).
QPointF polar(const QPointF &pivot, double radius, double angleDeg)
{
  const double a = angleDeg * M_PI / 180.0;
  return QPointF(pivot.x() + radius * std::sin(a), pivot.y() - radius * std::cos(a));
}

// Arc around the pivot from angle a0 to a1 (our angle convention), as a
// sub-path that continues the current point when `lineToStart` is false.
void addArc(QPainterPath &path, const QPointF &pivot, double radius, double a0, double a1, bool moveToStart)
{
  const QRectF r(pivot.x() - radius, pivot.y() - radius, 2 * radius, 2 * radius);
  // Qt: 0 deg = 3 o'clock, counter-clockwise positive.
  const double qtStart = 90.0 - a0;
  const double qtSpan = -(a1 - a0);
  if (moveToStart)
    path.arcMoveTo(r, qtStart);
  path.arcTo(r, qtStart, qtSpan);
}

QFont scaledFont(double px, bool bold = false)
{
  QFont f;
  f.setPixelSize(qMax(5, int(std::lround(px))));
  f.setBold(bold);
  return f;
}

// scaledFont(), made smaller until @p text is at most @p maxWidth wide
QFont fittedFont(double px, bool bold, const QString &text, double maxWidth)
{
  QFont f = scaledFont(px, bold);
  const double w = QFontMetricsF(f).horizontalAdvance(text);
  if (w > maxWidth && w > 0)
    f = scaledFont(px * maxWidth / w, bold);
  return f;
}

}

AnalogMeterStyle AnalogMeterStyle::dark()
{
  AnalogMeterStyle s;
  s.face = QColor(0x1c, 0x1c, 0x1c);
  s.faceGlow = QColor(0x3c, 0x3c, 0x3c);
  s.bezelLight = QColor(0x5c, 0x5c, 0x5c);
  s.bezelDark = QColor(0x1e, 0x1e, 0x1e);
  s.scale = QColor(0xf2, 0xf2, 0xf2);
  s.needle = QColor(0xff, 0xff, 0xff);
  s.redZone = QColor(0xd8, 0x22, 0x22);
  s.redBand = QColor(0x8c, 0x26, 0x26);
  s.passBand = QColor(0x9c, 0xec, 0xa8);
  s.boxBg = QColor(0x2c, 0x2c, 0x2c);
  s.boxText = QColor(0xea, 0xea, 0xea);
  s.lampOff = QColor(0x4a, 0x12, 0x12);
  s.lampOn = QColor(0xff, 0x30, 0x30);
  s.hold = QColor(0xff, 0xb0, 0x20);
  s.minMark = QColor(0xe0, 0x40, 0x40);
  s.maxMark = QColor(0x50, 0xd0, 0x50);
  return s;
}

AnalogMeterStyle AnalogMeterStyle::ivory()
{
  AnalogMeterStyle s;
  s.face = QColor(0xf1, 0xe9, 0xd2);
  s.faceGlow = QColor(0xff, 0xf9, 0xe8);
  s.bezelLight = QColor(0x6c, 0x6c, 0x6c);
  s.bezelDark = QColor(0x28, 0x28, 0x28);
  s.scale = QColor(0x1e, 0x1e, 0x1e);
  s.needle = QColor(0x10, 0x10, 0x10);
  s.redZone = QColor(0xc8, 0x18, 0x18);
  s.redBand = QColor(0xe8, 0x8c, 0x80);
  s.passBand = QColor(0xa8, 0xe4, 0xae);
  s.boxBg = QColor(0xe4, 0xda, 0xbe);
  s.boxText = QColor(0x1e, 0x1e, 0x1e);
  s.lampOff = QColor(0x6a, 0x20, 0x20);
  s.lampOn = QColor(0xff, 0x30, 0x30);
  s.hold = QColor(0xb0, 0x60, 0x00);
  s.minMark = QColor(0xc0, 0x20, 0x20);
  s.maxMark = QColor(0x20, 0x90, 0x20);
  return s;
}

AnalogMeter::AnalogMeter(QWidget *parent)
  : QWidget(parent)
  , m_peak(kNaN)
  , m_markMin(kNaN)
  , m_markMax(kNaN)
{
  setAttribute(Qt::WA_OpaquePaintEvent, false);
  setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
  m_timer.setInterval(kTickMs);
  connect(&m_timer, &QTimer::timeout, this, &AnalogMeter::stepBallistics);
}

QSize AnalogMeter::sizeHint() const
{
  return QSize(480, 270);
}

QSize AnalogMeter::minimumSizeHint() const
{
  return QSize(240, 135);
}

// ---------------------------------------------------------------- values

double AnalogMeter::angleForValue(double value, double fullScale, bool bipolar)
{
  if (!(fullScale > 0.0) || std::isnan(value))
    return -kSweep;
  const double frac = bipolar ? (value / fullScale + 1.0) / 2.0 : value / fullScale;
  return qBound(-kStop, -kSweep + 2.0 * kSweep * frac, kStop);
}

double AnalogMeter::fullScaleFromReading(const QString &value, int counts)
{
  if (counts <= 0)
    return kNaN;
  static const QRegularExpression re("^\\s*[-+]?\\s*(\\d+)(?:\\.(\\d*))?\\s*$");
  const QRegularExpressionMatch m = re.match(value);
  if (!m.hasMatch())
    return kNaN;
  const int decimals = m.captured(2).size();
  return counts / std::pow(10.0, decimals);
}

double AnalogMeter::fullScaleFromReading(const QString &value, int counts, const QString &unit)
{
  if (SiPrefix::split(unit).baseUnit == QLatin1String("%"))
    return 100.0;
  return fullScaleFromReading(value, counts);
}

double AnalogMeter::fullScaleWithoutRange(double value, double current)
{
  // the value stays left of the red zone, which starts at 90 % of full scale
  const double need = std::fabs(value) / 0.9;
  double fs = 10.0;
  static const double kSteps[] = {1.0, 2.0, 5.0};
  for (double decade = 10.0; fs < need && decade < 1e12; decade *= 10.0)
    for (double s : kSteps)
      if ((fs = s * decade) >= need)
        break;
  return std::isnan(current) ? fs : qMax(fs, current);
}

void AnalogMeter::setReading(double value, const QString &text, const QString &unit, bool overload, bool hold)
{
  if (m_unit != unit)
  {
    m_unit = unit;
    m_staticDirty = true;
  }
  m_value = value;
  m_text = text;
  m_overload = overload;
  if (!overload)
  {
    // MIN and MAX use the meter's decimals ("0.L" would say one); the most
    // seen since reset(), so a minimum from the 4 V range keeps its three
    // decimals after a change to the 40 V range
    const QString shown = SiPrefix::withoutLeadingZeros(text);
    const int dot = shown.indexOf('.');
    m_decimals = qMax(m_decimals, dot < 0 ? 0 : qBound(0, int(shown.size() - dot - 1), 6));
  }
  m_hold = hold;

  if (m_scaleMode == Auto && !overload && value < -0.05 * m_fullScale && !m_latched)
  {
    m_latched = true;
    updatePolarity();
  }
  retarget();
  update();
}

void AnalogMeter::updatePolarity()
{
  const bool bipolar = !m_neverNegative && (m_scaleMode == Bipolar || (m_scaleMode == Auto && m_latched));
  if (bipolar != m_bipolar)
  {
    m_bipolar = bipolar;
    m_staticDirty = true;
  }
}

bool AnalogMeter::neverNegative(const QString &unit)
{
  const QString base = SiPrefix::split(unit).baseUnit;
  return base == QLatin1String("ppm") || base == QLatin1String("%") || base == QLatin1String("lx")
         || base == QLatin1String("fc") || base == QLatin1String("%RH") || base == QLatin1String("m/s")
         || base == QLatin1String("dBA") || base == QLatin1String("dBC");
}

void AnalogMeter::setFullScale(double fs)
{
  if (!(fs > 0.0) || qFuzzyCompare(fs, m_fullScale))
    return;
  m_fullScale = fs;
  m_staticDirty = true;
  retarget();
  update();
}

void AnalogMeter::setPeak(double value)
{
  m_peak = value;
  update();
}

void AnalogMeter::setMinMax(double minValue, double maxValue)
{
  m_markMin = minValue;
  m_markMax = maxValue;
  update();
}

void AnalogMeter::setScaleMode(ScaleMode mode)
{
  m_scaleMode = mode;
  updatePolarity();
  retarget();
  update();
}

void AnalogMeter::setStyle(const AnalogMeterStyle &style)
{
  m_style = style;
  m_staticDirty = true;
  if (!m_style.ballistics)
  {
    m_timer.stop();
    m_angle = m_target;
  }
  update();
}

void AnalogMeter::reset()
{
  m_peak = kNaN;
  m_markMin = kNaN;
  m_markMax = kNaN;
  m_decimals = 0;
  m_rangelessScale = kNaN;
  m_latched = false;
  updatePolarity();
  retarget();
  update();
}

void AnalogMeter::retarget()
{
  m_target = m_overload ? kStop : angleForValue(m_value, m_fullScale, m_bipolar);
  if (!m_style.ballistics)
  {
    m_angle = m_target;
    return;
  }
  if (!m_timer.isActive() && std::fabs(m_target - m_angle) > 0.05)
    m_timer.start();
}

void AnalogMeter::stepBallistics()
{
  const double dt = kTickMs / 1000.0;
  const double acc = kOmega * kOmega * (m_target - m_angle) - 2.0 * kZeta * kOmega * m_velocity;
  m_velocity += acc * dt;
  m_angle += m_velocity * dt;
  m_angle = qBound(-kStop - 1.0, m_angle, kStop + 1.0);

  if (std::fabs(m_target - m_angle) < 0.05 && std::fabs(m_velocity) < 0.5)
  {
    m_angle = m_target;
    m_velocity = 0.0;
    m_timer.stop();
  }
  update();
}

// -------------------------------------------------------------- geometry

AnalogMeter::Geometry AnalogMeter::geometry() const
{
  Geometry g;
  const QRectF r = PanelFrame::panelRect(QRectF(rect()).adjusted(1, 1, -1, -1), kMinAspect, kMaxAspect);
  g.bezel = r;
  g.face = PanelFrame::faceRect(r, !m_caption.isEmpty());
  const double fh = g.face.height();
  const double fw = g.face.width();
  // the arc ends at +-45 deg, the labels sit one font size outside it and
  // must stay clear of the bezel when the panel is narrower than 16:9
  g.radius = qMin(fh * 0.85, fw * 0.58);
  g.pivot = QPointF(g.face.center().x(), g.face.bottom() + fh * 0.02);
  g.fontPx = qBound(6.0, fh * 0.085, 48.0);
  return g;
}

void AnalogMeter::resizeEvent(QResizeEvent *)
{
  m_staticDirty = true;
}

// --------------------------------------------------------------- drawing

void AnalogMeter::drawBezel(QPainter &p, const Geometry &g) const
{
  PanelFrame::Colors c;
  c.bezelLight = m_style.bezelLight;
  c.bezelDark = m_style.bezelDark;
  c.face = dialColor();
  c.faceGlow = m_alert.isValid() ? mixed(m_style.faceGlow, m_alert) : m_style.faceGlow;
  PanelFrame::paint(p, g.bezel, c, g.pivot, g.radius * 1.2, !m_caption.isEmpty());
  if (!m_caption.isEmpty())
    PanelFrame::paintPlate(p, g.bezel, m_caption, c);
}

void AnalogMeter::setAlertColor(const QColor &color)
{
  if (color == m_alert)
    return;
  m_alert = color;
  m_staticDirty = true;
  update();
}

void AnalogMeter::setLimits(int id, double low, double high)
{
  if (id == m_limitId && low == m_limitLow && high == m_limitHigh)
    return;
  m_limitId = id;
  m_limitLow = low;
  m_limitHigh = high;
  m_staticDirty = true;
  update();
}

QColor AnalogMeter::dialColor() const
{
  return m_alert.isValid() ? mixed(m_style.face, m_alert) : m_style.face;
}

double AnalogMeter::niceStep(double range, int targetMajors)
{
  const double raw = range / qMax(1, targetMajors);
  const double mag = std::pow(10.0, std::floor(std::log10(raw)));
  const double norm = raw / mag;
  double step;
  if (norm < 1.5)
    step = 1.0;
  else if (norm < 3.5)
    step = 2.0;
  else if (norm < 7.5)
    step = 5.0;
  else
    step = 10.0;
  return step * mag;
}

QString AnalogMeter::formatLabel(double v)
{
  QString s = QString::number(v, 'f', 3);
  while (s.endsWith('0'))
    s.chop(1);
  if (s.endsWith('.'))
    s.chop(1);
  if (s == "-0")
    s = "0";
  return s;
}

double AnalogMeter::scaleStep(double fullScale, bool bipolar, double labelRadius, double labelWidth)
{
  const double range = bipolar ? 2.0 * fullScale : fullScale;
  if (!(range > 0.0))
    return 1.0;
  double step = niceStep(range, bipolar ? 8 : 5);
  const double pxPerUnit = labelRadius * qDegreesToRadians(2.0 * kSweep) / range;
  // 1 -> 2 -> 5 -> 10 until two neighbouring labels no longer touch
  for (int guard = 0; guard < 12 && step * pxPerUnit < labelWidth; ++guard)
  {
    const double mag = std::pow(10.0, std::floor(std::log10(step) + 1e-9));
    const double norm = step / mag;
    step = (norm < 1.5 ? 2.0 : norm < 3.5 ? 5.0 : 10.0) * mag;
  }
  return step;
}

void AnalogMeter::drawScale(QPainter &p, const Geometry &g) const
{
  const double R = g.radius;
  const double vMin = m_bipolar ? -m_fullScale : 0.0;
  const double vMax = m_fullScale;
  const QFont labelFont = scaledFont(g.fontPx, true);
  const QFontMetricsF fm(labelFont);
  // the widest label is one of the ends; a small gap between neighbours
  const double labelWidth = qMax(fm.horizontalAdvance(formatLabel(vMin)), fm.horizontalAdvance(formatLabel(vMax)))
                            + g.fontPx * 0.5;
  const double step = scaleStep(m_fullScale, m_bipolar, R + g.fontPx, labelWidth);
  const int minorsPerMajor = (std::fmod(std::lround(step / std::pow(10.0, std::floor(std::log10(step) + 1e-9))), 2) == 0) ? 4 : 5;
  const double minor = step / minorsPerMajor;
  auto angleOf = [&](double v) { return angleForValue(v, m_fullScale, m_bipolar); };

  // red zone: band on the arc from redZoneFrom * FS to the end (both ends
  // when bipolar); the arc and the ticks are drawn over it afterwards
  {
    auto band = [&](double v0, double v1)
    {
      QPainterPath path;
      addArc(path, g.pivot, R, angleOf(v0), angleOf(v1), true);   // the arc is its outer edge
      addArc(path, g.pivot, R * 0.94, angleOf(v1), angleOf(v0), false);
      path.closeSubpath();
      p.setPen(Qt::NoPen);
      p.setBrush(m_style.redBand);
      p.drawPath(path);
    };
    // the limit test's band instead of the red zone, in the unit shown,
    // cut at the ends
    const double from = m_style.redZoneFrom * m_fullScale;
    if (!limitsShown())
    {
      band(from, vMax);
      if (m_bipolar)
        band(-vMax, -from);
    }
    if (limitsShown())
    {
      const double factor = SiPrefix::factor(SiPrefix::split(m_unitText).prefix);
      const double lo = qMax(vMin, std::isnan(m_limitLow) ? vMin : m_limitLow / factor);
      const double hi = qMin(vMax, std::isnan(m_limitHigh) ? vMax : m_limitHigh / factor);
      if (hi > lo)
      {
        QPainterPath path;
        addArc(path, g.pivot, R, angleOf(lo), angleOf(hi), true);
        addArc(path, g.pivot, R * 0.94, angleOf(hi), angleOf(lo), false);
        path.closeSubpath();
        // light, so the green MAX mark stands out on it, with a darker
        // inner edge against the dial
        p.setPen(Qt::NoPen);
        p.setBrush(m_style.passBand);
        p.drawPath(path);
        QPainterPath edge;
        addArc(edge, g.pivot, R * 0.94, angleOf(lo), angleOf(hi), true);
        p.setPen(QPen(m_style.passBand.darker(140), qMax(1.0, R * 0.008), Qt::SolidLine, Qt::FlatCap));
        p.setBrush(Qt::NoBrush);
        p.drawPath(edge);
      }
    }
  }

  // main arc, with the underswing stub left of zero; over the red zone, so
  // the scale line runs through it
  QPainterPath arc;
  addArc(arc, g.pivot, R, -kStop, kSweep, true);
  p.setPen(QPen(m_style.scale, qMax(1.0, R * 0.012), Qt::SolidLine, Qt::FlatCap));
  p.setBrush(Qt::NoBrush);
  p.drawPath(arc);

  // ticks and labels, on multiples of the step counted from 0 - so 0 is
  // always labelled, also when the full scale is no multiple of the step
  // (22 V: -20 ... 20, not -17 ... 18)
  p.setFont(labelFont);
  const double majorLen = R * 0.11;
  const double minorLen = R * 0.055;
  const long kFrom = long(std::ceil(vMin / minor - 1e-6));
  const long kTo = long(std::floor(vMax / minor + 1e-6));
  // the ends of the scale always get a tick; a label only on a major one
  auto endTick = [&](double v)
  {
    if (std::fabs(v / minor - std::lround(v / minor)) < 1e-6)
      return;   // on the grid, drawn below
    p.setPen(QPen(m_style.scale, qMax(1.0, R * 0.012), Qt::SolidLine, Qt::FlatCap));
    p.drawLine(polar(g.pivot, R, angleOf(v)), polar(g.pivot, R - majorLen, angleOf(v)));
  };
  endTick(vMax);
  if (m_bipolar)
    endTick(vMin);
  for (long k = kFrom; k <= kTo; ++k)
  {
    const double v = k * minor;
    const bool major = (k % minorsPerMajor) == 0;
    const double a = angleOf(v);
    const bool inRed = !limitsShown() && std::fabs(v) >= m_style.redZoneFrom * m_fullScale - 1e-9 && v != 0.0;
    const QColor c = inRed ? m_style.redZone.lighter(115) : m_style.scale;
    p.setPen(QPen(m_style.scale, major ? qMax(1.0, R * 0.012) : qMax(0.8, R * 0.007), Qt::SolidLine, Qt::FlatCap));
    p.drawLine(polar(g.pivot, R, a), polar(g.pivot, R - (major ? majorLen : minorLen), a));

    if (major)
    {
      const QString label = formatLabel(v);
      const QPointF c0 = polar(g.pivot, R + g.fontPx * 1.0, a);
      const double w = fm.horizontalAdvance(label) + 4;
      const double h = fm.height();
      // the end labels sit close to the bezel: keep them on the dial
      QRectF box(c0.x() - w / 2, c0.y() - h / 2, w, h);
      const double inset = g.fontPx * 0.3;
      if (box.right() > g.face.right() - inset)
        box.moveRight(g.face.right() - inset);
      if (box.left() < g.face.left() + inset)
        box.moveLeft(g.face.left() + inset);
      if (box.top() < g.face.top() + inset)
        box.moveTop(g.face.top() + inset);
      p.setPen(c);
      p.drawText(box, Qt::AlignCenter, label);
    }
  }

  // pivot cap at the bottom edge of the dial
  {
    const double capR = g.face.height() * 0.09;
    p.save();
    QPainterPath faceClip;
    faceClip.addRect(g.face);
    p.setClipPath(faceClip);
    QRadialGradient cap(g.pivot - QPointF(capR * 0.3, capR * 0.6), capR * 1.4);
    cap.setColorAt(0.0, m_style.bezelLight.lighter(120));
    cap.setColorAt(1.0, m_style.bezelDark);
    p.setPen(QPen(m_style.bezelDark.darker(150), 1));
    p.setBrush(cap);
    p.drawEllipse(g.pivot, capR, capR);
    p.setPen(Qt::NoPen);
    p.setBrush(m_style.scale);
    p.drawEllipse(g.pivot - QPointF(0, capR * 0.55), capR * 0.09, capR * 0.09);
    if (m_bipolar)
    {
      p.setFont(scaledFont(g.fontPx * 0.7, true));
      p.setPen(m_style.scale);
      const double h = g.fontPx;
      p.drawText(QRectF(g.pivot.x() - capR * 2.2, g.pivot.y() - capR * 0.9, capR, h), Qt::AlignCenter, QStringLiteral("−"));
      p.drawText(QRectF(g.pivot.x() + capR * 1.2, g.pivot.y() - capR * 0.9, capR, h), Qt::AlignCenter, QStringLiteral("+"));
    }
    p.restore();
  }
}

void AnalogMeter::drawBoxes(QPainter &p, const Geometry &g) const
{
  const double fw = g.face.width();
  const double fh = g.face.height();
  const double rr = fh * 0.03;

  auto box = [&](const QRectF &r)
  {
    p.setPen(QPen(QColor(0, 0, 0, 120), 1));
    p.setBrush(m_style.boxBg);
    p.drawRoundedRect(r, rr, rr);
    p.setPen(QPen(QColor(255, 255, 255, 30), 1));
    p.setBrush(Qt::NoBrush);
    p.drawRoundedRect(r.adjusted(1, 1, -1, -1), rr, rr);
  };

  // unit label in the middle, straight on the dial like the "VU" legend;
  // long labels ("mV AC+DC", "V DIODE") get a smaller font instead of
  // running over
  const QString unit = m_unit.isEmpty() ? QStringLiteral("—") : m_unit;
  const QRectF unitRect(g.face.center().x() - fw * 0.17, g.face.top() + fh * 0.60, fw * 0.34, fh * 0.17);
  p.setPen(m_style.scale);
  if (m_notice.isEmpty())
  {
    p.setFont(fittedFont(g.fontPx * 1.35, true, unit, unitRect.width()));
    p.drawText(unitRect, Qt::AlignCenter, unit);
  }
  else
  {
    // no value yet: why, in place of the unit
    const QRectF wide(g.face.center().x() - fw * 0.3, unitRect.top(), fw * 0.6, unitRect.height());
    p.setFont(fittedFont(g.fontPx * 0.9, false, m_notice, wide.width()));
    p.drawText(wide, Qt::AlignCenter, m_notice);
  }

  // readout boxes with their captions: minimum left, maximum right
  QRectF min, max;
  readoutRects(g, &min, &max);
  box(min);
  box(max);
  p.setFont(scaledFont(g.fontPx * 0.7));
  p.setPen(m_style.scale.darker(m_style.face.lightness() < 128 ? 150 : 100));
  p.drawText(QRectF(min.left(), min.top() - g.fontPx * 1.05, min.width(), g.fontPx), Qt::AlignLeft | Qt::AlignVCenter, tr("MIN"));
  p.drawText(QRectF(max.left(), max.top() - g.fontPx * 1.05, max.width(), g.fontPx), Qt::AlignRight | Qt::AlignVCenter, tr("MAX"));

  // "OL" caption next to the lamp
  const QPointF lamp = lampCenter(g);
  const QFont olFont = scaledFont(g.fontPx * 0.7, true);
  p.setFont(olFont);
  p.drawText(QRectF(lamp.x() + fh * 0.05, lamp.y() - g.fontPx * 0.5, QFontMetricsF(olFont).horizontalAdvance(tr("OL")) + 2, g.fontPx),
             Qt::AlignLeft | Qt::AlignVCenter, tr("OL"));
}

QPointF AnalogMeter::lampCenter(const Geometry &g) const
{
  const double fw = g.face.width();
  const double fh = g.face.height();
  // on a narrow dial the caption would run off the face at the usual place
  const double caption = QFontMetricsF(scaledFont(g.fontPx * 0.7, true)).horizontalAdvance(tr("OL")) + 2;
  const double x = qMin(g.face.right() - fw * 0.11, g.face.right() - fw * 0.03 - caption - fh * 0.05);
  return QPointF(x, g.face.top() + fh * 0.66);
}

// The boxes sit in the bottom corners, as wide as they can be while the
// needle - even at its end stop - passes to their inner side: at a height
// h above the pivot it is h * tan(49.5 deg) off the centre line.
void AnalogMeter::readoutRects(const Geometry &g, QRectF *min, QRectF *max) const
{
  const double fw = g.face.width();
  const double fh = g.face.height();
  const double top = g.face.top() + fh * 0.80;
  const double height = fh * 0.15;
  const double clear = (g.pivot.y() - top) * std::tan(qDegreesToRadians(kStop)) + fh * 0.03;
  const double margin = fw * 0.03;
  const double width = qMin(fw * 0.28, fw / 2 - clear - margin);
  *min = QRectF(g.face.left() + margin, top, width, height);
  *max = QRectF(g.face.right() - margin - width, top, width, height);
}

QString AnalogMeter::readoutText(double value) const
{
  return readoutFor(value, m_decimals, m_fullScale, m_counts, m_unitText);
}

QString AnalogMeter::readoutFor(double value, int decimals, double fullScale, int counts, const QString &unit)
{
  if (std::isnan(value))
    return readoutString(value, decimals);
  const int digits = counts > 0 ? int(QString::number(counts).size()) : 5;
  auto intDigits = [](double v) { return std::fabs(v) < 1.0 ? 1 : int(std::floor(std::log10(std::fabs(v)))) + 1; };
  const SiPrefix::Split split = SiPrefix::split(unit);
  const QString base = split.baseUnit;
  if (SiPrefix::takesPrefix(base) && fullScale > 0.0 && std::fabs(value) > fullScale * 1.0001)
  {
    // from another range: its own prefix, and the unit says which
    QString prefix;
    const double scaled = SiPrefix::scale(value * SiPrefix::factor(split.prefix), &prefix);
    return readoutString(scaled, qBound(0, digits - intDigits(scaled), 6)) + " " + SiPrefix::displayText(prefix + base);
  }
  const QString text = readoutString(value, qBound(0, qMin(decimals, digits - intDigits(value)), 6));
  return unit.isEmpty() ? text : text + " " + SiPrefix::displayText(unit);
}

QString AnalogMeter::readoutString(double value, int decimals)
{
  if (std::isnan(value))
    return QStringLiteral("—");
  QString s = QString::number(value, 'f', decimals);
  // -0.004 with two decimals is "-0.00"; no sign on a zero (as formatLabel)
  if (s.startsWith('-') && s.toDouble() == 0.0)
    s.remove(0, 1);
  return s;
}

void AnalogMeter::drawReadouts(QPainter &p, const Geometry &g) const
{
  const double fw = g.face.width();
  const double fh = g.face.height();
  QRectF min, max;
  readoutRects(g, &min, &max);

  p.setPen(m_style.boxText);
  auto readout = [&](const QRectF &r, const QString &text)
  {
    const QRectF inner = r.adjusted(fh * 0.02, 0, -fh * 0.02, 0);
    p.setFont(fittedFont(g.fontPx * 1.15, true, text, inner.width()));
    p.drawText(inner, Qt::AlignCenter, text);
  };
  readout(min, readoutText(m_markMin));
  // the maximum of the min/max memory, or the peak where only that is set
  readout(max, readoutText(std::isnan(m_markMax) ? m_peak : m_markMax));

  if (m_hold)
  {
    p.setFont(scaledFont(g.fontPx * 0.8, true));
    p.setPen(m_style.hold);
    p.drawText(QRectF(g.face.left() + fw * 0.05, g.face.top() + fh * 0.60, fw * 0.2, g.fontPx * 1.2),
               Qt::AlignLeft | Qt::AlignVCenter, tr("HOLD"));
  }
}

void AnalogMeter::drawLamp(QPainter &p, const Geometry &g) const
{
  const double fh = g.face.height();
  const QPointF c = lampCenter(g);
  const double r = fh * 0.035;

  if (m_overload)
  {
    QRadialGradient halo(c, r * 3.0);
    halo.setColorAt(0.0, QColor(m_style.lampOn.red(), m_style.lampOn.green(), m_style.lampOn.blue(), 140));
    halo.setColorAt(1.0, Qt::transparent);
    p.setPen(Qt::NoPen);
    p.setBrush(halo);
    p.drawEllipse(c, r * 3.0, r * 3.0);
  }

  QRadialGradient lamp(c - QPointF(r * 0.3, r * 0.3), r * 1.3);
  const QColor base = m_overload ? m_style.lampOn : m_style.lampOff;
  lamp.setColorAt(0.0, base.lighter(m_overload ? 160 : 120));
  lamp.setColorAt(1.0, base.darker(140));
  p.setPen(QPen(QColor(0, 0, 0, 150), 1));
  p.setBrush(lamp);
  p.drawEllipse(c, r, r);
}

// Small triangles just inside the arc, pointing at the scale: red for the
// minimum, green for the maximum of the min/max memory (as Ultra DMM does).
void AnalogMeter::drawMarks(QPainter &p, const Geometry &g) const
{
  auto mark = [&](double value, const QColor &color)
  {
    if (std::isnan(value))
      return;
    const double angle = angleForValue(value, m_fullScale, m_bipolar);
    const double h = qMax(3.0, g.radius * 0.055);
    QPolygonF tri;
    // inside the arc, tip touching it, so the scale labels stay clear
    tri << QPointF(0, -(g.radius * 0.985)) << QPointF(-h * 0.6, -(g.radius * 0.985 - h)) << QPointF(h * 0.6, -(g.radius * 0.985 - h));
    p.save();
    PanelFrame::clipToFace(p, g.face);
    p.translate(g.pivot);
    p.rotate(angle);
    p.setPen(QPen(QColor(0, 0, 0, 120), 1));
    p.setBrush(color);
    p.drawPolygon(tri);
    p.restore();
  };
  mark(m_markMin, m_style.minMark);
  mark(m_markMax, m_style.maxMark);
}

void AnalogMeter::drawNeedle(QPainter &p, const Geometry &g) const
{
  const double len = g.radius * 1.02;
  const double baseHalf = qMax(1.2, g.face.height() * 0.011);
  const double tipHalf = qMax(0.4, baseHalf * 0.2);

  // needle polygon in a frame pointing straight up, then rotated
  QPolygonF poly;
  poly << QPointF(-baseHalf, 0) << QPointF(-tipHalf, -len) << QPointF(tipHalf, -len) << QPointF(baseHalf, 0);

  p.save();
  PanelFrame::clipToFace(p, g.face);

  // shadow
  p.save();
  p.translate(g.pivot + QPointF(baseHalf * 1.5, baseHalf * 1.5));
  p.rotate(m_angle);
  p.setPen(Qt::NoPen);
  p.setBrush(QColor(0, 0, 0, 110));
  p.drawPolygon(poly);
  p.restore();

  p.translate(g.pivot);
  p.rotate(m_angle);
  p.setPen(Qt::NoPen);
  p.setBrush(m_style.needle);
  p.drawPolygon(poly);
  p.restore();
}

void AnalogMeter::renderStatic()
{
  m_static = PanelFrame::staticPixmap(this);
  QPainter p(&m_static);
  PanelFrame::smooth(p);
  const Geometry g = geometry();
  drawBezel(p, g);
  drawScale(p, g);
  drawBoxes(p, g);
  m_staticDirty = false;
}

void AnalogMeter::setCaption(const QString &caption)
{
  if (caption == m_caption)
    return;
  m_caption = caption;
  m_staticDirty = true;
  update();
}

void AnalogMeter::setNotice(const QString &notice)
{
  if (notice == m_notice)
    return;
  m_notice = notice;
  m_staticDirty = true;
  update();
}

void AnalogMeter::setStale(bool stale)
{
  if (stale == m_stale)
    return;
  m_stale = stale;
  update();
}

void AnalogMeter::paintEvent(QPaintEvent *)
{
  if (m_staticDirty || m_static.isNull())
    renderStatic();

  QPainter p(this);
  PanelFrame::smooth(p);
  p.drawPixmap(0, 0, m_static);

  const Geometry g = geometry();
  if (m_stale)
    p.setOpacity(PanelFrame::kStaleOpacity);
  drawReadouts(p, g);
  drawLamp(p, g);
  drawMarks(p, g);
  drawNeedle(p, g);
}

// ---------------------------------------------------------------- MeterController feed

// The analog meter works in the unit the multimeter displays (with prefix),
// so its full scale follows the display count and the decimals of the
// reading, exactly like the meter's own bar graph.
void AnalogMeter::setValueId(int id)
{
  if (id == m_valueId)
    return;
  m_valueId = id;
  m_rangelessScale = kNaN;
  m_staticDirty = true;   // the limit band belongs to one value
  clearMinMax();
}

void AnalogMeter::showReading(const Reading &r)
{
  if (r.id != m_valueId)
    return;
  const QString &val = r.text;
  const QString &unit = r.unit;

  if (r.temperature())
  {
    // no range: the display count would give 5000 °C for "37.2" at 50000
    // counts, so the scale follows the values instead
    if (unit != m_unitText)
      m_rangelessScale = kNaN;
    if (!r.overload && !r.noNumber())
      m_rangelessScale = fullScaleWithoutRange(QString(val).remove(' ').toDouble(), m_rangelessScale);
    if (!std::isnan(m_rangelessScale))
      setFullScale(m_rangelessScale);
  }
  else
  {
    m_rangelessScale = kNaN;
    const double fs = fullScaleFromReading(val, m_counts, unit);
    if (!std::isnan(fs))
      setFullScale(fs);
  }

  // the unit as the digital display writes it ("kΩ", "°C")
  QString label = SiPrefix::displayText(unit);
  if (r.diode())
    label += " DIODE";
  else if (r.continuity())
    label += " CONT";
  else if (!couplingText(r.flags).isEmpty())
    label += " " + couplingText(r.flags);

  // another prefix moves the limit test's band
  if (unit != m_unitText && limitsShown())
    m_staticDirty = true;
  m_unitText = unit;
  m_neverNegative = neverNegative(unit);
  updatePolarity();
  // no number shown ("----", "FUSE"): the needle rests
  const double value = r.overload || r.noNumber() ? 0.0 : QString(val).remove(' ').toDouble();
  setReading(value, val, label, r.overload, r.hold);
  applyMinMax();
}

void AnalogMeter::showValueMinMax(int id, double minimum, double maximum)
{
  if (id != m_valueId)
    return;
  if (std::isnan(minimum) && std::isnan(maximum))
  {
    clearMinMax();
    return;
  }
  m_minBase = minimum;
  m_maxBase = maximum;
  applyMinMax();
}

void AnalogMeter::clearMinMax()
{
  m_minBase = m_maxBase = std::numeric_limits<double>::quiet_NaN();
  reset();
}

// min/max memory is kept in SI base units; bring it into display units
void AnalogMeter::applyMinMax()
{
  const double factor = SiPrefix::factor(SiPrefix::split(m_unitText).prefix);
  const double minMark = m_minBase / factor;   // NaN stays NaN
  const double maxMark = m_maxBase / factor;
  if (!std::isnan(maxMark))
    setPeak(maxMark);
  setMinMax(minMark, maxMark);
}
