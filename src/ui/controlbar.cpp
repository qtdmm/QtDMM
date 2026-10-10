// Copyright (c) 2026 The QtDMM developers
// SPDX-License-Identifier: GPL-3.0-or-later
#include "ui/controlbar.h"

#include <QFrame>
#include <QHBoxLayout>
#include <QMouseEvent>
#include <QPainter>
#include <QElapsedTimer>
#include <QToolButton>

#include "core/reading.h"

namespace
{
constexpr int kGap = 6;   // between groups and rows
constexpr int kBottomMargin = 6;  // below the last row

// A key in a colour of its own: darker while pressed or on, faded while off
QString keyColour(const QColor &face, const QColor &text)
{
  QColor faded = face;
  faded.setAlpha(90);
  return QString("QToolButton { background: %1; color: %2; border: 1px solid %3; border-radius: 3px;"
                 " padding: 2px 6px; }"
                 " QToolButton:pressed, QToolButton:checked { background: %3; }"
                 " QToolButton:disabled { background: %4; color: %5; }")
    .arg(face.name(), text.name(), face.darker(135).name(), faded.name(QColor::HexArgb),
         QColor(text.red(), text.green(), text.blue(), 120).name(QColor::HexArgb));
}
}

ControlBar::ControlBar(QWidget *parent) :
  QWidget(parent)
{
  struct Key { const char *key; const char *text; const char *tip; bool state; };
  const QList<QList<Key>> groups = {
    { { "select1", QT_TR_NOOP("SELECT"), QT_TR_NOOP("The meter's SELECT key (the orange one): the next function on this switch position"), false },
      { "select2", QT_TR_NOOP("Hz/%"), QT_TR_NOOP("Frequency and duty cycle"), false } },
    { { "range", QT_TR_NOOP("RANGE"), QT_TR_NOOP("The next range (switches to manual ranging)"), false },
      { "auto", QT_TR_NOOP("AUTO"), QT_TR_NOOP("Automatic ranging"), true } },
    { { "rel", QT_TR_NOOP("REL"), QT_TR_NOOP("Relative reading: the current value becomes zero"), true },
      { "minmax", QT_TR_NOOP("MIN/MAX"), QT_TR_NOOP("The meter's own minimum and maximum"), true },
      { "peak", QT_TR_NOOP("PEAK"), QT_TR_NOOP("Peak minimum and maximum"), true } },
    // HOLD on the right, as on the UT60BT, where it shares its key with LIGHT
    { { "lamp", QT_TR_NOOP("LIGHT"), QT_TR_NOOP("The display backlight"), false },
      { "hold", QT_TR_NOOP("HOLD"), QT_TR_NOOP("Freeze the display"), true } },
  };
  for (const QList<Key> &keys : groups)
  {
    auto *group = new QFrame(this);
    auto *row = new QHBoxLayout(group);
    row->setContentsMargins(0, 0, 0, 0);
    row->setSpacing(2);
    for (const Key &k : keys)
    {
      auto *b = new QToolButton(group);
      b->setText(tr(k.text));
      b->setToolTip(tr(k.tip));
      b->setCheckable(k.state);
      b->setAutoRaise(false);
      b->setToolButtonStyle(Qt::ToolButtonTextOnly);
      const QString key = k.key;
      // MIN/MAX and PEAK: held down, the key leaves the mode, as on the meter
      const bool leave = key == QLatin1String("minmax") || key == QLatin1String("peak");
      if (leave)
      {
        b->setToolTip(tr(k.tip) + "\n" + tr("Hold the key to leave it, as on the meter."));
        auto *held = new QElapsedTimer;
        connect(b, &QObject::destroyed, b, [held] { delete held; });
        connect(b, &QToolButton::pressed, b, [held] { held->start(); });
        connect(b, &QToolButton::clicked, this, [this, b, key, held]
        {
          // Qt has toggled the check already: on before the click is unchecked now
          const bool wasOn = !b->isChecked();
          const bool off = wasOn && held->isValid() && held->elapsed() >= kLongPressMs;
          b->setChecked(!off);
          Q_EMIT keyPressed(off ? key + "_off" : key);
        });
      }
      else
        // a state key shows what the meter reports, not the click
        connect(b, &QToolButton::clicked, this, [this, b, key, state = k.state]
        {
          if (state)
            b->setChecked(!b->isChecked());
          Q_EMIT keyPressed(key);
        });
      row->addWidget(b);
      m_keys << b;
      b->setProperty("key", key);
      // SELECT and HOLD in the colours of the meter's keys (UT60BT)
      if (key == "select1")
        b->setStyleSheet(keyColour(QColor(0xff, 0xc6, 0x1c), QColor(0x20, 0x20, 0x20)));
      else if (key == "hold")
        b->setStyleSheet(keyColour(QColor(0x3c, 0x9a, 0xff), Qt::white));
      if (key == "hold")
        m_hold = b;
      else if (key == "auto")
        m_auto = b;
      else if (key == "rel")
        m_rel = b;
      else if (key == "minmax")
        m_minmax = b;
      else if (key == "peak")
        m_peak = b;
    }
    m_groups << group;
  }
  setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Fixed);

  setConnected(false);
}

