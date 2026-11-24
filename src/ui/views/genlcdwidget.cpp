// Copyright (c) 2026 The QtDMM developers
// SPDX-License-Identifier: GPL-3.0-or-later
#include "ui/views/genlcdwidget.h"

#include <QFontMetricsF>
#include <QPainter>
#include <QTimer>

#include <cmath>
#include <numbers>

namespace
{
// the annunciators; a generator's front speaks these words in every language
const QString kOut = QStringLiteral("OUT");
const QString kExt = QStringLiteral("EXT");
const QString kSet = QStringLiteral("SET");
const QString kErr = QStringLiteral("ERR");
const QString kDuty = QStringLiteral("DUTY");
const QString kSym = QStringLiteral("SYM");
const QStringList kShapeWords = { QStringLiteral("SINE"), QStringLiteral("SQUARE"), QStringLiteral("RAMP"),
                                  QStringLiteral("PULSE") };
constexpr int kArbitraryLetters = 8;   ///< of an arbitrary shape's name

// the rows of a block, parts of its height
constexpr double kFlagsRow = 0.18, kFreqRow = 0.42, kSmallRow = 0.24, kExtraRow = 0.16;
// the unit's size at a digit height of 1, as LcdPanel::drawUnit() draws it
constexpr double kUnitFont = 0.62;

QString format(double v, int decimals)
{
  return std::isnan(v) ? QString() : QString::number(v, 'f', decimals);
}
}

GenLcdWidget::GenLcdWidget(QWidget *parent) : LcdPanel(parent), m_blink(new QTimer(this))
{
  m_clock.start();
  m_blink->setInterval(250);
  connect(m_blink, &QTimer::timeout, this, &GenLcdWidget::blink);
}

void GenLcdWidget::setChannels(const QList<Channel> &channels)
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

// ---------------------------------------------------------------- state

void GenLcdWidget::setShape(int ch, Shape shape, const QString &arbitrary)
{
  if (!valid(ch))
    return;
  m_ch[ch].shape = shape;
  m_ch[ch].arbitrary = shape == Shape::Arbitrary ? arbitrary : QString();
  update();
}

void GenLcdWidget::setValues(int ch, double frequency, double amplitude, double offset, double duty, double symmetry)
{
  if (!valid(ch))
    return;
  State &s = m_ch[ch];
  s.f = frequency;
  s.amp = amplitude;
  s.offset = offset;
  s.duty = duty;
  s.symmetry = symmetry;
  update();
}

void GenLcdWidget::setOutput(int ch, bool on)
{
  if (valid(ch))
    m_ch[ch].out = on;
  update();
}

void GenLcdWidget::setExternal(int ch, bool on)
{
  if (valid(ch))
    m_ch[ch].ext = on;
  update();
}

void GenLcdWidget::markSet(int ch)
{
  if (!valid(ch))
    return;
  m_ch[ch].setUntil = m_clock.elapsed() + m_hold;
  // SET lit at once on every turn, blinking from there
  m_blinkOn = true;
  m_blink->start();
  update();
}

bool GenLcdWidget::setShown(int ch) const
{
  return valid(ch) && m_clock.elapsed() < m_ch[ch].setUntil;
}

void GenLcdWidget::blink()
{
  bool any = false;
  for (int ch = 0; ch < channelCount(); ++ch)
    any = any || setShown(ch);
  m_blinkOn = !m_blinkOn || !any;
  if (!any)
    m_blink->stop();
  update();
}

void GenLcdWidget::setChannelError(int ch, const QString &error)
{
  if (valid(ch))
    m_ch[ch].error = error;
  update();
}

// ---------------------------------------------------------------- text

std::pair<QString, QString> GenLcdWidget::frequencyText(double hz)
{
  if (std::isnan(hz))
    return { QString(), QStringLiteral("Hz") };
  // the prefix that keeps the number below 1000, seven digits in all
  const double a = std::abs(hz);
  const char *unit = a >= 1e6 ? "MHz" : a >= 1e3 ? "kHz" : "Hz";
  const double m = a >= 1e6 ? hz / 1e6 : a >= 1e3 ? hz / 1e3 : hz;
  const int whole = std::abs(m) >= 100 ? 3 : std::abs(m) >= 10 ? 2 : 1;
  return { QString::number(m, 'f', kFrequencyDigits - whole), QString::fromLatin1(unit) };
}

