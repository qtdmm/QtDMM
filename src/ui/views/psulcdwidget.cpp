// Copyright (c) 2026 The QtDMM developers
// SPDX-License-Identifier: GPL-3.0-or-later
#include "ui/views/psulcdwidget.h"

#include "ui/panelframe.h"

#include <QFontMetricsF>
#include <QPainter>
#include <QTimer>

#include <algorithm>
#include <cmath>

namespace
{
// the annunciators after the channel's name; a power supply's front speaks
// these words in every language
const QString kOut = QStringLiteral("OUT");
const QString kCv = QStringLiteral("CV");
const QString kCc = QStringLiteral("CC");
const QString kExt = QStringLiteral("EXT");
const QString kSet = QStringLiteral("SET");

// the rows of a block, parts of its height
constexpr double kFlagsRow = 0.17, kURow = 0.43, kIRow = 0.24, kPRow = 0.16;
constexpr double kFlagsRowU = 0.25, kURowU = 0.75;   // the voltage alone

// the unit's size at a digit height of 1, as LcdPanel::drawUnit() draws it
constexpr double kUnitFont = 0.62;

QString format(double v, int decimals)
{
  return std::isnan(v) ? QString() : QString::number(v, 'f', decimals);
}
}

PsuLcdWidget::PsuLcdWidget(QWidget *parent) : LcdPanel(parent), m_blink(new QTimer(this))
{
  m_clock.start();
  m_blink->setInterval(250);
  connect(m_blink, &QTimer::timeout, this, &PsuLcdWidget::blink);
}

void PsuLcdWidget::setChannels(const QList<Channel> &channels)
{
  m_ch.clear();
  for (const Channel &c : channels.mid(0, kMaxChannels))
  {
    State s;
    s.spec = c;
    m_ch << s;
  }
  invalidateStatic();
  updateGeometry();
  update();
}

bool PsuLcdWidget::hasCurrentRows() const
{
  return std::any_of(m_ch.cbegin(), m_ch.cend(), [](const State &s) { return s.spec.current; });
}

QSize PsuLcdWidget::grid(int channels)
{
  return channels <= 1 ? QSize(1, 1) : channels == 2 ? QSize(2, 1) : QSize(2, 2);
}

// ---------------------------------------------------------------- state

void PsuLcdWidget::setValues(int ch, double u, double i, double p)
{
  if (!valid(ch))
    return;
  m_ch[ch].u = u;
  m_ch[ch].i = i;
  m_ch[ch].p = p;
  update();
}

void PsuLcdWidget::setOutput(int ch, bool on)
{
  if (valid(ch))
    m_ch[ch].out = on;
  update();
}

void PsuLcdWidget::setMode(int ch, Mode mode)
{
  if (valid(ch))
    m_ch[ch].mode = mode;
  update();
}

void PsuLcdWidget::setExternal(int ch, bool on)
{
  if (valid(ch))
    m_ch[ch].ext = on;
  update();
}

void PsuLcdWidget::showSetpoint(int ch, double u)
{
  if (!valid(ch))
    return;
  m_ch[ch].setU = u;
  m_ch[ch].setUntil = m_clock.elapsed() + m_hold;
  // SET lit at once on every turn, blinking from there
  m_blinkOn = true;
  m_blink->start();
  update();
}

bool PsuLcdWidget::showsSetpoint(int ch) const
{
  return valid(ch) && !std::isnan(m_ch[ch].setU) && m_clock.elapsed() < m_ch[ch].setUntil;
}

void PsuLcdWidget::blink()
{
  bool any = false;
  for (int ch = 0; ch < channelCount(); ++ch)
    any = any || showsSetpoint(ch);
  m_blinkOn = !m_blinkOn || !any;
  if (!any)
    m_blink->stop();   // the measured values again
  update();
}

void PsuLcdWidget::setChannelAlert(int ch, const QColor &color)
{
  if (valid(ch))
    m_ch[ch].alert = color;
  update();
}

void PsuLcdWidget::setChannelStale(int ch, bool stale)
{
  if (valid(ch))
    m_ch[ch].stale = stale;
  update();
}

void PsuLcdWidget::setChannelVerdict(int ch, bool on, LimitTest::Verdict verdict, double deviation)
{
  if (!valid(ch))
    return;
  m_ch[ch].limitOn = on;
  m_ch[ch].verdict = on ? verdict : LimitTest::NoVerdict;
  m_ch[ch].deviation = deviation;
  update();
}

QString PsuLcdWidget::text(int ch, int row) const
{
  if (!valid(ch))
    return QString();
  const State &s = m_ch[ch];
  switch (row)
  {
    case 0:
      return showsSetpoint(ch) ? format(s.setU, s.spec.uDecimals) : format(s.u, s.spec.uDecimals);
    case 1:
      return format(s.i, s.spec.iDecimals);
    case 2:
      if (!std::isnan(s.p))
        return format(s.p, s.spec.pDecimals);
      return std::isnan(s.u) || std::isnan(s.i) ? QString() : format(s.u * s.i, s.spec.pDecimals);
    default:
      return QString();
  }
}

