// Copyright (c) 2026 The QtDMM developers
// SPDX-License-Identifier: GPL-3.0-or-later
#include "ui/frontpanel.h"

#include <QEvent>
#include <QTimer>
#include <QVBoxLayout>

#include <cmath>
#include <memory>

#include "core/functiongenerator.h"
#include "core/powersupply.h"
#include "core/reading.h"
#include "device/meterconnection.h"
#include "logic/logicnode.h"
#include "service/metercontroller.h"
#include "service/noderunner.h"
#include "service/valuefeed.h"
#include "ui/controlbar.h"
#include "ui/nodebar.h"
#include "ui/genfrontpanel.h"
#include "ui/psufrontpanel.h"
#include "ui/views/lcdwidget.h"
#include "ui/views/psulcdwidget.h"

FrontPanel::FrontPanel(DeviceSession *session, QWidget *parent)
  : QWidget(parent)
  , m_feed(new ValueFeed(session, this))
  , m_display(new LcdWidget(this))
  , m_controls(new ControlBar(this))
  , m_fold(new FoldButton(this))
  , m_nodeBar(new NodeBar(this))
  , m_gear(new PanelGear(m_display))
  , m_blinkTimer(new QTimer(this))
{
  auto *layout = new QVBoxLayout(this);
  layout->setContentsMargins(0, 0, 0, 0);
  layout->setSpacing(4);
  layout->addWidget(m_display, 1);
  layout->addWidget(m_controls);
  layout->addWidget(m_nodeBar);
  // the handle under the keys, in the middle of the lower edge
  layout->addWidget(m_fold, 0, Qt::AlignHCenter);
  m_fold->hide();
  m_controls->hide();
  m_nodeBar->hide();
  // the gear on the name plate, in the lettering's colours
  m_gear->setInk(m_display->plateInk(false), m_display->plateInk(true));
  m_gear->hide();
  connect(m_gear, &PanelGear::clicked, this, [this]
  {
    if (m_ctl)
      Q_EMIT settingsRequested(m_ctl->deviceId());
  });
  m_display->installEventFilter(this);

  connect(m_feed, &ValueFeed::reading, m_display, &LcdWidget::showReading);
  connect(m_feed, &ValueFeed::reading, m_controls, &ControlBar::showReading);
  connect(m_feed, &ValueFeed::staleChanged, m_display, &LcdWidget::setStale);
  connect(m_feed, &ValueFeed::staleChanged, this, [this](bool stale)
  {
    if (m_output)
      m_output->lcd()->setStale(stale);
  });
  // the menu of the LCD shown, wherever it is
  m_display->setContextMenuPolicy(Qt::CustomContextMenu);
  connect(m_display, &QWidget::customContextMenuRequested, this, [this](const QPoint &pos)
  {
    Q_EMIT menuRequested(m_display->mapToGlobal(pos));
  });
  connect(m_feed, &ValueFeed::minimumChanged, m_display, &LcdWidget::showMinimum);
  connect(m_feed, &ValueFeed::maximumChanged, m_display, &LcdWidget::showMaximum);
  connect(m_feed, &ValueFeed::minMaxReset, m_display, &LcdWidget::clearMinMax);
  connect(m_feed, &ValueFeed::controllerChanged, this, &FrontPanel::attach);
  connect(m_feed, &ValueFeed::noticeChanged, m_display, &LcdWidget::setNotice);
  m_blinkTimer->setInterval(500);
  connect(m_blinkTimer, &QTimer::timeout, this, [this]
  {
    m_phase = !m_phase;
    showAlert();
  });
  connect(m_fold, &FoldButton::toggled, this, [this](bool folded)
  {
    setFolded(folded);
    Q_EMIT foldToggled(folded);
  });
  connect(m_controls, &ControlBar::keyPressed, this, [this](const QString &key)
  {
    // a Bluetooth link takes a few seconds before it takes keys
    if (!m_ctl || !m_ctl->pressKey(key))
      Q_EMIT keyFailed();
  });
}

