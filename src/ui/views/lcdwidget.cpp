//======================================================================
// File:		displaywid.cpp
// Author:	Matthias Toussaint
// Created:	Thu Dec 26 12:04:35 CET 2002
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
//======================================================================

#include "ui/views/lcdwidget.h"
#include "core/reading.h"
#include "ui/panelframe.h"
#include "core/siprefix.h"

#include <QPainter>
#include <QPainterPath>
#include <QFontMetricsF>
#include <cmath>

namespace
{
enum Segment { A = 1, B = 2, C = 4, D = 8, E = 16, F = 32, G = 64 };

// Proportions of a seven-segment cell, relative to its height.
const double kDigitW = 0.56;      // cell width
const double kThick = 0.14;       // segment thickness
const double kGap = 0.018;        // gap between segments
const double kAdvance = 0.74;     // cell pitch
const double kDpW = 0.22;         // decimal point pitch
const double kSlant = 0.08;       // italic shear

// Housing colours: same dark metal as the analog meter.
const QColor kBezelLight(0x5c, 0x5c, 0x5c);
const QColor kBezelDark(0x1e, 0x1e, 0x1e);

QFont sansFont(double px, bool bold = true)
{
  QFont f;
  f.setPixelSize(qMax(5, int(std::lround(px))));
  f.setBold(bold);
  return f;
}
}

LcdWidget::LcdWidget(QWidget *parent)
  : QWidget(parent)
  , m_face(0xda, 0xdc, 0x77)
  , m_segment(0x16, 0x1e, 0x14)
{
  setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
  setDisplayMode(4000, true, true, 1);
}

QSize LcdWidget::sizeHint() const
{
  return QSize(m_limitOn ? int(520 * kLimitWiden) : 520, 200);
}

QSize LcdWidget::minimumSizeHint() const
{
  return QSize(260, 100);
}

// ---------------------------------------------------------------- state

void LcdWidget::setValue(int id, const QString &v)
{
  if (id < 0 || id >= kMaxValues)
    return;
  m_value[id] = v;
  fitDigits(v);
}
void LcdWidget::setUnit(int id, const QString &v)
{
  if (id >= 0 && id < kMaxValues)
    m_unit[id] = v;
}

void LcdWidget::setPrimary(int id)
{
  if (id == m_primary)
    return;
  m_primary = id;
  m_staticDirty = true;
  update();
}

int LcdWidget::primaryShown() const
{
  return m_primary > 0 && m_primary < m_numValues ? m_primary : 0;
}

void LcdWidget::setSecondaries(const std::optional<QList<int>> &ids)
{
  if (m_secondaries == ids)
    return;
  m_secondaries = ids;
  m_staticDirty = true;
  update();
}

QList<int> LcdWidget::secondaries() const
{
  const int primary = primaryShown();
  QList<int> ids;
  if (m_secondaries)
    for (int id : *m_secondaries)
      if (id >= 0 && id < m_numValues && id != primary && !ids.contains(id) && ids.size() < kSmallLineValues)
        ids << id;
  // a free place takes the next value: a choice made with a meter of two
  // values left the third one of the next meter off the display
  for (int id = 0; id < m_numValues && ids.size() < kSmallLineValues; ++id)
    if (id != primary && !ids.contains(id))
      ids << id;
  return ids;
}

// the big value first, then the small line, then those not shown
int LcdWidget::idAt(int line) const
{
  QList<int> order = secondaries();
  order.prepend(primaryShown());
  for (int id = 0; id < kMaxValues; ++id)
    if (!order.contains(id))
      order << id;
  return order.value(line, 0);
}
void LcdWidget::setMode(int id, quint32 flags, Quantity quantity)
{
  if (id < 0 || id >= kMaxValues)
    return;
  m_flags[id] = flags;
  m_quantity[id] = quantity;
}
void LcdWidget::setMinValue(const QString &v)
{
  m_minValue = v;
  fitDigits(v);
}
void LcdWidget::setMaxValue(const QString &v)
{
  m_maxValue = v;
  fitDigits(v);
}
void LcdWidget::setMinUnit(const QString &v) { m_minUnit = v; }
void LcdWidget::setMaxUnit(const QString &v) { m_maxUnit = v; }
void LcdWidget::setShowBar(bool on) { m_paintBar = on; }
void LcdWidget::setHold(bool on) { m_hold = on; }

void LcdWidget::setAuto(bool on)
{
  m_auto = on;
}

void LcdWidget::setManu(bool on)
{
  m_auto = !on;
}

LcdWidget::Look LcdWidget::lookOf(DeviceType::Type type)
{
  switch (type)
  {
    case DeviceType::Thermometer:
      return { false, AnnHold | AnnMax | AnnMin };
    case DeviceType::Luxmeter:
    case DeviceType::SoundLevel:
      return { false, AnnAuto | AnnHold | AnnMax | AnnMin };
    case DeviceType::Anemometer:
    case DeviceType::Environment:
      return { false, AnnHold | AnnMax | AnnMin | AnnAvg };
    case DeviceType::BatteryMonitor:
    case DeviceType::SolarCharger:
    case DeviceType::Inverter:
      return { false, 0 };
    default:   // the multimeters, the Simulated meter: all of it
      return {};
  }
}