bool ControlBar::supported(const DmmDecoder::DMMInfo &info)
{
  return info.protocol == FrameFormat::UniTiDMM || info.protocol == FrameFormat::UniTUT61Plus;
}

void ControlBar::setConnected(bool connected)
{
  for (QToolButton *b : std::as_const(m_keys))
    b->setEnabled(connected);
}

void ControlBar::setProtocol(FrameFormat::DataFormat format)
{
  const std::shared_ptr<DmmDecoder> decoder = DmmDecoder::getInstance(format);
  for (QToolButton *b : std::as_const(m_keys))
    b->setVisible(decoder && !decoder->keyRequest(b->property("key").toString()).isEmpty());
  updateGeometry();
}

void ControlBar::showReading(const Reading &reading)
{
  if (reading.id != 0)
    return;
  m_hold->setChecked(reading.hold);
  m_auto->setChecked(reading.range == "AUTO");
  m_rel->setChecked(reading.flags & SampleFlag::Relative);
  m_minmax->setChecked(reading.flags & (SampleFlag::Max | SampleFlag::Min));
  m_peak->setChecked(reading.flags & SampleFlag::Peak);
}

int ControlBar::layoutGroups(int width, bool apply) const
{
    const int gap = 2 * kGap;
    int y = 0;
    int i = 0;
    const int n = m_groups.size();
    while (i < n)
    {
        // evaluate row from group i
        int rowWidth = 0, rowHeight = 0, j = i;
        while (j < n)
        {
            const QSize s = m_groups[j]->sizeHint();
            const int w = rowWidth + (j > i ? gap : 0) + s.width();
            if (j > i && w > width)
                break;
            rowWidth = w;
            rowHeight = qMax(rowHeight, s.height());
            ++j;
        }
        if (apply)
        {
            int x = qMax(0, (width - rowWidth) / 2);
            for (int k = i; k < j; ++k)
            {
                const QSize s = m_groups[k]->sizeHint();
                m_groups[k]->setGeometry(x, y, s.width(), s.height());
                x += s.width() + gap;
            }
        }
        y += rowHeight;
        if (j < n)
            y += kGap;
        i = j;
    }
    return y + kBottomMargin;
}

int ControlBar::heightForWidth(int width) const
{
  return layoutGroups(width, false);
}

QSize ControlBar::sizeHint() const
{
  int w = 0;
  for (QWidget *g : m_groups)
    w += g->sizeHint().width() + 2 * kGap;
  return QSize(w - 2 * kGap, heightForWidth(w));
}

QSize ControlBar::minimumSizeHint() const
{
  int w = 0;
  for (QWidget *g : m_groups)
    w = qMax(w, g->sizeHint().width());
  return QSize(w, heightForWidth(w));
}

void ControlBar::resizeEvent(QResizeEvent *event)
{
  QWidget::resizeEvent(event);
  const int h = layoutGroups(width(), true);
  if (h != height())
  {
    setFixedHeight(h);
    updateGeometry();
  }
}

// ---------------------------------------------------------------- FoldButton

FoldButton::FoldButton(QWidget *parent) :
  QWidget(parent)
{
  setFixedSize(20, 20);
  setCursor(Qt::PointingHandCursor);
  setFolded(false);
}

void FoldButton::setFolded(bool folded)
{
  m_folded = folded;
  setToolTip(folded ? tr("Show controls") : tr("Hide controls"));
  update();
}

void FoldButton::paintEvent(QPaintEvent *)
{
  QPainter p(this);
  p.setRenderHint(QPainter::Antialiasing);
  p.setPen(Qt::NoPen);
  p.setBrush(palette().color(m_hover ? QPalette::WindowText : QPalette::Mid));
  // ▾ open, ▸ folded
  const QPointF c = rect().center();
  QPolygonF tri;
  if (m_folded)
    tri << c + QPointF(-3, -5) << c + QPointF(4, 0) << c + QPointF(-3, 5);
  else
    tri << c + QPointF(-5, -3) << c + QPointF(5, -3) << c + QPointF(0, 4);
  p.drawPolygon(tri);
}

void FoldButton::enterEvent(QEnterEvent *)
{
  m_hover = true;
  update();
}

void FoldButton::leaveEvent(QEvent *)
{
  m_hover = false;
  update();
}

void FoldButton::mousePressEvent(QMouseEvent *event)
{
  if (event->button() != Qt::LeftButton)
    return;
  setFolded(!m_folded);
  Q_EMIT toggled(m_folded);
}
