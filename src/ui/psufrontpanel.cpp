// Copyright (c) 2026 The QtDMM developers
// SPDX-License-Identifier: GPL-3.0-or-later
#include "ui/psufrontpanel.h"

#include <QEvent>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QMessageBox>
#include <QPainter>
#include <QTimer>
#include <QToolButton>
#include <QVBoxLayout>

#include "core/powersupply.h"
#include "ui/controlbar.h"
#include "ui/rotaryknob.h"
#include "ui/views/psulcdwidget.h"

namespace
{
// the OUT key's LED: green while the output is on
QIcon led(bool on)
{
  QPixmap pm(12, 12);
  pm.fill(Qt::transparent);
  QPainter p(&pm);
  p.setRenderHint(QPainter::Antialiasing, true);
  p.setPen(QPen(QColor(0x10, 0x10, 0x10), 1));
  p.setBrush(on ? QColor(0x30, 0xe0, 0x40) : QColor(0x3a, 0x40, 0x3a));
  p.drawEllipse(QRectF(1, 1, 10, 10));
  return QIcon(pm);
}

PsuLcdWidget::Mode lcdMode(PowerSupply::Mode mode)
{
  return mode == PowerSupply::Mode::CV ? PsuLcdWidget::Mode::CV
         : mode == PowerSupply::Mode::CC ? PsuLcdWidget::Mode::CC
                                         : PsuLcdWidget::Mode::None;
}
}

PsuFrontPanel::PsuFrontPanel(PowerSupply *supply, QWidget *parent)
  : QWidget(parent)
  , m_supply(supply)
  , m_display(new PsuLcdWidget(this))
  , m_controls(new QWidget(this))
  , m_grid(new QGridLayout(m_controls))
  , m_fold(new FoldButton(this))
  , m_gear(new PanelGear(m_display))
  , m_send(new QTimer(this))
{
  auto *layout = new QVBoxLayout(this);
  layout->setContentsMargins(0, 0, 0, 0);
  layout->setSpacing(4);
  layout->addWidget(m_display, 1);
  layout->addWidget(m_controls);
  layout->addWidget(m_fold, 0, Qt::AlignHCenter);
  m_grid->setContentsMargins(4, 0, 4, 0);
  m_grid->setHorizontalSpacing(8);
  m_grid->setVerticalSpacing(4);

  m_gear->setInk(m_display->plateInk(false), m_display->plateInk(true));
  m_gear->hide();
  connect(m_gear, &PanelGear::clicked, this, &PsuFrontPanel::settingsRequested);
  m_display->installEventFilter(this);
  connect(m_fold, &FoldButton::toggled, this, [this](bool folded)
  {
    setFolded(folded);
    Q_EMIT foldToggled(folded);
  });

  // the first turn goes at once, the next at most every kSendInterval
  m_send->setSingleShot(true);
  m_send->setInterval(kSendInterval);
  connect(m_send, &QTimer::timeout, this, &PsuFrontPanel::sendPending);

  if (m_supply)
  {
    connect(m_supply, &PowerSupply::stateChanged, this, &PsuFrontPanel::showState);
    connect(m_supply, &PowerSupply::settingsChanged, this, &PsuFrontPanel::rebuild);
  }
  rebuild();
}

void PsuFrontPanel::setCaption(const QString &caption)
{
  m_display->setCaption(caption);
  placeCorner();
}

RotaryKnob *PsuFrontPanel::knob(int ch) const
{
  for (const Strip &s : m_strips)
    if (s.ch == ch)
      return s.knob;
  return nullptr;
}

QToolButton *PsuFrontPanel::outputKey(int ch) const
{
  for (const Strip &s : m_strips)
    if (s.ch == ch)
      return s.out;
  return nullptr;
}

void PsuFrontPanel::setFolded(bool folded)
{
  m_fold->setFolded(folded);
  m_controls->setVisible(!folded);
}

bool PsuFrontPanel::isFolded() const
{
  return m_fold->isFolded();
}

// ---------------------------------------------------------------- building