void LcdWidget::setLook(const Look &look)
{
  if (look.bar == m_look.bar && look.annunciators == m_look.annunciators)
    return;
  m_look = look;
  m_staticDirty = true;
  update();
}

void LcdWidget::setDisplayMode(int counts, bool minMax, bool bar, int numValues)
{
  m_counts = counts > 0 ? counts : 4000;
  m_seenDigits = 0;
  m_showMinMax = minMax;
  m_showBar = bar;
  setValueCount(numValues);
}

void LcdWidget::setValueCount(int numValues)
{
  m_numValues = qBound(1, numValues, kMaxValues);
  m_staticDirty = true;
  updateGeometry();
  update();
}

void LcdWidget::setFaceColor(const QColor &c)
{
  if (!c.isValid())
    return;
  m_customFace = c;
  applyVariant();
}

void LcdWidget::setLcdVariant(LcdVariant variant)
{
  m_variant = variant;
  applyVariant();
}

void LcdWidget::setLimitTest(bool on)
{
  if (on == m_limitOn)
    return;
  m_limitOn = on;
  m_verdict = LimitTest::NoVerdict;
  m_staticDirty = true;
  updateGeometry();
  update();
}

void LcdWidget::setVerdict(LimitTest::Verdict verdict, double deviation)
{
  m_verdict = verdict;
  m_deviation = deviation;
  update();
}

void LcdWidget::setAlertColor(const QColor &color)
{
  if (color == m_alert)
    return;
  m_alert = color;
  applyVariant();
}

QColor LcdWidget::classicFace()
{
  return QColor(0xda, 0xdc, 0x77);
}

// Face and segments; the glow and the ghost segments follow from them.
void LcdWidget::applyVariant()
{
  switch (m_variant)
  {
    case Classic:
      m_face = classicFace();
      m_segment = QColor(0x16, 0x1e, 0x14);
      break;
    case BacklightBlue:
      m_face = QColor(0x1d, 0x5c, 0xc4);
      m_segment = QColor(0xe9, 0xf3, 0xff);
      break;
    case Amber:
      m_face = QColor(0xf2, 0xa9, 0x3b);
      m_segment = QColor(0x24, 0x16, 0x02);
      break;
    case HighContrast:
      m_face = QColor(0xf6, 0xf6, 0xf2);
      m_segment = Qt::black;
      break;
    case Custom:
      m_face = m_customFace.isValid() ? m_customFace : classicFace();
      m_segment = QColor(0x16, 0x1e, 0x14);
      break;
  }
  if (m_alert.isValid())
  {
    // light or dark segments, whichever has more contrast on the colour
    // (WCAG relative luminance: on the default alarm red light ones)
    auto luminance = [](const QColor &c)
    {
      auto lin = [](double v) { return v <= 0.03928 ? v / 12.92 : std::pow((v + 0.055) / 1.055, 2.4); };
      return 0.2126 * lin(c.redF()) + 0.7152 * lin(c.greenF()) + 0.0722 * lin(c.blueF());
    };
    const QColor light(0xf4, 0xf4, 0xf0), dark(0x16, 0x1e, 0x14);
    const double l = luminance(m_alert);
    const double onLight = (luminance(light) + 0.05) / (l + 0.05);
    const double onDark = (l + 0.05) / (luminance(dark) + 0.05);
    m_face = m_alert;
    m_segment = onLight >= onDark ? light : dark;
  }
  m_staticDirty = true;
  update();
}

QString LcdWidget::lcdVariantName(LcdVariant variant)
{
  switch (variant)
  {
    case BacklightBlue: return "blue";
    case Amber:         return "amber";
    case HighContrast:  return "contrast";
    case Custom:        return "custom";
    default:            return "classic";
  }
}

LcdWidget::LcdVariant LcdWidget::lcdVariantFromName(const QString &name)
{
  for (LcdVariant v : { BacklightBlue, Amber, HighContrast, Custom })
    if (name == lcdVariantName(v))
      return v;
  return Classic;
}

QColor LcdWidget::ghost() const
{
  QColor c = m_segment;
  // light segments on a dark face (backlight blue) stand out much more:
  // their ghosts must stay fainter than dark ghosts on a light face
  if (m_segment.lightness() > m_face.lightness())
    c.setAlpha(18);
  else
    c.setAlpha(m_face.lightness() < 128 ? 40 : 26);
  return c;
}

int LcdWidget::numDigits() const
{
  // digits of the display count: 2000 -> 4, 22000 -> 5, 1000000 -> 7; more
  // when a value had more (a Victron current "-10.967" with 6000 counts),
  // else the unit would sit on its last digit
  return qMax(int(QString::number(m_counts).size()), m_seenDigits);
}

void LcdWidget::fitDigits(const QString &value)
{
  int cells = 0;
  for (QChar ch : value.trimmed())
    if (ch != '-' && ch != '+' && ch != '.' && ch != ',' && !ch.isSpace())
      ++cells;
  if (cells <= numDigits())
    return;
  m_seenDigits = cells;
  m_staticDirty = true;
}