void FrontPanel::setValue(const ValueRef &value)
{
  m_feed->setValue(value);
  // the LCD itself falls back while the meter has fewer values (primaryShown())
  m_display->setPrimary(value.id);
}

ValueRef FrontPanel::value() const
{
  return m_feed->value();
}

void FrontPanel::setSecondaries(const std::optional<QList<int>> &ids)
{
  m_secondariesChosen = ids.has_value();
  m_display->setSecondaries(ids);
}

QList<int> FrontPanel::secondaries() const
{
  return m_display->secondaries();
}

MeterController *FrontPanel::controller() const
{
  return m_feed->controller();
}

void FrontPanel::attach(MeterController *ctl)
{
  // another meter: its lines afresh, none of the one before; its name above
  m_display->setCaption(ctl ? ctl->deviceName() : QString());
  m_display->setNotice(m_feed->notice());
  m_display->clearMinMax();
  for (int id = 0; id < kMaxValues; ++id)
  {
    Reading none;
    none.id = id;
    none.quality = Quality::NoValue;
    m_display->showReading(none);
  }
  disconnect(m_link);
  for (const QMetaObject::Connection &c : std::as_const(m_rules))
    disconnect(c);
  m_rules.clear();
  m_ctl = ctl;
  m_alertColor = ctl ? ctl->alertColor() : QColor();
  m_display->setLimitTest(ctl && ctl->limitTest().enabled);
  showVerdict(ctl ? ctl->limitTest().value.id : 0, ctl ? ctl->verdict() : LimitTest::NoVerdict, std::nan(""));
  if (ctl)
  {
    m_link = connect(ctl, &MeterController::linkStateChanged, this, &FrontPanel::updateConnected);
    m_rules << connect(ctl, &MeterController::alertColorChanged, this, [this](const QColor &color)
    {
      m_alertColor = color;
      m_phase = true;
      showAlert();
    });
    m_rules << connect(ctl, &MeterController::limitJudged, this, &FrontPanel::showVerdict);
    m_rules << connect(ctl, &MeterController::deviceNameChanged, m_display, &LcdWidget::setCaption);
    m_rules << connect(ctl, &MeterController::limitTestChanged, this, [this](const LimitTest &test)
    {
      m_display->setLimitTest(test.enabled);
      if (PsuFrontPanel *psu = supplyPanel())
        for (int k = 0; k < psu->display()->channelCount(); ++k)
          psu->display()->setChannelVerdict(k, test.enabled && k == test.value.id);
      showLimitCount(m_ctl->limitCount());
    });
    m_rules << connect(ctl, &MeterController::limitCountChanged, this, &FrontPanel::showLimitCount);
  }
  showLimitCount(ctl ? ctl->limitCount() : LimitTest::Count());
  updateKeys();
}

void FrontPanel::setBlink(bool blink)
{
  m_blink = blink;
  showAlert();
}

void FrontPanel::showVerdict(int id, LimitTest::Verdict v, double deviation)
{
  m_display->setVerdict(id == m_display->primaryShown() ? v : LimitTest::NoVerdict, deviation);
  // a supply: in the block of that value
  if (PsuFrontPanel *psu = supplyPanel())
    psu->display()->setChannelVerdict(id, m_ctl && m_ctl->limitTest().enabled, v, deviation);
  const bool tint = m_ctl && m_ctl->limitTest().tint;
  m_limitColor = !tint || v == LimitTest::NoVerdict ? QColor()
                 : v == LimitTest::Pass ? QColor(0x3c, 0xb0, 0x4a) : QColor(0xd8, 0x22, 0x22);
  showAlert();
}