// ---------------------------------------------------------------- layout

int PsuLcdWidget::digits(double max, int decimals)
{
  const int whole = max >= 1 ? int(QString::number(qint64(std::floor(max))).size()) : 1;
  return whole + qMax(0, decimals);
}

QRectF PsuLcdWidget::valueRect(const QRectF &row, double h, int digits, const QString &unit) const
{
  const QFontMetricsF fm(sansFont(h * kUnitFont));
  const double w = numberWidth(h, digits, false) + h * 0.3 + fm.horizontalAdvance(unit) + 2;
  return QRectF(row.right() - w, row.center().y() - h / 2, w, h);
}

double PsuLcdWidget::flagsWidth(double fontPx, bool current) const
{
  const QFontMetricsF fm(sansFont(fontPx));
  const double gap = fontPx * 0.8;
  // the widest name of the channels, on its label
  double label = 0;
  for (const State &s : m_ch)
    label = qMax(label, fm.horizontalAdvance(s.spec.label) + fontPx * 0.7);
  double w = label + gap;
  QStringList words = { kOut, kCv };
  if (current)
    words << kCc;
  words << kExt << kSet;
  for (const QString &t : words)
    w += fm.horizontalAdvance(t) + 2 + gap;
  return w;
}

QList<PsuLcdWidget::Block> PsuLcdWidget::layout() const
{
  QList<Block> blocks;
  const Frame f = frame();
  const QSize g = grid(channelCount());
  const bool current = hasCurrentRows();
  const double fw = f.face.width(), fh = f.face.height();
  const double gap = qMin(fw / g.width(), fh / g.height()) * 0.04;
  const QRectF inner = f.face.adjusted(fw * 0.02, fh * 0.03, -fw * 0.02, -fh * 0.03);
  const double cellW = (inner.width() - gap * (g.width() - 1)) / g.width();
  const double cellH = (inner.height() - gap * (g.height() - 1)) / g.height();

  for (int k = 0; k < channelCount(); ++k)
  {
    const State &s = m_ch[k];
    Block b;
    b.cell = QRectF(inner.left() + (k % g.width()) * (cellW + gap), inner.top() + (k / g.width()) * (cellH + gap),
                    cellW, cellH);
    const double pad = cellH * 0.05;
    const QRectF c = b.cell.adjusted(pad * 1.5, pad, -pad * 1.5, -pad);
    const double flagsH = c.height() * (current ? kFlagsRow : kFlagsRowU);
    const double uH = c.height() * (current ? kURow : kURowU);
    const double iH = current ? c.height() * kIRow : 0;
    const double pH = current ? c.height() * kPRow : 0;
    b.flags = QRectF(c.left(), c.top(), c.width(), flagsH);
    // the verdict's column right of the values, the annunciators above it
    double valuesW = c.width();
    if (s.limitOn)
    {
      const double vw = c.width() * 0.26;
      b.verdict = QRectF(c.right() - vw, c.top() + flagsH, vw, uH + iH + pH);
      valuesW -= vw + c.width() * 0.03;
    }
    b.u = QRectF(c.left(), c.top() + flagsH, valuesW, uH);
    if (current)
    {
      b.i = QRectF(c.left(), b.u.bottom(), valuesW, iH);
      b.p = QRectF(c.left(), b.i.bottom(), valuesW, pH);
    }

    // a digit height that fills the row and lets number and unit fit its width
    auto fit = [this](const QRectF &row, double share, int digits, const QString &unit)
    {
      if (row.isEmpty())
        return 0.0;
      double h = row.height() * share;
      for (int n = 0; n < 20 && valueRect(row, h, digits, unit).width() > row.width() && h > 3; ++n)
        h = qMax(3.0, h * row.width() / valueRect(row, h, digits, unit).width() * 0.98);
      return h;
    };
    const int uDigits = digits(s.spec.uMax, s.spec.uDecimals);
    const int iDigits = digits(s.spec.iMax, s.spec.iDecimals);
    const int pDigits = digits(s.spec.uMax * s.spec.iMax, s.spec.pDecimals);
    b.uH = fit(b.u, 0.8, uDigits, "V");
    b.iH = fit(b.i, 0.78, iDigits, "A");
    b.pH = fit(b.p, 0.78, pDigits, "W");
    // the rows below never larger than the one above
    b.iH = qMin(b.iH, b.uH * 0.62);
    b.pH = qMin(b.pH, b.iH * 0.75);
    b.uText = valueRect(b.u, b.uH, uDigits, "V");
    if (current)
    {
      b.iText = valueRect(b.i, b.iH, iDigits, "A");
      b.pText = valueRect(b.p, b.pH, pDigits, "W");
    }

    double fpx = flagsH * 0.62;
    for (int n = 0; n < 20 && flagsWidth(fpx, current) > b.flags.width() && fpx > 4; ++n)
      fpx = qMax(4.0, fpx * b.flags.width() / flagsWidth(fpx, current) * 0.97);
    b.flagsPx = fpx;
    blocks << b;
  }
  return blocks;
}

