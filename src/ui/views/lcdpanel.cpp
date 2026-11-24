// Copyright (c) 2002 Matthias Toussaint
// Copyright (c) 2026 The QtDMM developers
// SPDX-License-Identifier: GPL-3.0-or-later
#include "ui/views/lcdpanel.h"

#include "core/siprefix.h"
#include "ui/panelframe.h"

#include <QFontMetricsF>
#include <QPainter>
#include <QPainterPath>
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

double diodeWidth(double fontPx) { return fontPx * 1.7; }
double holdWidth(double fontPx) { return fontPx * 0.95; }
double batteryWidth(double fontPx) { return fontPx * 1.5; }
}

LcdPanel::LcdPanel(QWidget *parent)
  : QWidget(parent)
  , m_face(0xda, 0xdc, 0x77)
  , m_segment(0x16, 0x1e, 0x14)
{
  setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
}

QFont LcdPanel::sansFont(double px, bool bold)
{
  QFont f;
  f.setPixelSize(qMax(5, int(std::lround(px))));
  f.setBold(bold);
  return f;
}

// ---------------------------------------------------------------- colours

void LcdPanel::setFaceColor(const QColor &c)
{
  if (!c.isValid())
    return;
  m_customFace = c;
  applyVariant();
}

void LcdPanel::setLcdVariant(LcdVariant variant)
{
  m_variant = variant;
  applyVariant();
}

void LcdPanel::setAlertColor(const QColor &color)
{
  if (color == m_alert)
    return;
  m_alert = color;
  applyVariant();
}

QColor LcdPanel::classicFace()
{
  return QColor(0xda, 0xdc, 0x77);
}

// Face and segments; the glow and the ghost segments follow from them.
void LcdPanel::applyVariant()
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
    m_face = m_alert;
    m_segment = segmentsOn(m_alert);
  }
  m_staticDirty = true;
  update();
}

QColor LcdPanel::segmentsOn(const QColor &face)
{
  // light or dark segments, whichever has more contrast on the colour
  // (WCAG relative luminance: on the default alarm red light ones)
  auto luminance = [](const QColor &c)
  {
    auto lin = [](double v) { return v <= 0.03928 ? v / 12.92 : std::pow((v + 0.055) / 1.055, 2.4); };
    return 0.2126 * lin(c.redF()) + 0.7152 * lin(c.greenF()) + 0.0722 * lin(c.blueF());
  };
  const QColor light(0xf4, 0xf4, 0xf0), dark(0x16, 0x1e, 0x14);
  const double l = luminance(face);
  const double onLight = (luminance(light) + 0.05) / (l + 0.05);
  const double onDark = (l + 0.05) / (luminance(dark) + 0.05);
  return onLight >= onDark ? light : dark;
}

void LcdPanel::setInk(const QColor &face, const QColor &segment) const
{
  m_inkFace = face;
  m_inkSegment = segment;
}

void LcdPanel::resetInk() const
{
  m_inkFace = QColor();
  m_inkSegment = QColor();
}

QString LcdPanel::lcdVariantName(LcdVariant variant)
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

LcdPanel::LcdVariant LcdPanel::lcdVariantFromName(const QString &name)
{
  for (LcdVariant v : { BacklightBlue, Amber, HighContrast, Custom })
    if (name == lcdVariantName(v))
      return v;
  return Classic;
}

QColor LcdPanel::ghost() const
{
  QColor c = ink();
  // light segments on a dark face (backlight blue) stand out much more:
  // their ghosts must stay fainter than dark ghosts on a light face
  if (ink().lightness() > inkFace().lightness())
    c.setAlpha(18);
  else
    c.setAlpha(inkFace().lightness() < 128 ? 40 : 26);
  return c;
}

// ---------------------------------------------------------- segments

int LcdPanel::segmentsFor(QChar ch)
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

void LcdPanel::drawDigit(QPainter &p, const QPointF &origin, double h, int mask) const
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
    p.setBrush((mask & (1 << i)) ? ink() : ghost());
    p.drawPolygon(segs[i]);
  }
  p.restore();
}

double LcdPanel::numberWidth(double h, int digits, bool signCell)
{
  // sign cell + digits + one decimal point
  return (digits + (signCell ? 1 : 0)) * h * kAdvance + h * kDpW;
}