void FrontPanel::showLimitCount(const LimitTest::Count &count)
{
  // the readings judged, in the display's tooltip while the test is on
  if (!m_ctl || !m_ctl->limitTest().enabled)
  {
    m_display->setToolTip(QString());
    return;
  }
  const QLocale locale;
  const QString share = count.total() > 0 ? locale.toString(100.0 * count.fail() / count.total(), 'f', 1) : "-";
  m_display->setToolTip(tr("PASS %1 · FAIL %2 (%3 %)").arg(count.pass).arg(count.fail()).arg(share) + '\n'
                        + tr("Readings judged since the limits were set or min/max was reset: "
                             "%1 too high, %2 too low").arg(count.high).arg(count.low));
}

void FrontPanel::showAlert()
{
  const bool blinking = m_alertColor.isValid() && m_blink;
  if (blinking && !m_blinkTimer->isActive())
    m_blinkTimer->start();
  else if (!blinking)
  {
    m_blinkTimer->stop();
    m_phase = true;
  }
  // the alarm's colour over the limit test's; the colour is never the only
  // sign (banner, sound)
  const QColor color = m_alertColor.isValid() ? (m_phase ? m_alertColor : QColor()) : m_limitColor;
  m_display->setAlertColor(color);
  if (m_output)
    m_output->lcd()->setAlertColor(color);
}

PsuFrontPanel *FrontPanel::supplyPanel() const
{
  return qobject_cast<PsuFrontPanel *>(m_output);
}

GenFrontPanel *FrontPanel::generatorPanel() const
{
  return qobject_cast<GenFrontPanel *>(m_output);
}

void FrontPanel::followOutputs()
{
  PowerSupply *supply = nullptr;
  FunctionGenerator *generator = nullptr;
  if (m_ctl && m_ctl->isNode())
    if (NodeRunner *runner = NodeRunner::of(m_ctl))
    {
      supply = runner->node()->powerSupply();
      generator = runner->node()->functionGenerator();
    }
  QObject *device = supply ? static_cast<QObject *>(supply) : generator;
  if (m_output && m_outputDevice == device)
    return;
  delete m_output;
  m_output = nullptr;
  m_outputDevice = device;
  m_display->setVisible(!device);
  if (!device)
    return;
  if (supply)
  {
    m_output = new PsuFrontPanel(supply, this);
    // channels shown or not: the LCD's proportions and the controls' height change
    connect(supply, &PowerSupply::settingsChanged, m_output, [this] { Q_EMIT keysChanged(); });
  }
  else
  {
    m_output = new GenFrontPanel(generator, this);
    connect(generator, &FunctionGenerator::settingsChanged, m_output, [this] { Q_EMIT keysChanged(); });
    // another shape: a duty or symmetry knob comes or goes, the controls'
    // width may change (not on every turn of a knob)
    connect(generator, &FunctionGenerator::stateChanged, m_output,
            [this, generator, shapes = std::make_shared<QList<int>>()](int ch)
    {
      shapes->resize(qMax(int(shapes->size()), ch + 1), -1);
      const int shape = int(generator->state(ch).shape);
      if ((*shapes)[ch] == shape)
        return;
      (*shapes)[ch] = shape;
      Q_EMIT keysChanged();
    });
  }
  static_cast<QVBoxLayout *>(layout())->insertWidget(0, m_output, 1);
  m_output->setCaption(m_ctl->deviceName());
  connect(m_ctl, &MeterController::deviceNameChanged, m_output, &OutputFront::setCaption);
  connect(m_output, &OutputFront::settingsRequested, this, [this]
  {
    if (m_ctl)
      Q_EMIT settingsRequested(m_ctl->deviceId());
  });
  connect(m_output, &OutputFront::foldToggled, this, [this](bool folded)
  {
    Q_EMIT foldToggled(folded);
    Q_EMIT keysChanged();
  });
  m_output->lcd()->setContextMenuPolicy(Qt::CustomContextMenu);
  connect(m_output->lcd(), &QWidget::customContextMenuRequested, this, [this](const QPoint &pos)
  {
    Q_EMIT menuRequested(m_output->lcd()->mapToGlobal(pos));
  });
  syncLook();
  if (PsuFrontPanel *psu = supplyPanel())
  {
    const LimitTest test = m_ctl->limitTest();
    for (int k = 0; k < psu->display()->channelCount(); ++k)
      psu->display()->setChannelVerdict(k, test.enabled && k == test.value.id);
  }
  m_output->lcd()->setStale(m_display->isStale());
  showAlert();
}