double PsuLcdWidget::minAspect() const
{
  const QSize g = grid(channelCount());
  return g.width() * (hasCurrentRows() ? kBlockMinAspect : kBlockMinAspectU) / g.height();
}

double PsuLcdWidget::maxAspect() const
{
  const QSize g = grid(channelCount());
  return g.width() * (hasCurrentRows() ? kBlockMaxAspect : kBlockMaxAspectU) / g.height();
}

QSize PsuLcdWidget::sizeHint() const
{
  const QSize g = grid(channelCount());
  const bool current = hasCurrentRows();
  const double blockH = current ? 190 : 120;
  const double blockW = blockH * (current ? (kBlockMinAspect + kBlockMaxAspect) : (kBlockMinAspectU + kBlockMaxAspectU)) / 2;
  // the bezel around the face
  return QSize(int(g.width() * blockW * 1.08), int(g.height() * blockH * 1.12));
}

QSize PsuLcdWidget::minimumSizeHint() const
{
  return sizeHint() / 2;
}

// --------------------------------------------------------------- drawing

void PsuLcdWidget::paintContent(QPainter &p)
{
  const QList<Block> blocks = layout();
  for (int k = 0; k < int(blocks.size()); ++k)
    drawBlock(p, blocks[k], m_ch[k]);
}

void PsuLcdWidget::drawBlock(QPainter &p, const Block &b, const State &s) const
{
  const int ch = int(&s - m_ch.constData());
  p.save();
  // a stale channel fades like the multimeter's LCD, on its own
  if (s.stale)
    p.setOpacity(p.opacity() * PanelFrame::kStaleOpacity);
  const double radius = b.cell.height() * 0.04;
  if (s.alert.isValid())
  {
    p.setPen(Qt::NoPen);
    p.setBrush(s.alert);
    p.drawRoundedRect(b.cell, radius, radius);
    setInk(s.alert, segmentsOn(s.alert));
  }
  else if (channelCount() > 1)
  {
    // the blocks apart, faintly, as printed on a front's glass
    p.setPen(QPen(ghost(), qMax(1.0, b.cell.height() * 0.008)));
    p.setBrush(Qt::NoBrush);
    p.drawRoundedRect(b.cell, radius, radius);
  }

  drawFlags(p, b, s);
  drawValue(p, b.uText, b.uH, text(ch, 0), digits(s.spec.uMax, s.spec.uDecimals), "V");
  if (!b.i.isEmpty())
  {
    drawValue(p, b.iText, b.iH, text(ch, 1), digits(s.spec.iMax, s.spec.iDecimals), "A");
    drawValue(p, b.pText, b.pH, text(ch, 2), digits(s.spec.uMax * s.spec.iMax, s.spec.pDecimals), "W");
  }
  if (s.limitOn)
    drawVerdict(p, b.verdict, s.verdict, s.deviation);
  resetInk();
  p.restore();
}

void PsuLcdWidget::drawValue(QPainter &p, const QRectF &area, double h, const QString &text, int digits,
                             const QString &unit) const
{
  const double w = drawNumber(p, area.topLeft(), h, text, digits, !text.isEmpty(), false);
  drawUnit(p, QPointF(area.left() + w + h * 0.3, area.top()), h, unit);
}

void PsuLcdWidget::drawFlags(QPainter &p, const Block &b, const State &s) const
{
  const int ch = int(&s - m_ch.constData());
  const double fontPx = b.flagsPx;
  const QFontMetricsF fm(sansFont(fontPx));
  const double gap = fontPx * 0.8;
  const QRectF &r = b.flags;

  // the name on its colour, lettered light or dark to read on it; on any
  // LCD colour the label keeps its own background
  const QColor label = s.spec.color.isValid() ? s.spec.color : ink();
  const double labelW = fm.horizontalAdvance(s.spec.label) + fontPx * 0.7;
  const QRectF box(r.left(), r.center().y() - fontPx * 0.68, labelW, fontPx * 1.36);
  p.save();
  p.setPen(Qt::NoPen);
  p.setBrush(label);
  p.drawRoundedRect(box, fontPx * 0.2, fontPx * 0.2);
  p.setFont(sansFont(fontPx));
  p.setPen(segmentsOn(label));
  p.drawText(box, Qt::AlignCenter, s.spec.label);
  p.restore();

  double x = box.right() + gap;
  auto flag = [&](const QString &t, bool on)
  {
    const double w = fm.horizontalAdvance(t);
    drawAnnunciator(p, QRectF(x, r.top(), w + 2, r.height()), t, on, fontPx);
    x += w + 2 + gap;
  };
  flag(kOut, s.out);
  flag(kCv, s.mode == Mode::CV);
  if (!b.i.isEmpty())
    flag(kCc, s.mode == Mode::CC);
  flag(kExt, s.ext);
  const double setW = fm.horizontalAdvance(kSet);
  drawAnnunciator(p, QRectF(r.right() - setW - 2, r.top(), setW + 2, r.height()), kSet,
                  showsSetpoint(ch) && m_blinkOn, fontPx);
}