// Draws a reading right-aligned in a field of minDigits digits: leading
// cells stay ghosted, the sign has its own cell, the decimal point sits
// between cells. Returns the field width.
double LcdPanel::drawNumber(QPainter &p, const QPointF &origin, double h, const QString &text, int minDigits, bool lit,
                            bool signCell) const
{
  const double adv = h * kAdvance;
  const double dpW = h * kDpW;
  const double fieldW = numberWidth(h, minDigits, signCell);

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
  if (signCell)
  {
    drawDigit(p, QPointF(x, origin.y()), h, (negative && lit) ? G : 0);
    x += adv;
  }

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
      p.setBrush((dpAfter >= 0 && lit) ? ink() : ghost());
      p.drawEllipse(QPointF(x - adv * 0.17 - h * kSlant * 0.5 + dpW * 0.35, origin.y() + h - r), r, r);
      x += dpW;
    }
  }
  return fieldW;
}

double LcdPanel::drawUnit(QPainter &p, const QPointF &origin, double h, const QString &unit) const
{
  if (unit.isEmpty())
    return 0.0;
  const QString text = SiPrefix::displayText(unit);

  const QFont f = sansFont(h * 0.62);
  p.setFont(f);
  p.setPen(ink());
  const QFontMetricsF fm(f);
  const double w = fm.horizontalAdvance(text);
  p.drawText(QRectF(origin.x(), origin.y(), w + 2, h), Qt::AlignLeft | Qt::AlignBottom, text);
  return w + 2;
}

// ----------------------------------------------------------- annunciators

double LcdPanel::flagWidth(const QString &t, const QFontMetricsF &fm, double fontPx)
{
  if (t == kDiodeFlag)
    return diodeWidth(fontPx);
  if (t == kHoldFlag)
    return holdWidth(fontPx);
  if (t == kBatteryFlag)
    return batteryWidth(fontPx);
  return fm.horizontalAdvance(t);
}

void LcdPanel::drawAnnunciator(QPainter &p, const QRectF &r, const QString &text, bool on, double fontPx,
                               int batteryBars) const
{
  const QColor c = on ? ink() : ghost();
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
  if (text == kBatteryFlag)
  {
    // the outline lit with the symbol, the bars up to the level; the
    // others as ghosts, as on a meter's LCD
    const double w = batteryWidth(fontPx);
    const double h = fontPx * 0.72;
    const double lineW = qMax(1.0, fontPx * 0.09);
    const QRectF body(r.left() + lineW / 2, r.center().y() - h / 2, w * 0.86 - lineW, h);
    const QRectF nub(body.right() + lineW / 2, r.center().y() - h * 0.22, w * 0.14 - lineW / 2, h * 0.44);
    p.save();
    p.setRenderHint(QPainter::Antialiasing, true);
    p.setPen(QPen(c, lineW));
    p.setBrush(Qt::NoBrush);
    p.drawRect(body);
    p.fillRect(nub, c);
    const QRectF in = body.adjusted(lineW * 1.5, lineW * 1.5, -lineW * 1.5, -lineW * 1.5);
    const double barGap = lineW;
    const double barW = (in.width() - 2 * barGap) / 3;
    for (int i = 0; i < 3; ++i)
      p.fillRect(QRectF(in.left() + i * (barW + barGap), in.top(), barW, in.height()), on && i < batteryBars ? ink() : ghost());
    p.restore();
    return;
  }
  p.setFont(sansFont(fontPx));
  p.setPen(c);
  p.drawText(r, Qt::AlignLeft | Qt::AlignVCenter, text);
}