void FrontPanel::syncLook()
{
  if (!m_output)
    return;
  m_output->lcd()->setFaceColor(m_display->customFaceColor());
  m_output->lcd()->setLcdVariant(m_display->lcdVariant());
}

double FrontPanel::minAspect() const
{
  return m_output ? m_output->lcd()->minimumAspect() : LcdWidget::kMinAspect;
}

double FrontPanel::maxAspect() const
{
  return m_output ? m_output->lcd()->maximumAspect() : LcdWidget::kMaxAspect;
}

void FrontPanel::updateKeys()
{
  const bool before = keysShown();
  followOutputs();
  if (m_output)
  {
    // a device's own front has its knobs, gear and fold itself
    m_keysSupported = true;
    m_controls->hide();
    m_nodeBar->hide();
    m_fold->hide();
    m_gear->hide();
    if (keysShown() != before)
      Q_EMIT keysChanged();
    return;
  }
  // a meter with keys QtDMM can press, or a logic node with its bar
  const bool node = m_ctl && m_ctl->isNode();
  m_keysSupported = node || (m_ctl && m_ctl->dmm() && ControlBar::supported(m_ctl->sourceInfo()));
  if (m_ctl && m_ctl->dmm())
    m_controls->setProtocol(m_ctl->dmm()->format(), m_ctl->sourceInfo().model);
  m_nodeBar->setController(node ? m_ctl.data() : nullptr);
  m_fold->setVisible(m_keysSupported);
  m_gear->setVisible(m_ctl);
  placeCorner();
  m_controls->setVisible(keysShown() && !node);
  m_nodeBar->setVisible(keysShown() && node);
  updateConnected();
  if (keysShown() != before)
    Q_EMIT keysChanged();
}

void FrontPanel::updateConnected()
{
  m_controls->setConnected(m_ctl && m_ctl->isWanted());
}

void FrontPanel::setFolded(bool folded)
{
  const bool before = keysShown();
  if (m_output)
  {
    m_output->setFolded(folded);
    if (keysShown() != before)
      Q_EMIT keysChanged();
    return;
  }
  m_fold->setFolded(folded);
  const bool node = m_ctl && m_ctl->isNode();
  m_controls->setVisible(keysShown() && !node);
  m_nodeBar->setVisible(keysShown() && node);
  if (keysShown() != before)
    Q_EMIT keysChanged();
}

bool FrontPanel::isFolded() const
{
  return m_output ? m_output->isFolded() : m_fold->isFolded();
}

bool FrontPanel::keysShown() const
{
  return m_keysSupported && !isFolded();
}

int FrontPanel::keysHeight() const
{
  if (m_output)
    return m_output->controlsHeight();
  // the keys while shown, the handle under them while there are any
  return (keysShown() ? bar()->sizeHint().height() + 4 : 0) + (m_keysSupported ? m_fold->height() + 4 : 0);
}

QWidget *FrontPanel::bar() const
{
  return m_ctl && m_ctl->isNode() ? static_cast<QWidget *>(m_nodeBar) : m_controls;
}

bool FrontPanel::eventFilter(QObject *watched, QEvent *event)
{
  if (watched == m_display && event->type() == QEvent::Resize)
    placeCorner();
  return QWidget::eventFilter(watched, event);
}

void FrontPanel::placeCorner()
{
  // at the right end of the name plate, as high as its letters; the name
  // keeps clear of it
  const QRectF band = m_display->plateBand();
  if (band.isEmpty())
  {
    m_gear->hide();
    return;
  }
  const int size = qBound(10, int(band.height() * 0.8), 18);
  m_gear->setFixedSize(size, size);
  m_gear->move(int(band.right() - size), int(band.center().y() - size / 2.0));
  m_display->setPlateReserve(size + 6);
}