// ---------------------------------------------------------- segments

int LcdWidget::segmentsFor(QChar ch)
{
  switch (ch.toLower().toLatin1())
  {
    case '0': return A | B | C | D | E | F;
    case '1': return B | C;
    case '2': return A | B | G | E | D;
    case '3': return A | B | G | C | D;
    case '4': return F | G | B | C;
    case '5': return A | F | G | C | D;
    case '6': return A | F | G | E | D | C;
    case '7': return A | B | C;
    case '8': return A | B | C | D | E | F | G;
    case '9': return A | B | C | D | F | G;
    case '-': return G;
    case 'a': return A | B | C | E | F | G;
    case 'b': return F | E | G | C | D;
    case 'c': return G | E | D;
    case 'd': return B | C | D | E | G;
    case 'e': return A | F | G | E | D;
    case 'f': return A | F | G | E;
    case 'h': return F | E | G | C;
    case 'l': return F | E | D;
    case 'n': return E | G | C;
    case 'o': return G | E | C | D;
    case 'p': return A | B | F | G | E;
    case 'r': return E | G;
    case 's': return A | F | G | C | D;
    case 't': return F | G | E | D;
    case 'u': return B | C | D | E | F;
    case 'y': return F | B | G | C | D;
    default: return 0;
  }
}

void LcdWidget::drawDigit(QPainter &p, const QPointF &origin, double h, int mask) const
{
  const double w = h * kDigitW;
  const double t = h * kThick;
  const double g = h * kGap;
  const double half = t / 2.0;

  // A segment is a hexagon: a bar with pointed ends, so neighbours meet at
  // 45 degrees like on a real LCD.
  auto horizontal = [&](double y, double x0, double x1)
  {
    QPolygonF poly;
    poly << QPointF(x0 + g, y) << QPointF(x0 + g + half, y - half) << QPointF(x1 - g - half, y - half)
         << QPointF(x1 - g, y) << QPointF(x1 - g - half, y + half) << QPointF(x0 + g + half, y + half);
    return poly;
  };
  auto vertical = [&](double x, double y0, double y1)
  {
    QPolygonF poly;
    poly << QPointF(x, y0 + g) << QPointF(x + half, y0 + g + half) << QPointF(x + half, y1 - g - half)
         << QPointF(x, y1 - g) << QPointF(x - half, y1 - g - half) << QPointF(x - half, y0 + g + half);
    return poly;
  };

  const double top = half, mid = h / 2.0, bottom = h - half;
  const double left = half, right = w - half;
  const QPolygonF segs[7] = {
    horizontal(top, left, right),          // a
    vertical(right, top, mid),             // b
    vertical(right, mid, bottom),          // c
    horizontal(bottom, left, right),       // d
    vertical(left, mid, bottom),           // e
    vertical(left, top, mid),              // f
    horizontal(mid, left, right),          // g
  };

  p.save();
  p.translate(origin);
  p.shear(-kSlant, 0.0);
  p.setPen(Qt::NoPen);
  for (int i = 0; i < 7; ++i)
  {
    p.setBrush((mask & (1 << i)) ? m_segment : ghost());
    p.drawPolygon(segs[i]);
  }
  p.restore();
}

double LcdWidget::numberWidth(double h, int digits) const
{
  // sign cell + digits + one decimal point
  return (digits + 1) * h * kAdvance + h * kDpW;
}

// Draws a reading right-aligned in a field of minDigits digits: leading
// cells stay ghosted, the sign has its own cell, the decimal point sits
// between cells. Returns the field width.
double LcdWidget::drawNumber(QPainter &p, const QPointF &origin, double h, const QString &text, int minDigits, bool lit) const
{
  const double adv = h * kAdvance;
  const double dpW = h * kDpW;
  const double fieldW = numberWidth(h, minDigits);

  QString s = text.trimmed();
  bool negative = false;
  if (s.startsWith('-'))
  {
    negative = true;
    s.remove(0, 1);
  }
  else if (s.startsWith('+'))
    s.remove(0, 1);
  s = s.trimmed();

  // character cells without the decimal point, and where the point goes
  QString cells;
  int dpAfter = -1;
  for (QChar ch : s)
  {
    if (ch == '.' || ch == ',')
      dpAfter = cells.size() - 1;
    else
      cells += ch;
  }
  while (cells.size() < minDigits)
  {
    cells.prepend(' ');
    if (dpAfter >= 0)
      dpAfter++;
  }
  const int n = cells.size();

  // leading zeros stay dark, as on the meter: "000.00" shows "  0.00"; the
  // zero before the decimal point (or the last digit) stays lit. Only for
  // numbers - "0L" keeps its zero.
  bool number = true;
  for (QChar ch : cells)
    number = number && (ch.isDigit() || ch == ' ');
  if (number)
  {
    const int lastInteger = dpAfter >= 0 ? dpAfter : n - 1;
    for (int i = 0; i < lastInteger && (cells[i] == '0' || cells[i] == ' '); ++i)
      cells[i] = ' ';
  }

  // the minus always in the sign cell, in front of the field, wherever the
  // first digit is - as on the meter
  double x = origin.x();
  drawDigit(p, QPointF(x, origin.y()), h, (negative && lit) ? G : 0);
  x += adv;

  // the decimal point takes its slot after the cell it follows; the field is
  // sized for exactly one point, so a value without one leaves the slot empty
  int dpSlot = dpAfter >= 0 ? dpAfter : n - 1;
  for (int i = 0; i < n; ++i)
  {
    const int mask = lit ? segmentsFor(cells[i]) : 0;
    drawDigit(p, QPointF(x, origin.y()), h, mask);
    x += adv;
    if (i == dpSlot)
    {
      const double r = h * kThick * 0.45;
      p.setPen(Qt::NoPen);
      p.setBrush((dpAfter >= 0 && lit) ? m_segment : ghost());
      p.drawEllipse(QPointF(x - adv * 0.17 - h * kSlant * 0.5 + dpW * 0.35, origin.y() + h - r), r, r);
      x += dpW;
    }
  }
  return fieldW;
}