// PASS on green or FAIL on red, HI and LO below it as annunciators, the
// deviation under them; without a verdict a ghost of PASS
void LcdPanel::drawVerdict(QPainter &p, const QRectF &col, LimitTest::Verdict verdict, double deviation) const
{
  const double gap = col.width() * 0.08;
  const double w = col.width() - gap;
  const double h = qMin(col.height() * 0.4, w * 0.42);
  // badge, HI/LO and the deviation as one block, centred on the digits
  const QRectF badge(col.left() + gap, col.center().y() - h * 2.2 / 2, w, h);
  const bool fail = verdict == LimitTest::High || verdict == LimitTest::Low;
  p.save();
  p.setRenderHint(QPainter::Antialiasing, true);
  if (verdict == LimitTest::NoVerdict)
  {
    p.setPen(QPen(ghost(), qMax(1.0, h * 0.05)));
    p.setBrush(Qt::NoBrush);
  }
  else
  {
    // on a face in the verdict's (or an alarm's) colour a red badge would
    // vanish: then it is drawn in the segments' colour, the text in the face's
    p.setPen(Qt::NoPen);
    p.setBrush(onAlert() ? ink() : fail ? QColor(0xc6, 0x28, 0x28) : QColor(0x1f, 0x8f, 0x3a));
  }
  p.drawRoundedRect(badge, h * 0.14, h * 0.14);
  p.setFont(sansFont(h * 0.58));
  p.setPen(verdict == LimitTest::NoVerdict ? ghost() : onAlert() ? inkFace() : QColor(Qt::white));
  p.drawText(badge, Qt::AlignCenter, fail ? tr("FAIL") : tr("PASS"));
  p.restore();

  // HI and LO side by side, centred under the badge
  const double fpx = h * 0.42;
  const QFontMetricsF fm(sansFont(fpx));
  const QString hi = tr("HI"), lo = tr("LO");
  const double pair = fm.horizontalAdvance(hi) + fpx * 0.9 + fm.horizontalAdvance(lo);
  const double y = badge.bottom() + h * 0.12;
  double x = badge.center().x() - pair / 2;
  drawAnnunciator(p, QRectF(x, y, fm.horizontalAdvance(hi) + 2, h * 0.5), hi, verdict == LimitTest::High, fpx);
  x += fm.horizontalAdvance(hi) + fpx * 0.9;
  drawAnnunciator(p, QRectF(x, y, fm.horizontalAdvance(lo) + 2, h * 0.5), lo, verdict == LimitTest::Low, fpx);

  if (!std::isnan(deviation) && verdict != LimitTest::NoVerdict)
  {
    // "+0.6 %", one decimal below 10 %, none above
    const double d = deviation;
    const QString text = QString("%1%2 %").arg(d >= 0 ? "+" : "\u2212").arg(qAbs(d), 0, 'f', qAbs(d) < 10 ? 1 : 0);
    p.setFont(sansFont(fpx));
    p.setPen(ink());
    p.drawText(QRectF(badge.left(), y + h * 0.5, badge.width(), h * 0.55), Qt::AlignCenter, text);
  }
}

// ---------------------------------------------------------------- housing

LcdPanel::Frame LcdPanel::frame() const
{
  Frame f;
  f.bezel = PanelFrame::panelRect(QRectF(rect()).adjusted(1, 1, -1, -1), minAspect(), maxAspect());
  f.face = PanelFrame::faceRect(f.bezel, !m_caption.isEmpty());
  return f;
}

void LcdPanel::renderStatic()
{
  m_static = PanelFrame::staticPixmap(this);
  QPainter p(&m_static);
  PanelFrame::smooth(p);
  const Frame l = frame();

  PanelFrame::Colors c;
  c.bezelLight = kBezelLight;
  c.bezelDark = kBezelDark;
  c.face = m_face;
  c.faceGlow = m_face.lighter(112);
  PanelFrame::paint(p, l.bezel, c, l.face.center(), l.face.width() * 0.6, !m_caption.isEmpty());
  if (!m_caption.isEmpty())
    PanelFrame::paintPlate(p, l.bezel, m_caption, c, m_plateReserve);

  PanelFrame::clipToFace(p, l.face);
  paintStatic(p);
  m_staticDirty = false;
}

void LcdPanel::setPlateReserve(double px)
{
  if (qFuzzyCompare(px + 1, m_plateReserve + 1))
    return;
  m_plateReserve = px;
  m_staticDirty = true;
  update();
}

QRectF LcdPanel::plateBand() const
{
  return m_caption.isEmpty() ? QRectF() : PanelFrame::plateBand(frame().bezel);
}

QColor LcdPanel::plateInk(bool strong) const
{
  PanelFrame::Colors c;
  c.bezelLight = kBezelLight;
  c.bezelDark = kBezelDark;
  return PanelFrame::plateInk(c, strong);
}

void LcdPanel::setCaption(const QString &caption)
{
  if (caption == m_caption)
    return;
  m_caption = caption;
  m_staticDirty = true;
  update();
}

void LcdPanel::setStale(bool stale)
{
  if (stale == m_stale)
    return;
  m_stale = stale;
  update();
}

void LcdPanel::resizeEvent(QResizeEvent *)
{
  m_staticDirty = true;
}

void LcdPanel::paintEvent(QPaintEvent *)
{
  if (m_staticDirty || m_static.isNull())
    renderStatic();

  QPainter p(this);
  PanelFrame::smooth(p);
  p.drawPixmap(0, 0, m_static);

  PanelFrame::clipToFace(p, frame().face);
  // a stale value fades like an LCD losing its drive; the ghosts of the
  // static part stay as they are
  if (m_stale)
    p.setOpacity(PanelFrame::kStaleOpacity);
  paintContent(p);
}
