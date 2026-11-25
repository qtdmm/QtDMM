// Copyright (c) 2026 The QtDMM developers
// SPDX-License-Identifier: GPL-3.0-or-later
#include "ui/genfrontpanel.h"

#include <QHBoxLayout>
#include <QLabel>
#include <QMenu>
#include <QPainter>
#include <QTimer>
#include <QToolButton>
#include <QVBoxLayout>

#include <cmath>

#include "ui/controlbar.h"
#include "ui/rotaryknob.h"
#include "ui/views/genlcdwidget.h"

namespace
{
using Shape = FunctionGenerator::Shape;
using Value = FunctionGenerator::Value;
constexpr std::array<Shape, FunctionGenerator::kBasicShapes> kBasic { Shape::Sine, Shape::Square, Shape::Ramp, Shape::Pulse };
constexpr std::array<Value, FunctionGenerator::kValues> kAll { Value::Frequency, Value::Amplitude, Value::Offset, Value::Duty,
                                                               Value::Symmetry };

// a shape key's symbol, light on the dark key
QIcon shapeIcon(Shape shape)
{
  QPixmap pm(28, 16);
  pm.fill(Qt::transparent);
  QPainter p(&pm);
  p.setRenderHint(QPainter::Antialiasing, true);
  p.setPen(QPen(QColor(0xee, 0xee, 0xee), 1.8, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
  p.drawPath(GenLcdWidget::shapePath(shape, QRectF(3, 3, 22, 10)));
  return QIcon(pm);
}

QString shapeTip(Shape shape)
{
  switch (shape)
  {
    case Shape::Sine: return GenFrontPanel::tr("Sine");
    case Shape::Square: return GenFrontPanel::tr("Square");
    case Shape::Ramp: return GenFrontPanel::tr("Ramp (triangle; its symmetry on the last knob)");
    case Shape::Pulse: return GenFrontPanel::tr("Pulse (its duty cycle on the last knob)");
    case Shape::Arbitrary: return GenFrontPanel::tr("The generator's other shapes");
  }
  return QString();
}

size_t at(Value v)
{
  return size_t(v);
}
}

GenFrontPanel::GenFrontPanel(FunctionGenerator *generator, QWidget *parent)
  : OutputFront(parent)
  , m_generator(generator)
  , m_display(new GenLcdWidget(this))
  , m_row(new QHBoxLayout)
  , m_send(new QTimer(this))
{
  setDisplay(m_display);
  auto *controlsLayout = new QVBoxLayout(controls());
  controlsLayout->setContentsMargins(4, 0, 4, 0);
  controlsLayout->setSpacing(4);
  m_row->setSpacing(12);
  controlsLayout->addLayout(m_row);

  // the first turn goes at once, the next at most every kSendInterval
  m_send->setSingleShot(true);
  m_send->setInterval(kSendInterval);
  connect(m_send, &QTimer::timeout, this, &GenFrontPanel::sendPending);

  if (m_generator)
  {
    connect(m_generator, &FunctionGenerator::stateChanged, this, &GenFrontPanel::showState);
    connect(m_generator, &FunctionGenerator::settingsChanged, this, &GenFrontPanel::rebuild);
  }
  rebuild();
}

GenFrontPanel::Strip *GenFrontPanel::strip(int ch)
{
  for (Strip &s : m_strips)
    if (s.ch == ch)
      return &s;
  return nullptr;
}

const GenFrontPanel::Strip *GenFrontPanel::strip(int ch) const
{
  for (const Strip &s : m_strips)
    if (s.ch == ch)
      return &s;
  return nullptr;
}

RotaryKnob *GenFrontPanel::knob(int ch, Value v) const
{
  const Strip *s = strip(ch);
  return s ? s->knob[at(v)] : nullptr;
}

QToolButton *GenFrontPanel::shapeKey(int ch, Shape shape) const
{
  const Strip *s = strip(ch);
  return s ? s->shape[size_t(shape)] : nullptr;
}

QToolButton *GenFrontPanel::outputKey(int ch) const
{
  const Strip *s = strip(ch);
  return s ? s->out : nullptr;
}

// ---------------------------------------------------------------- building

void GenFrontPanel::rebuild()
{
  for (const Strip &s : std::as_const(m_strips))
    delete s.box;
  m_strips.clear();
  delete m_allOff;
  m_allOff = nullptr;
  if (!m_generator)
    return;

  const QList<int> shown = m_generator->shownChannels();
  QList<GenLcdWidget::Channel> lcd;
  for (int ch : shown)
  {
    const FunctionGenerator::ChannelInfo info = m_generator->channel(ch);
    GenLcdWidget::Channel c;
    c.label = info.name;
    c.color = m_generator->color(ch);
    c.ampMax = info.ampMax;
    c.offsetMax = info.offsetMax;
    c.ampDecimals = info.ampDecimals;
    c.offsetDecimals = info.offsetDecimals;
    c.percentDecimals = info.percentDecimals;
    lcd << c;
  }
  m_display->setChannels(lcd);

  for (int k = 0; k < int(shown.size()); ++k)
  {
    Strip s;
    s.ch = shown[k];
    buildStrip(s, k);
    m_strips << s;
  }
  for (const Strip &s : std::as_const(m_strips))
    showState(s.ch);
  if (shown.size() > 1)
  {
    m_allOff = new QToolButton(controls());
    // not red: that is the emergency off of all devices in the status bar
    m_allOff->setText(tr("All OUT off"));
    m_allOff->setToolTip(tr("Every output of this device shown off"));
    m_allOff->setStyleSheet(ControlBar::keyColour(QColor(0x4a, 0x4d, 0x52), QColor(0xee, 0xee, 0xee)));
    static_cast<QVBoxLayout *>(controls()->layout())->addWidget(m_allOff, 0, Qt::AlignRight);
    connect(m_allOff, &QToolButton::clicked, this, [this]
    {
      if (m_generator)
        m_generator->allOff();
    });
  }
}

void GenFrontPanel::buildStrip(Strip &s, int column)
{
  const int ch = s.ch;
  const FunctionGenerator::ChannelInfo info = m_generator->channel(ch);
  const FunctionGenerator::ChannelSettings set = m_generator->settings(ch);
  const QColor color = m_generator->color(ch);
  const QString key = ControlBar::keyColour(QColor(0x4a, 0x4d, 0x52), QColor(0xee, 0xee, 0xee));
  // the shape on: the key in the channel's colour
  const QString shapeKeyStyle =
    key + QString(" QToolButton:checked { background: %1; color: %2; }").arg(color.name(), GenLcdWidget::segmentsOn(color).name());

  s.box = new QWidget(controls());
  auto *box = new QVBoxLayout(s.box);
  box->setContentsMargins(0, 0, 0, 0);
  box->setSpacing(4);

  // the name, the shapes, OUT
  auto *keys = new QHBoxLayout;
  keys->setSpacing(4);
  auto *name = new QLabel(info.name, s.box);
  name->setStyleSheet(QString("QLabel { background: %1; color: %2; border-radius: 3px; padding: 1px 5px;"
                              " font-weight: bold; }")
                        .arg(color.name(), GenLcdWidget::segmentsOn(color).name()));
  keys->addWidget(name, 0, Qt::AlignVCenter);
  for (Shape shape : kBasic)
  {
    if (!info.shapes.contains(shape))
      continue;
    auto *b = new QToolButton(s.box);
    b->setIcon(shapeIcon(shape));
    b->setIconSize(QSize(28, 16));
    b->setCheckable(true);
    b->setStyleSheet(shapeKeyStyle);
    b->setToolTip(QString("%1: %2").arg(info.name, shapeTip(shape)));
    connect(b, &QToolButton::clicked, this, [this, ch, shape]
    {
      if (m_generator)
        m_generator->requestShape(ch, shape);
      showState(ch);   // the key stays as the device has it until it answers
    });
    s.shape[size_t(shape)] = b;
    keys->addWidget(b);
  }
  if (!info.arbitrary.isEmpty())
  {
    auto *more = new QToolButton(s.box);
    more->setIcon(shapeIcon(Shape::Arbitrary));
    more->setIconSize(QSize(28, 16));
    more->setCheckable(true);
    more->setStyleSheet(shapeKeyStyle);
    more->setToolTip(QString("%1: %2").arg(info.name, shapeTip(Shape::Arbitrary)));
    more->setPopupMode(QToolButton::InstantPopup);
    auto *menu = new QMenu(more);
    for (const QString &arb : info.arbitrary)
      menu->addAction(arb, this, [this, ch, arb]
      {
        if (m_generator)
          m_generator->requestShape(ch, Shape::Arbitrary, arb);
      });
    more->setMenu(menu);
    s.shape[size_t(Shape::Arbitrary)] = more;
    keys->addWidget(more);
  }
  keys->addStretch(1);
  s.out = new QToolButton(s.box);
  s.out->setText(QStringLiteral("OUT"));
  s.out->setToolButtonStyle(Qt::ToolButtonTextBesideIcon);
  s.out->setStyleSheet(key);
  s.out->setToolTip(tr("%1: output on or off").arg(info.name));
  connect(s.out, &QToolButton::clicked, this, [this, ch]
  {
    if (m_generator)
      m_generator->requestOutput(ch, !m_generator->state(ch).output);
  });
  keys->addWidget(s.out, 0, Qt::AlignVCenter);
  box->addLayout(keys);

  // the knobs, each with its caption below
  auto *knobs = new QHBoxLayout;
  knobs->setSpacing(6);
  knobs->addStretch(1);
  struct Spec
  {
    Value v;
    QString caption, tip, unit;
    double coarse, fine;
    int decimals;
  };
  const QList<Spec> specs = {
    { Value::Frequency, tr("Frequency"), tr("1-2-5 a step, with Shift by 1 in the third digit; double click: type it (\"1kHz\")"),
      QStringLiteral("Hz"), 0, 0, 3 },
    { Value::Amplitude, tr("Amplitude"), tr("Peak to peak. Wheel or drag: %1 V a step, with Shift %2 V").arg(set.ampStepCoarse).arg(set.ampStepFine),
      QStringLiteral("Vpp"), set.ampStepCoarse, set.ampStepFine, info.ampDecimals },
    { Value::Offset, tr("Offset"), tr("Wheel or drag: %1 V a step, with Shift %2 V").arg(set.offsetStepCoarse).arg(set.offsetStepFine),
      QStringLiteral("V"), set.offsetStepCoarse, set.offsetStepFine, info.offsetDecimals },
    { Value::Duty, tr("Duty cycle"), tr("Of the pulse: 1 % a step, with Shift 0.1 %"), QStringLiteral("%"), 1, 0.1,
      info.percentDecimals },
    { Value::Symmetry, tr("Symmetry"), tr("Of the ramp, rising part: 1 % a step, with Shift 0.1 %"), QStringLiteral("%"), 1, 0.1,
      info.percentDecimals },
  };
  for (const Spec &spec : specs)
  {
    auto *holder = new QWidget(s.box);
    auto *col = new QVBoxLayout(holder);
    col->setContentsMargins(0, 0, 0, 0);
    col->setSpacing(1);
    auto *k = new RotaryKnob(holder);
    k->setColor(color);
    k->setUnit(spec.unit);
    if (spec.v == Value::Frequency)
      k->setDecades(true);
    else
    {
      k->setSteps(spec.coarse, spec.fine);
      k->setDecimals(spec.decimals);
    }
    k->setToolTip(QString("%1, %2: %3").arg(info.name, spec.caption, spec.tip));
    auto *caption = new QLabel(spec.caption, holder);
    caption->setAlignment(Qt::AlignHCenter);
    QFont f = caption->font();
    f.setPointSizeF(f.pointSizeF() * 0.85);
    caption->setFont(f);
    col->addWidget(k, 0, Qt::AlignHCenter);
    col->addWidget(caption, 0, Qt::AlignHCenter);
    knobs->addWidget(holder);
    s.knob[at(spec.v)] = k;
    s.knobBox[at(spec.v)] = holder;
    connect(k, &RotaryKnob::turned, this, [this, ch, v = spec.v](double value) { turned(ch, v, value); });
  }
  knobs->addStretch(1);
  box->addLayout(knobs);

  m_row->insertWidget(column, s.box, 1);
}

// ---------------------------------------------------------------- state

void GenFrontPanel::showState(int ch)
{
  int k = 0;
  while (k < int(m_strips.size()) && m_strips[k].ch != ch)
    ++k;
  if (k == int(m_strips.size()) || !m_generator)
    return;
  Strip &s = m_strips[k];
  const FunctionGenerator::ChannelState st = m_generator->state(ch);
  const bool settable = st.connected && !st.external;
  const bool failed = st.setState == FunctionGenerator::SetState::Failed;

  // what is shown: the knob while it is the user's, else what was read
  // back, else what was sent
  std::array<double, FunctionGenerator::kValues> shownValue {};
  for (Value v : kAll)
  {
    const double read = st.get(v);
    shownValue[at(v)] = s.turning[at(v)] ? s.knob[at(v)]->value() : !std::isnan(read) ? read : st.requested[at(v)];
  }
  m_display->setShape(k, st.shape, st.arbitrary);
  m_display->setValues(k, shownValue[at(Value::Frequency)], shownValue[at(Value::Amplitude)], shownValue[at(Value::Offset)],
                       shownValue[at(Value::Duty)], shownValue[at(Value::Symmetry)]);
  m_display->setOutput(k, st.output);
  m_display->setExternal(k, st.external);
  m_display->setStale(!st.connected);
  m_display->setChannelError(k, failed ? st.error : QString());
  m_display->setToolTip(failed ? tr("%1: not set: %2").arg(m_generator->channel(ch).name, st.error) : QString());
  if (st.setState == FunctionGenerator::SetState::Pending)
    m_display->markSet(k);

  for (size_t i = 0; i < s.shape.size(); ++i)
    if (s.shape[i])
    {
      s.shape[i]->setEnabled(settable);
      s.shape[i]->setChecked(st.shape == Shape(i));
    }
  s.out->setEnabled(settable);
  s.out->setIcon(OutputFront::led(st.output));

  for (Value v : kAll)
  {
    RotaryKnob *knob = s.knob[at(v)];
    s.knobBox[at(v)]->setVisible(FunctionGenerator::hasValue(st.shape, v));
    knob->setEnabled(settable);
    // the limits move with the other values (offset and amplitude) and the shape
    knob->setRange(m_generator->minimum(ch, v), m_generator->maximum(ch, v));
    // the knob follows the device once what was sent came back and nobody turns it
    if (s.hasPending[at(v)] || knob->isEditing())
      continue;
    if (st.setState != FunctionGenerator::SetState::Pending && !m_send->isActive())
      s.turning[at(v)] = false;
    if (!s.turning[at(v)] && !std::isnan(st.get(v)))
      knob->setValue(st.get(v));
  }
}

void GenFrontPanel::turned(int ch, Value v, double value)
{
  for (int k = 0; k < int(m_strips.size()); ++k)
  {
    Strip &s = m_strips[k];
    if (s.ch != ch)
      continue;
    s.turning[at(v)] = true;
    s.pending[at(v)] = value;
    s.hasPending[at(v)] = true;
    m_display->markSet(k);
  }
  showState(ch);
  if (!m_send->isActive())
    sendPending();
}

void GenFrontPanel::sendPending()
{
  bool sent = false;
  for (Strip &s : m_strips)
    for (Value v : kAll)
      if (s.hasPending[at(v)] && m_generator)
      {
        s.hasPending[at(v)] = false;
        m_generator->requestValue(s.ch, v, s.pending[at(v)]);
        sent = true;
      }
  if (sent)
  {
    m_send->start();
    return;
  }
  // nothing more turned: the knobs follow the device again
  for (Strip &s : m_strips)
    s.turning = {};
  for (const Strip &st : std::as_const(m_strips))
    showState(st.ch);
}