double LcdWidget::drawUnit(QPainter &p, const QPointF &origin, double h, const QString &unit) const
{
  if (unit.isEmpty())
    return 0.0;
  const QString text = SiPrefix::displayText(unit);

  const QFont f = sansFont(h * 0.62);
  p.setFont(f);
  p.setPen(m_segment);
  const QFontMetricsF fm(f);
  const double w = fm.horizontalAdvance(text);
  p.drawText(QRectF(origin.x(), origin.y(), w + 2, h), Qt::AlignLeft | Qt::AlignBottom, text);
  return w + 2;
}

namespace
{
// the diode annunciator is drawn, not typed: the usual schematic symbol
// (triangle, bar, a line through) as on a meter's LCD
const QString kDiodeFlag = QStringLiteral("\x01diode");
double diodeWidth(double fontPx) { return fontPx * 1.7; }
// HOLD is drawn too: a filled square with an H cut out, the symbol of the
// old hold.xpm and of many meters' LCDs
const QString kHoldFlag = QStringLiteral("\x01hold");
double holdWidth(double fontPx) { return fontPx * 0.95; }
// the gap after annunciator @p t when @p next follows: "AC+DC" stays together
double flagGap(const QString &t, const QString &next, double gap)
{
  // "+" between AC and DC, "P" (peak) right before MAX and MIN
  return t == QLatin1String("+") || next == QLatin1String("+") || t == QLatin1String("P") ? gap * 0.25 : gap;
}
double flagWidth(const QString &t, const QFontMetricsF &fm, double fontPx)
{
  if (t == kDiodeFlag)
    return diodeWidth(fontPx);
  if (t == kHoldFlag)
    return holdWidth(fontPx);
  return fm.horizontalAdvance(t);
}
}

void LcdWidget::drawAnnunciator(QPainter &p, const QRectF &r, const QString &text, bool on, double fontPx) const
{
  const QColor c = on ? m_segment : ghost();
  if (text == kDiodeFlag)
  {
    const double h = fontPx * 0.72;                 // symbol height
    const double w = diodeWidth(fontPx);
    const double cy = r.center().y();
    const double x0 = r.left();
    const double x1 = x0 + w;
    const double lineW = qMax(1.0, fontPx * 0.09);
    const double triL = x0 + w * 0.25, triR = x0 + w * 0.68;
    p.save();
    p.setRenderHint(QPainter::Antialiasing, true);
    p.setPen(QPen(c, lineW, Qt::SolidLine, Qt::FlatCap, Qt::MiterJoin));
    p.setBrush(c);
    p.drawLine(QPointF(x0, cy), QPointF(triL, cy));   // anode lead
    QPainterPath tri;
    tri.moveTo(triL, cy - h / 2);
    tri.lineTo(triR, cy);
    tri.lineTo(triL, cy + h / 2);
    tri.closeSubpath();
    p.drawPath(tri);
    p.drawLine(QPointF(triR, cy - h / 2), QPointF(triR, cy + h / 2));   // cathode bar
    p.drawLine(QPointF(triR, cy), QPointF(x1, cy));   // cathode lead
    p.restore();
    return;
  }
  if (text == kHoldFlag)
  {
    // the square, the H as a hole in it (14 x 14 in the old XPM: posts 3
    // wide, inside 2..11, the bar at rows 6 and 7)
    const double s = holdWidth(fontPx);
    const QRectF sq(r.left(), r.center().y() - s / 2, s, s);
    const double u = s / 14.0;
    QPainterPath square;
    square.addRoundedRect(sq, 2 * u, 2 * u);
    QPainterPath h;
    h.addRect(QRectF(sq.left() + 3 * u, sq.top() + 2 * u, 2 * u, 10 * u));
    h.addRect(QRectF(sq.left() + 9 * u, sq.top() + 2 * u, 2 * u, 10 * u));
    h.addRect(QRectF(sq.left() + 5 * u, sq.top() + 6 * u, 4 * u, 2 * u));
    p.save();
    p.setRenderHint(QPainter::Antialiasing, true);
    p.fillPath(square.subtracted(h.simplified()), c);
    p.restore();
    return;
  }
  p.setFont(sansFont(fontPx));
  p.setPen(c);
  p.drawText(r, Qt::AlignLeft | Qt::AlignVCenter, text);
}

