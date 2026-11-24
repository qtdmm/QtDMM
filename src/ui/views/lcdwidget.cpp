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
// SPDX-License-Identifier: GPL-3.0-or-later
//======================================================================

#include "ui/views/lcdwidget.h"
#include "core/reading.h"

#include <QPainter>
#include <QFontMetricsF>
#include <cmath>

LcdWidget::LcdWidget(QWidget *parent) : LcdPanel(parent)
{
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
  invalidateStatic();
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
  invalidateStatic();
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
void LcdWidget::setBattery(Battery battery) { m_battery = battery; }

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
    case DeviceType::Tachometer:
    case DeviceType::Environment:
      return { false, AnnHold | AnnMax | AnnMin | AnnAvg };
    case DeviceType::Microcontroller:   // plain numbers, no meter's annunciators
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
  invalidateStatic();
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
  invalidateStatic();
  updateGeometry();
  update();
}

void LcdWidget::setLimitTest(bool on)
{
  if (on == m_limitOn)
    return;
  m_limitOn = on;
  m_verdict = LimitTest::NoVerdict;
  invalidateStatic();
  updateGeometry();
  update();
}

void LcdWidget::setVerdict(LimitTest::Verdict verdict, double deviation)
{
  m_verdict = verdict;
  m_deviation = deviation;
  update();
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
  invalidateStatic();
}

namespace
{
// the gap after annunciator @p t when @p next follows: "AC+DC" stays together
double flagGap(const QString &t, const QString &next, double gap)
{
  // "+" between AC and DC, "P" (peak) right before MAX and MIN
  return t == QLatin1String("+") || next == QLatin1String("+") || t == QLatin1String("P") ? gap * 0.25 : gap;
}
}

// ---------------------------------------------------------------- layout

LcdWidget::Layout LcdWidget::layout() const
{
  Layout l;
  const Frame f = frame();
  l.bezel = f.bezel;
  l.face = f.face;

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
  // before them: the peak maximum or minimum (UT202BT); the battery last,
  // where it comes and goes without moving the others
  static const QStringList l = { LcdWidget::tr("AUTO"), QStringLiteral("P"), LcdWidget::tr("MAX"), QStringLiteral("P"),
                                 LcdWidget::tr("MIN"), LcdWidget::tr("AVG"), LcdPanel::kBatteryFlag };
  return l;
}
const QStringList &rightFlags()
{
  // "+" between AC and DC lights for AC+DC
  static const QStringList l = { QStringLiteral("AC"), QStringLiteral("+"), QStringLiteral("DC"), LcdPanel::kDiodeFlag,
                                 QStringLiteral("●))"), LcdPanel::kHoldFlag };
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
    { lt[6], m_battery != Battery::Unknown, AnnBattery },
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

  const int bars = m_battery == Battery::Full ? 3 : m_battery == Battery::Mid ? 2 : m_battery == Battery::Low ? 1 : 0;
  double x = l.flags.left();
  auto width = [&](const QString &t) { return flagWidth(t, fm, fontPx); };
  for (int i = 0; i < int(left.size()); ++i)
  {
    const double w = width(left[i].text);
    drawAnnunciator(p, QRectF(x, l.flags.top(), w + 2, l.flags.height()), left[i].text, left[i].on, fontPx, bars);
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
    p.setPen(segmentColor());
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
    p.setPen(value.isEmpty() ? ghost() : segmentColor());
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
      p.setPen(QPen(segmentColor(), major ? 1.5 : 1.0));
      p.drawLine(QPointF(x, tickTop + (major ? 0 : tickH * 0.5)), QPointF(x, tickTop + tickH));
      if (c % 10 == 0)
      {
        p.setPen(segmentColor());
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
  p.setBrush(segmentColor());
  for (int c = 0; c < lit; ++c)
    p.drawRect(QRectF(x0 + c * step + 1, blockTop, step - 2, blockH));
}

void LcdWidget::setNotice(const QString &notice)
{
  if (notice == m_notice)
    return;
  m_notice = notice;
  update();
}

double LcdWidget::minAspect() const
{
  return m_limitOn ? kMinAspect * kLimitWiden : kMinAspect;
}

double LcdWidget::maxAspect() const
{
  return m_limitOn ? kMaxAspect * kLimitWiden : kMaxAspect;
}

void LcdWidget::paintStatic(QPainter &p)
{
  drawBar(p, layout(), true);
}

void LcdWidget::paintContent(QPainter &p)
{
  const Layout l = layout();
  drawFlags(p, l);
  drawMain(p, l);
  drawMinMax(p, l);
  drawExtra(p, l);
  if (m_limitOn)
    drawVerdict(p, l.verdict, m_verdict, m_deviation);
  drawBar(p, l, false);
}

// ---------------------------------------------------------------- MeterController feed

void LcdWidget::showReading(const Reading &r)
{
  setHold(r.hold);
  setBattery(r.battery);
  if (r.range == "AUTO")
    setAuto(true);
  if (r.range == "MANU")
    setManu(true);
  // the bar is the big value's (a meter sends it with its main value only)
  if (r.id == idAt(0))
    setShowBar(r.showBar);
  // a value beyond those of the model: a microcontroller's lines bring
  // more, as they come
  if (r.id >= m_numValues && r.id < kMaxValues)
    setValueCount(r.id + 1);
  setMode(r.id, r.flags, r.port.quantity);
  // what the meter's display shows, held or not: a held value is the same
  // again, an auto-hold (OpenOSC) takes the next stable one while HOLD stays
  setValue(r.id, r.text);
  setUnit(r.id, r.unit);
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