QString GenLcdWidget::text(int ch, Row row) const
{
  if (!valid(ch))
    return QString();
  const State &s = m_ch[ch];
  switch (row)
  {
    case Frequency:
      return frequencyText(s.f).first;
    case Amplitude:
      return format(s.amp, s.spec.ampDecimals);
    case Offset:
      return format(s.offset, s.spec.offsetDecimals);
    case Extra:
      return format(s.shape == Shape::Pulse ? s.duty : s.shape == Shape::Ramp ? s.symmetry : kNone,
                    s.spec.percentDecimals);
  }
  return QString();
}

QString GenLcdWidget::frequencyUnit(int ch) const
{
  return valid(ch) ? frequencyText(m_ch[ch].f).second : QStringLiteral("Hz");
}

QString GenLcdWidget::shapeWord(int ch) const
{
  if (!valid(ch))
    return QString();
  const State &s = m_ch[ch];
  if (s.shape == Shape::Arbitrary)
    return s.arbitrary.toUpper().left(kArbitraryLetters);
  return kShapeWords.value(int(s.shape));
}

// ---------------------------------------------------------------- layout

int GenLcdWidget::digits(double max, int decimals)
{
  const int whole = max >= 1 ? int(QString::number(qint64(std::floor(max))).size()) : 1;
  return whole + qMax(0, decimals);
}

QRectF GenLcdWidget::valueRect(const QRectF &row, double h, int digits, const QString &unit, Qt::Alignment align,
                                bool signCell) const
{
  const QFontMetricsF fm(sansFont(h * kUnitFont));
  const double w = numberWidth(h, digits, signCell) + h * 0.3 + fm.horizontalAdvance(unit) + 2;
  const double x = align & Qt::AlignLeft ? row.left() : row.right() - w;
  return QRectF(x, row.center().y() - h / 2, w, h);
}

double GenLcdWidget::flagsWidth(double fontPx) const
{
  const QFontMetricsF fm(sansFont(fontPx));
  const double gap = fontPx * 0.8;
  double label = 0;
  for (const State &s : m_ch)
    label = qMax(label, fm.horizontalAdvance(s.spec.label) + fontPx * 0.7);
  // the widest shape word any channel could show
  double word = fm.horizontalAdvance(QString(kArbitraryLetters, QLatin1Char('M')));
  for (const QString &t : kShapeWords)
    word = qMax(word, fm.horizontalAdvance(t));
  double w = label + gap + fontPx * 2.2 + gap * 0.5 + word + 2 + gap;
  for (const QString &t : { kOut, kExt })
    w += fm.horizontalAdvance(t) + 2 + gap;
  return w + qMax(fm.horizontalAdvance(kSet), fm.horizontalAdvance(kErr)) + 2;
}