// ---------------------------------------------------------------- layout

LcdWidget::Layout LcdWidget::layout() const
{
  Layout l;
  const double widen = m_limitOn ? kLimitWiden : 1.0;
  l.bezel = PanelFrame::panelRect(QRectF(rect()).adjusted(1, 1, -1, -1), kMinAspect * widen, kMaxAspect * widen);
  l.face = PanelFrame::faceRect(l.bezel, !m_caption.isEmpty());

  const double fh = l.face.height();
  const double fw = l.face.width();
  const double mx = fw * 0.04;
  const double my = fh * 0.06;
  const QRectF inner = l.face.adjusted(mx, my, -mx, -my);

  // row weights; the main row takes what is left
  const double flagsH = inner.height() * 0.14;
  const double minMaxH = m_showMinMax ? inner.height() * 0.20 : 0.0;
  const double barH = m_showBar ? inner.height() * 0.15 : 0.0;
  const double extraH = smallLineValues() > 0 ? inner.height() * 0.20 : 0.0;
  const double mainH = inner.height() - flagsH - minMaxH - barH - extraH;

  double y = inner.top();
  l.flags = QRectF(inner.left(), y, inner.width(), flagsH);
  y += flagsH;
  l.main = QRectF(inner.left(), y, inner.width(), mainH);
  // the limit test's column takes what the panel grew by
  if (m_limitOn)
  {
    const double vw = inner.width() * (1 - 1 / kLimitWiden);
    l.verdict = QRectF(inner.right() - vw, y, vw, mainH);
    l.main.setWidth(inner.width() - vw);
  }
  y += mainH;
  l.minMax = QRectF(inner.left(), y, inner.width(), minMaxH);
  y += minMaxH;
  l.extra = QRectF(inner.left(), y, inner.width(), extraH);
  y += extraH;
  l.bar = QRectF(inner.left(), y, inner.width(), barH);

  // digit height: fits the row, and the number plus unit fit the width
  const int digits = numDigits();
  double h = mainH * 0.78;
  const double unitFactor = 0.62 * 2.6;     // ~ width of "mV" at 0.62 h
  const double needed = numberWidth(1.0, digits) + 0.3 + unitFactor;
  if (h * needed > l.main.width())
    h = l.main.width() / needed;
  l.digitH = h;

  double sh = qMin(minMaxH > 0 ? minMaxH * 0.7 : extraH * 0.7, h * 0.5);
  if (sh <= 0)
    sh = h * 0.4;
  // the MIN and MAX blocks side by side, with a gap of one small digit,
  // must fit the width too: on a narrow panel the small glyphs shrink
  if (m_showMinMax)
    for (int i = 0; i < 20 && 2.0 * minMaxBlockWidth(sh) + sh > inner.width() && sh > 4; ++i)
      sh = qMax(4.0, sh * inner.width() / (2.0 * minMaxBlockWidth(sh) + sh) * 0.97);
  l.smallH = sh;
  l.minMaxBlockW = minMaxBlockWidth(sh);

  // annunciators: AUTO, MAX, MIN, AVG left, AC + DC, diode, continuity and HOLD right, all
  // present as ghosts - shrink the font until the row holds them
  // (font sizes are whole pixels, so one proportional step may still be a
  // little too wide - repeat until it fits)
  double fpx = flagsH * 0.75;
  for (int i = 0; i < 20 && flagsWidth(fpx) > inner.width() && fpx > 4; ++i)
    fpx = qMax(4.0, fpx * inner.width() / flagsWidth(fpx) * 0.97);
  l.flagsPx = fpx;
  return l;
}

namespace
{
const QStringList &leftFlags()
{
  // manual range is AUTO off, as on the meters; MAX, MIN and AVG: the
  // meter shows its own maximum, minimum or average (its MAX/MIN keys); P
  // before them: the peak maximum or minimum (UT202BT)
  static const QStringList l = { LcdWidget::tr("AUTO"), QStringLiteral("P"), LcdWidget::tr("MAX"), QStringLiteral("P"),
                                 LcdWidget::tr("MIN"), LcdWidget::tr("AVG") };
  return l;
}
const QStringList &rightFlags()
{
  // "+" between AC and DC lights for AC+DC
  static const QStringList l = { QStringLiteral("AC"), QStringLiteral("+"), QStringLiteral("DC"), kDiodeFlag,
                                 QStringLiteral("●))"), kHoldFlag };
  return l;
}
}

double LcdWidget::flagsWidth(double fontPx) const
{
  const QFontMetricsF fm(sansFont(fontPx));
  const double gap = fontPx * 0.9;
  double w = gap;   // between the two groups at least one gap
  const QStringList all = leftFlags() + rightFlags();
  for (int i = 0; i < all.size(); ++i)
    w += flagWidth(all[i], fm, fontPx) + 2 + (i + 1 < all.size() ? flagGap(all[i], all[i + 1], gap) : 0);
  return w;
}

