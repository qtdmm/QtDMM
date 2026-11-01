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
  // longKey: a key the meter reads differently when held, sent instead when
  // the click was a long one
  struct Key { const char *key; const char *text; const char *tip; bool state; const char *longKey = nullptr; };
  const QList<QList<Key>> groups = {
    // the UT202BT's function keys, in the colours of the clamp
    { { "fn_a", QT_TR_NOOP("A~"), QT_TR_NOOP("The A key (yellow): AC A and AC A LPF in turn; held, inrush current"), false, "fn_a_long" },
      { "fn_v", QT_TR_NOOP("V~"), QT_TR_NOOP("The V key (red): AC V, AC V LPF and DC V in turn"), false },
      { "fn_ohm", QT_TR_NOOP("Ω"), QT_TR_NOOP("The Ω key (blue): resistance, continuity, capacitance and temperature in turn"), false },
      { "fn_ncv", QT_TR_NOOP("NCV/PEAK"), QT_TR_NOOP("Non-contact voltage; held, peak capture (AC V and AC A)"), false, "fn_ncv_long" } },

    // the ZOTEK meters' function keys, as in ZOTEK's app; NCV is fn_ncv above
    { { "fn_auto", QT_TR_NOOP("AUTO"), QT_TR_NOOP("Automatic function: the meter picks V, Ω or A by itself"), false },
      { "fn_cap", QT_TR_NOOP("CAP"), QT_TR_NOOP("Capacitance"), false },
      { "fn_diode", QT_TR_NOOP("Diode"), QT_TR_NOOP("Diode and continuity"), false },
      { "fn_hz", QT_TR_NOOP("Hz"), QT_TR_NOOP("Frequency"), false } },
    { { "select1", QT_TR_NOOP("SELECT"), QT_TR_NOOP("The meter's SELECT key (the orange one): the next function on this switch position"), false },
      { "select2", QT_TR_NOOP("Hz/%"), QT_TR_NOOP("Frequency and duty cycle"), false } },
    { { "range", QT_TR_NOOP("RANGE"), QT_TR_NOOP("The next range (switches to manual ranging)"), false },
      { "auto", QT_TR_NOOP("AUTO"), QT_TR_NOOP("Automatic ranging"), true } },
    { { "rel", QT_TR_NOOP("REL"), QT_TR_NOOP("Relative reading: the current value becomes zero"), true },
      { "zero", QT_TR_NOOP("ZERO"), QT_TR_NOOP("Zero the capacitance reading, with the leads open"), false },
      { "minmax", QT_TR_NOOP("MIN/MAX"), QT_TR_NOOP("The meter's own minimum and maximum"), true },
      { "peak", QT_TR_NOOP("PEAK"), QT_TR_NOOP("Peak minimum and maximum"), true } },
    // the UNI-T iENV meters (UT333BT, UT353BT, UT363BT, UT383BT): a key for each mode
    { { "max", QT_TR_NOOP("MAX"), QT_TR_NOOP("The meter shows its maximum"), true },
      { "min", QT_TR_NOOP("MIN"), QT_TR_NOOP("The meter shows its minimum"), true },
      { "avg", QT_TR_NOOP("AVG"), QT_TR_NOOP("The meter shows its average"), true } },
    { { "unit", QT_TR_NOOP("UNIT"), QT_TR_NOOP("The next unit of the main value (lx/fc, dBA/dBC, %RH/%, m/s ... mph)"), false },
      { "unit2", QT_TR_NOOP("°C/°F"), QT_TR_NOOP("The unit of the temperature"), false },
      { "weighting", QT_TR_NOOP("FAST/SLOW"), QT_TR_NOOP("Fast or slow time weighting"), false } },
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
      else if (k.longKey)
      {
        auto *held = new QElapsedTimer;
        connect(b, &QObject::destroyed, b, [held] { delete held; });
        connect(b, &QToolButton::pressed, b, [held] { held->start(); });
        b->setProperty("longKey", QString::fromLatin1(k.longKey));
        connect(b, &QToolButton::clicked, this, [this, b, key, longKey = QString::fromLatin1(k.longKey), held]
        {
          // held, where the meter has the long key (setProtocol())
          const bool isLong = b->property("longOn").toBool() && held->isValid() && held->elapsed() >= kLongPressMs;
          Q_EMIT keyPressed(isLong ? longKey : key);
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
      // SELECT and HOLD in the colours of the meter's keys (UT60BT), the
      // clamp's V, A and Ω keys in theirs (UT202BT)
      if (key == "select1" || key == "fn_a")
        b->setStyleSheet(keyColour(QColor(0xff, 0xc6, 0x1c), QColor(0x20, 0x20, 0x20)));
      else if (key == "hold" || key == "fn_ohm")
        b->setStyleSheet(keyColour(QColor(0x3c, 0x9a, 0xff), Qt::white));
      else if (key == "fn_v")
        b->setStyleSheet(keyColour(QColor(0xe0, 0x3c, 0x31), Qt::white));
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
      else if (key == "max")
        m_max = b;
      else if (key == "min")
        m_min = b;
      else if (key == "avg")
        m_avg = b;
    }
    m_groups << group;
  }
  setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Fixed);

  setConnected(false);
}