QList<GenLcdWidget::Block> GenLcdWidget::layout() const
{
  QList<Block> blocks;
  const int n = qMax(1, channelCount());
  const Frame f = frame();
  const double fw = f.face.width(), fh = f.face.height();
  const double gap = qMin(fw / n, fh) * 0.04;
  const QRectF inner = f.face.adjusted(fw * 0.02, fh * 0.03, -fw * 0.02, -fh * 0.03);
  const double cellW = (inner.width() - gap * (n - 1)) / n;

  for (int k = 0; k < channelCount(); ++k)
  {
    const State &s = m_ch[k];
    Block b;
    b.cell = QRectF(inner.left() + k * (cellW + gap), inner.top(), cellW, inner.height());
    const double pad = b.cell.height() * 0.05;
    const QRectF c = b.cell.adjusted(pad * 1.5, pad, -pad * 1.5, -pad);
    b.flags = QRectF(c.left(), c.top(), c.width(), c.height() * kFlagsRow);
    b.freq = QRectF(c.left(), b.flags.bottom(), c.width(), c.height() * kFreqRow);
    const QRectF small(c.left(), b.freq.bottom(), c.width(), c.height() * kSmallRow);
    const double half = (small.width() - small.width() * 0.06) / 2;
    b.amp = QRectF(small.left(), small.top(), half, small.height());
    b.offset = QRectF(small.right() - half, small.top(), half, small.height());
    b.extra = QRectF(c.left(), small.bottom(), c.width(), c.height() * kExtraRow);

    // a digit height that fills the row and lets number and unit fit its width
    auto fit = [this](const QRectF &row, double share, int digits, const QString &unit, bool signCell = false)
    {
      double h = row.height() * share;
      auto width = [&] { return valueRect(row, h, digits, unit, Qt::AlignRight, signCell).width(); };
      for (int i = 0; i < 20 && width() > row.width() && h > 3; ++i)
        h = qMax(3.0, h * row.width() / width() * 0.98);
      return h;
    };
    const int ampDigits = digits(s.spec.ampMax, s.spec.ampDecimals);
    const int offsetDigits = digits(s.spec.offsetMax, s.spec.offsetDecimals);
    b.freqH = fit(b.freq, 0.8, kFrequencyDigits, QStringLiteral("MHz"));
    b.smallH = qMin(qMin(fit(b.amp, 0.78, ampDigits, QStringLiteral("Vpp")), fit(b.offset, 0.78, offsetDigits, QStringLiteral("V"), true)),
                    b.freqH * 0.62);
    b.freqText = valueRect(b.freq, b.freqH, kFrequencyDigits, QStringLiteral("MHz"), Qt::AlignRight);
    b.ampText = valueRect(b.amp, b.smallH, ampDigits, QStringLiteral("Vpp"), Qt::AlignLeft);
    b.offsetText = valueRect(b.offset, b.smallH, offsetDigits, QStringLiteral("V"), Qt::AlignRight, true);
    // the duty or symmetry right-aligned, its word at the left end
    b.extraH = qMin(fit(b.extra, 0.78, 3 + s.spec.percentDecimals, QStringLiteral("%")), b.smallH * 0.8);
    b.extraText = valueRect(b.extra, b.extraH, 3 + s.spec.percentDecimals, QStringLiteral("%"), Qt::AlignRight);

    double fpx = b.flags.height() * 0.62;
    for (int i = 0; i < 20 && flagsWidth(fpx) > b.flags.width() && fpx > 4; ++i)
      fpx = qMax(4.0, fpx * b.flags.width() / flagsWidth(fpx) * 0.97);
    b.flagsPx = fpx;
    blocks << b;
  }
  return blocks;
}

double GenLcdWidget::minAspect() const
{
  return qMax(1, channelCount()) * kBlockMinAspect;
}

double GenLcdWidget::maxAspect() const
{
  return qMax(1, channelCount()) * kBlockMaxAspect;
}

QSize GenLcdWidget::sizeHint() const
{
  const double blockH = 170;
  const double blockW = blockH * (kBlockMinAspect + kBlockMaxAspect) / 2;
  return QSize(int(qMax(1, channelCount()) * blockW * 1.08), int(blockH * 1.12));
}

QSize GenLcdWidget::minimumSizeHint() const
{
  return sizeHint() / 2;
}

// --------------------------------------------------------------- drawing

void GenLcdWidget::paintContent(QPainter &p)
{
  const QList<Block> blocks = layout();
  for (int k = 0; k < int(blocks.size()); ++k)
    drawBlock(p, blocks[k], k);
}

void GenLcdWidget::drawBlock(QPainter &p, const Block &b, int ch) const
{
  const State &s = m_ch[ch];
  if (channelCount() > 1)
  {
    // the blocks apart, faintly, as printed on a front's glass
    const double radius = b.cell.height() * 0.04;
    p.save();
    p.setPen(QPen(ghost(), qMax(1.0, b.cell.height() * 0.008)));
    p.setBrush(Qt::NoBrush);
    p.drawRoundedRect(b.cell, radius, radius);
    p.restore();
  }
  drawFlags(p, b, ch);

  const QString f = text(ch, Frequency);
  double w = drawNumber(p, b.freqText.topLeft(), b.freqH, f, kFrequencyDigits, !f.isEmpty(), false);
  drawUnit(p, QPointF(b.freqText.left() + w + b.freqH * 0.3, b.freqText.top()), b.freqH, frequencyUnit(ch));

  const QString amp = text(ch, Amplitude);
  w = drawNumber(p, b.ampText.topLeft(), b.smallH, amp, digits(s.spec.ampMax, s.spec.ampDecimals), !amp.isEmpty(), false);
  drawUnit(p, QPointF(b.ampText.left() + w + b.smallH * 0.3, b.ampText.top()), b.smallH, QStringLiteral("Vpp"));

  const QString offset = text(ch, Offset);
  w = drawNumber(p, b.offsetText.topLeft(), b.smallH, offset, digits(s.spec.offsetMax, s.spec.offsetDecimals),
                 !offset.isEmpty());
  drawUnit(p, QPointF(b.offsetText.left() + w + b.smallH * 0.3, b.offsetText.top()), b.smallH, QStringLiteral("V"));

  // duty of a pulse, symmetry of a ramp; the row stays for the others
  const QString extra = text(ch, Extra);
  const QString word = s.shape == Shape::Ramp ? kSym : kDuty;
  const double fontPx = b.flagsPx;
  const QFontMetricsF fm(sansFont(fontPx));
  drawAnnunciator(p, QRectF(b.extra.left(), b.extra.top(), fm.horizontalAdvance(word) + 2, b.extra.height()), word,
                  !extra.isEmpty(), fontPx);
  w = drawNumber(p, b.extraText.topLeft(), b.extraH, extra, 3 + s.spec.percentDecimals, !extra.isEmpty(), false);
  drawUnit(p, QPointF(b.extraText.left() + w + b.extraH * 0.3, b.extraText.top()), b.extraH, QStringLiteral("%"));
}