double LcdWidget::minMaxBlockWidth(double h) const
{
  const QFontMetricsF fm(sansFont(h * 0.7));
  const double labelW = fm.horizontalAdvance(tr("MAX")) + h * 0.3;
  // the unit at 0.62 h: three characters ("MΩ", "µF" with a margin)
  const QFontMetricsF um(sansFont(h * 0.62));
  const double unitW = um.horizontalAdvance(QStringLiteral("MΩ")) * 1.3 + 2;
  return labelW + numberWidth(h, numDigits()) + h * 0.25 + unitW;
}

void LcdWidget::resizeEvent(QResizeEvent *)
{
  m_staticDirty = true;
}

// --------------------------------------------------------------- drawing

void LcdWidget::drawFlags(QPainter &p, const Layout &l) const
{
  const double fontPx = l.flagsPx;   // sized by layout() to fit the width
  const QFontMetricsF fm(sansFont(fontPx));
  const double gap = fontPx * 0.9;
  const int main = idAt(0);
  const quint32 flags = m_flags[main];

  struct Flag { QString text; bool on; quint16 has; };
  const QStringList &lt = leftFlags();
  const QStringList &rt = rightFlags();
  const bool ac = flags & SampleFlag::AC, dc = flags & SampleFlag::DC;
  const bool peak = flags & SampleFlag::Peak;
  const Flag all[] = {
    { lt[0], m_auto, AnnAuto },
    { lt[1], peak && (flags & SampleFlag::Max), AnnPeak },
    { lt[2], bool(flags & SampleFlag::Max), AnnMax },
    { lt[3], peak && (flags & SampleFlag::Min), AnnPeak },
    { lt[4], bool(flags & SampleFlag::Min), AnnMin },
    { lt[5], bool(flags & SampleFlag::Avg), AnnAvg },
    { rt[0], ac, AnnAc },
    { rt[1], ac && dc, AnnAc | AnnDc },
    { rt[2], dc, AnnDc },
    { rt[3], bool(flags & SampleFlag::Diode), AnnDiode },
    { rt[4], m_quantity[main] == Quantity::Continuity, AnnContinuity },
    { rt[5], m_hold, AnnHold },
  };
  // the ones the meter's display has, and any a reading lights; they close
  // up within their group
  QList<Flag> left, right;
  for (int i = 0; i < int(std::size(all)); ++i)
    if (all[i].on || (m_look.annunciators & all[i].has) == all[i].has)
      (i < int(lt.size()) ? left : right) << all[i];

  double x = l.flags.left();
  auto width = [&](const QString &t) { return flagWidth(t, fm, fontPx); };
  for (int i = 0; i < int(left.size()); ++i)
  {
    const double w = width(left[i].text);
    drawAnnunciator(p, QRectF(x, l.flags.top(), w + 2, l.flags.height()), left[i].text, left[i].on, fontPx);
    x += w + flagGap(left[i].text, QString(), gap);
  }
  double xr = l.flags.right();
  for (int i = int(right.size()) - 1; i >= 0; --i)
  {
    const double w = width(right[i].text);
    xr -= w;
    drawAnnunciator(p, QRectF(xr, l.flags.top(), w + 2, l.flags.height()), right[i].text, right[i].on, fontPx);
    if (i > 0)
      xr -= flagGap(right[i - 1].text, right[i].text, gap);
  }
}

void LcdWidget::drawMain(QPainter &p, const Layout &l) const
{
  const double h = l.digitH;
  const int digits = numDigits();
  const double numW = numberWidth(h, digits);
  const double y = l.main.top() + (l.main.height() - h) / 2.0;
  // no number yet: the words, not a row of ghost digits that looks broken
  if (!m_notice.isEmpty())
  {
    QFont font = sansFont(qMin(h * 0.36, l.main.height() * 0.3), false);
    const double fit = l.main.width() * 0.92 / QFontMetricsF(font).horizontalAdvance(m_notice);
    if (fit < 1)
      font.setPixelSize(qMax(6, int(font.pixelSize() * fit)));
    p.save();
    p.setOpacity(1);   // not faded as a stale value: it is no value
    p.setFont(font);
    p.setPen(m_segment);
    p.drawText(l.main, Qt::AlignCenter, m_notice);
    p.restore();
    return;
  }

  // number left, unit right after it; the whole group is centred
  const int id = idAt(0);
  const QString unit = m_unit[id];
  const QFontMetricsF fm(sansFont(h * 0.62));
  const double unitW = unit.isEmpty() ? 0.0 : fm.horizontalAdvance(unit) + 2;
  const double groupW = numW + h * 0.3 + unitW;
  double x = l.main.left() + qMax(0.0, (l.main.width() - groupW) / 2.0);

  drawNumber(p, QPointF(x, y), h, m_value[id], digits, !m_value[id].isEmpty());
  x += numW + h * 0.3;
  drawUnit(p, QPointF(x, y), h, unit);
}