void PsuFrontPanel::rebuild()
{
  for (const Strip &s : std::as_const(m_strips))
    delete s.box;
  m_strips.clear();
  delete m_allOff;
  m_allOff = nullptr;
  if (!m_supply)
    return;

  const QList<int> shown = m_supply->shownChannels();
  QList<PsuLcdWidget::Channel> lcd;
  for (int ch : shown)
  {
    const PowerSupply::ChannelInfo info = m_supply->channel(ch);
    PsuLcdWidget::Channel c;
    c.label = info.name;
    c.color = m_supply->color(ch);
    c.current = info.caps & PowerSupply::MeasureCurrent;
    c.uMax = info.uMax;
    c.iMax = info.iMax;
    c.uDecimals = info.uDecimals;
    c.iDecimals = info.iDecimals;
    lcd << c;
  }
  m_display->setChannels(lcd);

  // the controls as the blocks: one row for one or two, else 2×2
  const QString key = ControlBar::keyColour(QColor(0x4a, 0x4d, 0x52), QColor(0xee, 0xee, 0xee));
  const int columns = shown.size() <= 2 ? qMax(1, int(shown.size())) : 2;
  for (int k = 0; k < int(shown.size()); ++k)
  {
    const int ch = shown[k];
    const PowerSupply::ChannelInfo info = m_supply->channel(ch);
    const PowerSupply::ChannelSettings set = m_supply->settings(ch);
    Strip s;
    s.ch = ch;
    s.box = new QWidget(m_controls);
    auto *row = new QHBoxLayout(s.box);
    row->setContentsMargins(0, 0, 0, 0);
    row->setSpacing(6);
    // the channel's name on its colour, as on the LCD, for knob and key
    auto *name = new QLabel(info.name, s.box);
    const QColor color = m_supply->color(ch);
    name->setStyleSheet(QString("QLabel { background: %1; color: %2; border-radius: 3px; padding: 1px 5px;"
                                " font-weight: bold; }")
                          .arg(color.name(), PsuLcdWidget::segmentsOn(color).name()));
    s.knob = new RotaryKnob(s.box);
    s.knob->setRange(m_supply->minVoltage(ch), m_supply->maxVoltage(ch));
    s.knob->setSteps(set.stepCoarse, set.stepFine);
    s.knob->setDecimals(info.uDecimals);
    s.knob->setColor(color);
    s.knob->setToolTip(tr("%1: the voltage. Wheel or drag: %2 V a step, with Shift %3 V; double click: type it.")
                         .arg(info.name).arg(set.stepCoarse).arg(set.stepFine));
    s.out = new QToolButton(s.box);
    s.out->setText(QStringLiteral("OUT"));
    s.out->setToolButtonStyle(Qt::ToolButtonTextBesideIcon);
    s.out->setStyleSheet(key);
    s.out->setToolTip(tr("%1: output on or off").arg(info.name));
    row->addStretch(1);
    row->addWidget(name, 0, Qt::AlignVCenter);
    row->addWidget(s.knob, 0, Qt::AlignVCenter);
    row->addWidget(s.out, 0, Qt::AlignVCenter);
    row->addStretch(1);
    m_grid->addWidget(s.box, k / columns, k % columns);
    connect(s.knob, &RotaryKnob::turned, this, [this, ch](double v) { turned(ch, v); });
    connect(s.out, &QToolButton::clicked, this, [this, ch]
    {
      if (!m_supply)
        return;
      const bool on = !m_supply->state(ch).output;
      if (on && !mayTurnOn())
        return;
      m_supply->requestOutput(ch, on);
    });
    m_strips << s;
    showState(ch);
  }
  if (shown.size() > 1)
  {
    m_allOff = new QToolButton(m_controls);
    m_allOff->setText(tr("ALL OUT"));
    m_allOff->setToolTip(tr("Every output shown off"));
    m_allOff->setStyleSheet(ControlBar::keyColour(QColor(0xb0, 0x30, 0x30), Qt::white));
    // below the channels, at the right: the controls stay under their blocks
    m_grid->addWidget(m_allOff, (int(shown.size()) + columns - 1) / columns, 0, 1, columns, Qt::AlignRight);
    connect(m_allOff, &QToolButton::clicked, this, [this]
    {
      if (m_supply)
        m_supply->allOff();
    });
  }
  for (int c = 0; c < 2; ++c)
    m_grid->setColumnStretch(c, c < columns ? 1 : 0);
}