void GenLcdWidget::drawFlags(QPainter &p, const Block &b, int ch) const
{
  const State &s = m_ch[ch];
  const double fontPx = b.flagsPx;
  const QFontMetricsF fm(sansFont(fontPx));
  const double gap = fontPx * 0.8;
  const QRectF &r = b.flags;

  // the name on its colour, lettered light or dark to read on it
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
  drawShape(p, QRectF(x, r.center().y() - fontPx * 0.55, fontPx * 2.2, fontPx * 1.1), s.shape, true);
  x += fontPx * 2.2 + gap * 0.5;
  auto flag = [&](const QString &t, bool on)
  {
    const double w = fm.horizontalAdvance(t);
    drawAnnunciator(p, QRectF(x, r.top(), w + 2, r.height()), t, on, fontPx);
    x += w + 2 + gap;
  };
  flag(shapeWord(ch), true);
  flag(kOut, s.out);
  flag(kExt, s.ext);
  // SET at the right end; ERR in its place while a request failed
  const QString &last = s.error.isEmpty() ? kSet : kErr;
  const double lastW = fm.horizontalAdvance(last);
  drawAnnunciator(p, QRectF(r.right() - lastW - 2, r.top(), lastW + 2, r.height()), last,
                  !s.error.isEmpty() || (setShown(ch) && m_blinkOn), fontPx);
}

QPainterPath GenLcdWidget::shapePath(Shape shape, const QRectF &r)
{
  QPainterPath path;
  const double l = r.left(), w = r.width(), top = r.top(), bottom = r.bottom(), mid = r.center().y();
  switch (shape)
  {
    case Shape::Sine:
      path.moveTo(l, mid);
      for (int i = 1; i <= 24; ++i)
        path.lineTo(l + w * i / 24.0, mid - std::sin(2 * std::numbers::pi * i / 24.0) * r.height() / 2);
      break;
    case Shape::Square:
      path.moveTo(l, bottom);
      path.lineTo(l, top);
      path.lineTo(l + w / 2, top);
      path.lineTo(l + w / 2, bottom);
      path.lineTo(l + w, bottom);
      path.lineTo(l + w, top);
      break;
    case Shape::Ramp:
      path.moveTo(l, bottom);
      path.lineTo(l + w / 2, top);
      path.lineTo(l + w, bottom);
      break;
    case Shape::Pulse:
      path.moveTo(l, bottom);
      path.lineTo(l + w * 0.3, bottom);
      path.lineTo(l + w * 0.3, top);
      path.lineTo(l + w * 0.5, top);
      path.lineTo(l + w * 0.5, bottom);
      path.lineTo(l + w, bottom);
      break;
    case Shape::Arbitrary:
      // a wave of no rule
      path.moveTo(l, mid);
      path.cubicTo(l + w * 0.2, top, l + w * 0.3, bottom, l + w * 0.5, mid);
      path.cubicTo(l + w * 0.6, top - r.height() * 0.2, l + w * 0.8, mid, l + w, bottom);
      break;
  }
  return path;
}

void GenLcdWidget::drawShape(QPainter &p, const QRectF &r, Shape shape, bool on) const
{
  // one period of the shape, drawn like a segment
  const QPainterPath path = shapePath(shape, r);
  p.save();
  p.setRenderHint(QPainter::Antialiasing);
  p.setPen(QPen(on ? ink() : ghost(), qMax(1.0, r.height() * 0.14), Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
  p.setBrush(Qt::NoBrush);
  p.drawPath(path);
  p.restore();
}