// PASS on green or FAIL on red, HI and LO below it as annunciators, the
// deviation under them; without a verdict a ghost of PASS
void LcdWidget::drawVerdict(QPainter &p, const Layout &l) const
{
  if (!m_limitOn)
    return;
  const QRectF &col = l.verdict;
  const double gap = col.width() * 0.08;
  const double w = col.width() - gap;
  const double h = qMin(col.height() * 0.4, w * 0.42);
  // badge, HI/LO and the deviation as one block, centred on the digits
  const QRectF badge(col.left() + gap, col.center().y() - h * 2.2 / 2, w, h);
  const bool fail = m_verdict == LimitTest::High || m_verdict == LimitTest::Low;
  p.save();
  p.setRenderHint(QPainter::Antialiasing, true);
  if (m_verdict == LimitTest::NoVerdict)
  {
    p.setPen(QPen(ghost(), qMax(1.0, h * 0.05)));
    p.setBrush(Qt::NoBrush);
  }
  else
  {
    // on a face in the verdict's (or an alarm's) colour a red badge would
    // vanish: then it is drawn in the segments' colour, the text in the face's
    p.setPen(Qt::NoPen);
    p.setBrush(m_alert.isValid() ? m_segment : fail ? QColor(0xc6, 0x28, 0x28) : QColor(0x1f, 0x8f, 0x3a));
  }
  p.drawRoundedRect(badge, h * 0.14, h * 0.14);
  p.setFont(sansFont(h * 0.58));
  p.setPen(m_verdict == LimitTest::NoVerdict ? ghost() : m_alert.isValid() ? m_face : QColor(Qt::white));
  p.drawText(badge, Qt::AlignCenter, fail ? tr("FAIL") : tr("PASS"));
  p.restore();

  // HI and LO side by side, centred under the badge
  const double fpx = h * 0.42;
  const QFontMetricsF fm(sansFont(fpx));
  const QString hi = tr("HI"), lo = tr("LO");
  const double pair = fm.horizontalAdvance(hi) + fpx * 0.9 + fm.horizontalAdvance(lo);
  const double y = badge.bottom() + h * 0.12;
  double x = badge.center().x() - pair / 2;
  drawAnnunciator(p, QRectF(x, y, fm.horizontalAdvance(hi) + 2, h * 0.5), hi, m_verdict == LimitTest::High, fpx);
  x += fm.horizontalAdvance(hi) + fpx * 0.9;
  drawAnnunciator(p, QRectF(x, y, fm.horizontalAdvance(lo) + 2, h * 0.5), lo, m_verdict == LimitTest::Low, fpx);

  if (!std::isnan(m_deviation) && m_verdict != LimitTest::NoVerdict)
  {
    // "+0.6 %", one decimal below 10 %, none above
    const double d = m_deviation;
    const QString text = QString("%1%2 %").arg(d >= 0 ? "+" : "\u2212").arg(qAbs(d), 0, 'f', qAbs(d) < 10 ? 1 : 0);
    p.setFont(sansFont(fpx));
    p.setPen(m_segment);
    p.drawText(QRectF(badge.left(), y + h * 0.5, badge.width(), h * 0.55), Qt::AlignCenter, text);
  }
}

void LcdWidget::drawMinMax(QPainter &p, const Layout &l) const
{
  if (!m_showMinMax)
    return;
  const double h = l.smallH;
  const int digits = numDigits();
  const double y = l.minMax.top() + (l.minMax.height() - h) / 2.0;
  const double fontPx = h * 0.7;
  const QFontMetricsF fm(sansFont(fontPx));
  const double labelW = fm.horizontalAdvance(tr("MAX")) + h * 0.3;

  auto block = [&](double x, const QString &label, const QString &value, const QString &unit)
  {
    p.setFont(sansFont(fontPx));
    p.setPen(value.isEmpty() ? ghost() : m_segment);
    p.drawText(QRectF(x, y, labelW, h), Qt::AlignLeft | Qt::AlignVCenter, label);
    x += labelW;
    x += drawNumber(p, QPointF(x, y), h, value, digits, !value.isEmpty());
    x += h * 0.25;
    drawUnit(p, QPointF(x, y), h, unit);
  };

  // MAX starts right after MIN's block (plus a gap), never under it; what is
  // left of the row stays free on the right
  block(l.minMax.left(), tr("MIN"), m_minValue, m_minUnit);
  block(l.minMax.left() + qMax(l.minMaxBlockW + h, l.minMax.width() / 2.0), tr("MAX"), m_maxValue, m_maxUnit);
}

void LcdWidget::drawExtra(QPainter &p, const Layout &l) const
{
  const QList<int> ids = secondaries();
  if (ids.isEmpty())
    return;
  const double h = l.smallH;
  const int digits = numDigits();
  const double y = l.extra.top() + (l.extra.height() - h) / 2.0;
  const double slot = l.extra.width() / ids.size();
  for (int i = 0; i < ids.size(); ++i)
  {
    double x = l.extra.left() + i * slot;
    const int id = ids[i];
    x += drawNumber(p, QPointF(x, y), h, m_value[id], digits, !m_value[id].isEmpty());
    x += h * 0.25;
    drawUnit(p, QPointF(x, y), h, m_unit[id]);
  }
}