bool ControlBar::supported(const DmmDecoder::DMMInfo &info)
{
  return info.protocol == FrameFormat::UniTiDMM || info.protocol == FrameFormat::UniTUT61Plus
         || info.protocol == FrameFormat::UniTUT202BT || info.protocol == FrameFormat::UniTiENV
         || info.protocol == FrameFormat::ZotekBLE || info.protocol == FrameFormat::OwonBLE
         || info.protocol == FrameFormat::OwonBLE15 || info.protocol == FrameFormat::UniTUT181A;
}

void ControlBar::setConnected(bool connected)
{
  for (QToolButton *b : std::as_const(m_keys))
    b->setEnabled(connected);
}

void ControlBar::setProtocol(FrameFormat::DataFormat format, const QString &model)
{
  const std::shared_ptr<DmmDecoder> decoder = DmmDecoder::getInstance(format);
  for (QToolButton *b : std::as_const(m_keys))
    b->setVisible(decoder && decoder->hasKey(b->property("key").toString(), model));
  // a key held sends its long key only where the meter has one; NCV held
  // is the UT202BT's PEAK, a ZOTEK meter's NCV key is NCV alone
  for (QToolButton *b : std::as_const(m_keys))
  {
    const QString longKey = b->property("longKey").toString();
    const bool longOn = decoder && !longKey.isEmpty() && decoder->hasKey(longKey, model);
    b->setProperty("longOn", longOn);
    if (b->property("key").toString() == QLatin1String("fn_ncv"))
    {
      b->setText(longOn ? tr("NCV/PEAK") : tr("NCV"));
      b->setToolTip(longOn ? tr("Non-contact voltage; held, peak capture (AC V and AC A)") : tr("Non-contact voltage"));
    }
  }
  // a group without a key of this meter takes no place
  for (QWidget *g : std::as_const(m_groups))
  {
    bool any = false;
    for (QToolButton *b : g->findChildren<QToolButton *>())
      any = any || !b->isHidden();
    g->setVisible(any);
  }
  // the groups have other widths now; at the same size no resizeEvent
  // places them anew (they overlapped until the window was resized)
  relayout();
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
  m_max->setChecked(reading.flags & SampleFlag::Max);
  m_min->setChecked(reading.flags & SampleFlag::Min);
  m_avg->setChecked(reading.flags & SampleFlag::Avg);
}

int ControlBar::layoutGroups(int width, bool apply) const
{
    const int gap = 2 * kGap;
    int y = 0;
    int i = 0;
    QList<QWidget *> groups;
    for (QWidget *g : m_groups)
      if (!g->isHidden())
        groups << g;
    const int n = groups.size();
    while (i < n)
    {
        // evaluate row from group i
        int rowWidth = 0, rowHeight = 0, j = i;
        while (j < n)
        {
            const QSize s = groups[j]->sizeHint();
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
                const QSize s = groups[k]->sizeHint();
                groups[k]->setGeometry(x, y, s.width(), s.height());
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
    if (!g->isHidden())
      w += g->sizeHint().width() + 2 * kGap;
  return QSize(w - 2 * kGap, heightForWidth(w));
}

QSize ControlBar::minimumSizeHint() const
{
  int w = 0;
  for (QWidget *g : m_groups)
    if (!g->isHidden())
      w = qMax(w, g->sizeHint().width());
  return QSize(w, heightForWidth(w));
}

void ControlBar::resizeEvent(QResizeEvent *event)
{
  QWidget::resizeEvent(event);
  relayout();
}

void ControlBar::showEvent(QShowEvent *event)
{
  QWidget::showEvent(event);
  relayout();   // set up while hidden
}

void ControlBar::relayout()
{
  // a group's size hint follows its keys shown only once its layout has run
  for (QWidget *g : std::as_const(m_groups))
    if (g->layout())
      g->layout()->activate();
  const int h = layoutGroups(width(), true);
  if (h != height())
    setFixedHeight(h);
  updateGeometry();
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