// ---------------------------------------------------------------- state

void PsuFrontPanel::showState(int ch)
{
  int k = 0;
  while (k < int(m_strips.size()) && m_strips[k].ch != ch)
    ++k;
  if (k == int(m_strips.size()) || !m_supply)
    return;
  Strip &s = m_strips[k];
  const PowerSupply::ChannelState st = m_supply->state(ch);
  const bool settable = st.connected && !st.external && (m_supply->channel(ch).caps & PowerSupply::SetVoltage);

  m_display->setValues(k, st.u, st.i, st.p);
  m_display->setOutput(k, st.output);
  m_display->setMode(k, lcdMode(st.mode));
  m_display->setExternal(k, st.external);
  m_display->setChannelStale(k, !st.connected);
  m_display->setChannelError(k, st.setState == PowerSupply::SetState::Failed ? st.error : QString());
  m_display->setToolTip(st.setState == PowerSupply::SetState::Failed
                          ? tr("%1: the voltage was not set: %2").arg(m_supply->channel(ch).name, st.error)
                          : QString());

  s.knob->setEnabled(settable);
  s.out->setEnabled(st.connected);
  s.out->setIcon(led(st.output));
  // the knob follows the device once what was sent came back and nobody turns it
  if (st.setState == PowerSupply::SetState::Confirmed && !s.hasPending && !s.knob->isEditing())
  {
    if (!m_send->isActive())
      s.turning = false;
    if (!s.turning)
      s.knob->setValue(st.setpoint);
  }
  else if (st.setState == PowerSupply::SetState::Idle && !s.turning)
    s.knob->setValue(st.setpoint);
}

void PsuFrontPanel::turned(int ch, double value)
{
  for (int k = 0; k < int(m_strips.size()); ++k)
  {
    Strip &s = m_strips[k];
    if (s.ch != ch)
      continue;
    m_display->showSetpoint(k, value);
    s.turning = true;
    s.pending = value;
    s.hasPending = true;
  }
  if (!m_send->isActive())
    sendPending();
}

void PsuFrontPanel::sendPending()
{
  bool sent = false;
  for (Strip &s : m_strips)
    if (s.hasPending && m_supply)
    {
      s.hasPending = false;
      m_supply->requestVoltage(s.ch, s.pending);
      sent = true;
    }
  if (sent)
  {
    m_send->start();
    return;
  }
  // nothing more turned: the knobs follow the device again
  for (int k = 0; k < int(m_strips.size()); ++k)
    m_strips[k].turning = false;
  for (const Strip &st : std::as_const(m_strips))
    showState(st.ch);
}

bool PsuFrontPanel::mayTurnOn()
{
  if (!m_supply || !m_supply->confirmOutputOn())
    return true;
  return QMessageBox::question(this, tr("Output on"), tr("Switch the output on? The voltage reaches what is connected."))
         == QMessageBox::Yes;
}

// ---------------------------------------------------------------- the gear

bool PsuFrontPanel::eventFilter(QObject *watched, QEvent *event)
{
  if (watched == m_display && event->type() == QEvent::Resize)
    placeCorner();
  return QWidget::eventFilter(watched, event);
}

void PsuFrontPanel::placeCorner()
{
  // at the right end of the name plate, as high as its letters (FrontPanel)
  const QRectF band = m_display->plateBand();
  if (band.isEmpty())
  {
    m_gear->hide();
    return;
  }
  const int size = qBound(10, int(band.height() * 0.8), 18);
  m_gear->setFixedSize(size, size);
  m_gear->move(int(band.right() - size), int(band.center().y() - size / 2.0));
  m_gear->show();
  m_display->setPlateReserve(size + 6);
}