// The bar graph mirrors the one on the meter's own LCD: the reading's
// digits over the display count, in blocks with a tick scale above.
void LcdWidget::drawBar(QPainter &p, const Layout &l, bool staticPart) const
{
  // without a bar on the meter its room stays empty
  if (!m_showBar || !m_look.bar)
    return;

  const int digits = int(QString::number(m_counts).size());
  int divisions = int(m_counts / std::pow(10.0, digits - 2));
  if (divisions < 20 || divisions > 60)
    divisions = 50;

  const double tickH = l.bar.height() * 0.30;
  const double blockH = l.bar.height() * 0.42;
  const double fontPx = l.bar.height() * 0.32;
  const double x0 = l.bar.left();
  const double w = l.bar.width();
  const double step = w / divisions;
  const double tickTop = l.bar.top() + fontPx * 1.1;
  const double blockTop = l.bar.bottom() - blockH;

  if (staticPart)
  {
    p.setFont(sansFont(fontPx, false));
    for (int c = 0; c <= divisions; ++c)
    {
      const double x = x0 + c * step;
      const bool major = (c % 5) == 0;
      p.setPen(QPen(m_segment, major ? 1.5 : 1.0));
      p.drawLine(QPointF(x, tickTop + (major ? 0 : tickH * 0.5)), QPointF(x, tickTop + tickH));
      if (c % 10 == 0)
      {
        p.setPen(m_segment);
        p.drawText(QRectF(x - fontPx * 2, l.bar.top() - fontPx * 0.1, fontPx * 4, fontPx * 1.2), Qt::AlignCenter, QString::number(c));
      }
    }
    // ghost blocks
    p.setPen(Qt::NoPen);
    p.setBrush(ghost());
    for (int c = 0; c < divisions; ++c)
      p.drawRect(QRectF(x0 + c * step + 1, blockTop, step - 2, blockH));
    return;
  }

  const QString &shown = m_value[idAt(0)];
  if (!m_paintBar || shown.isEmpty())
    return;
  QString val;
  for (QChar ch : shown)
    if (ch.isDigit())
      val += ch;
  const double percent = qBound(0.0, val.toDouble() / m_counts, 1.0);
  const int lit = int(std::lround(percent * divisions));
  p.setPen(Qt::NoPen);
  p.setBrush(m_segment);
  for (int c = 0; c < lit; ++c)
    p.drawRect(QRectF(x0 + c * step + 1, blockTop, step - 2, blockH));
}

void LcdWidget::renderStatic()
{
  m_static = PanelFrame::staticPixmap(this);
  QPainter p(&m_static);
  PanelFrame::smooth(p);
  const Layout l = layout();

  PanelFrame::Colors c;
  c.bezelLight = kBezelLight;
  c.bezelDark = kBezelDark;
  c.face = m_face;
  c.faceGlow = m_face.lighter(112);
  PanelFrame::paint(p, l.bezel, c, l.face.center(), l.face.width() * 0.6, !m_caption.isEmpty());
  if (!m_caption.isEmpty())
    PanelFrame::paintPlate(p, l.bezel, m_caption, c);

  PanelFrame::clipToFace(p, l.face);
  drawBar(p, l, true);
  m_staticDirty = false;
}

void LcdWidget::setCaption(const QString &caption)
{
  if (caption == m_caption)
    return;
  m_caption = caption;
  m_staticDirty = true;
  update();
}

void LcdWidget::setNotice(const QString &notice)
{
  if (notice == m_notice)
    return;
  m_notice = notice;
  update();
}

void LcdWidget::setStale(bool stale)
{
  if (stale == m_stale)
    return;
  m_stale = stale;
  update();
}

void LcdWidget::paintEvent(QPaintEvent *)
{
  if (m_staticDirty || m_static.isNull())
    renderStatic();

  QPainter p(this);
  PanelFrame::smooth(p);
  p.drawPixmap(0, 0, m_static);

  const Layout l = layout();
  PanelFrame::clipToFace(p, l.face);
  // a stale value fades like an LCD losing its drive; the ghosts of the
  // static part stay as they are
  if (m_stale)
    p.setOpacity(PanelFrame::kStaleOpacity);
  drawFlags(p, l);
  drawMain(p, l);
  drawMinMax(p, l);
  drawExtra(p, l);
  drawVerdict(p, l);
  drawBar(p, l, false);
}

// ---------------------------------------------------------------- MeterController feed

void LcdWidget::showReading(const Reading &r)
{
  setHold(r.hold);
  if (r.range == "AUTO")
    setAuto(true);
  if (r.range == "MANU")
    setManu(true);
  // the bar is the big value's (a meter sends it with its main value only)
  if (r.id == idAt(0))
    setShowBar(r.showBar);
  setMode(r.id, r.flags, r.port.quantity);
  if (!r.hold)
  {
    setValue(r.id, r.text);
    setUnit(r.id, r.unit);
  }
  update();
}

void LcdWidget::showMinimum(double, const QString &text, const QString &unit)
{
  setMinUnit(unit);
  setMinValue(text);
  update();
}

void LcdWidget::showMaximum(double, const QString &text, const QString &unit)
{
  setMaxUnit(unit);
  setMaxValue(text);
  update();
}

void LcdWidget::clearMinMax()
{
  setMinValue("");
  setMaxValue("");
  setMinUnit("");
  setMaxUnit("");
  update();
}
