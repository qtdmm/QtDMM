// Copyright (c) 2026 The QtDMM developers
// SPDX-License-Identifier: GPL-3.0-or-later
#include "service/noderunner.h"

#include <cmath>

#include <QDateTime>

#include "core/devicelibrary.h"
#include "core/siprefix.h"
#include "service/devicesession.h"
#include "service/metercontroller.h"

namespace
{
// a number as an autoranging meter of @p counts shows it, with the prefix
// it takes (none for °C, %, ...): 40000 counts -> 3.9999 in the 4 range
QString shown(double value, const QString &baseUnit, int counts, QString *prefix)
{
  const double scaled = SiPrefix::scale(value, baseUnit, prefix);
  int digits = 0;
  for (int c = qMax(1, counts); c > 0; c /= 10)
    digits++;
  digits = qBound(3, digits, 9);
  const double limit = qMax(1000, counts);
  int decimals = digits - 1;
  while (decimals > 0 && std::round(std::fabs(scaled) * std::pow(10.0, decimals)) >= limit)
    decimals--;
  return QString::number(scaled, 'f', decimals);
}

// a number in a unit chosen with its prefix ("mWh"), as many decimals as
// @p counts allow for it
QString fixed(double value, int counts)
{
  int digits = 0;
  for (int c = qMax(1, counts); c > 0; c /= 10)
    digits++;
  digits = qBound(3, digits, 9);
  int decimals = digits - 1;
  for (double a = std::fabs(value); a >= 10 && decimals > 0; a /= 10)
    decimals--;
  return QString::number(value, 'f', decimals);
}
}

NodeRunner::NodeRunner(DeviceSession *session, MeterController *ctl, std::unique_ptr<LogicNode> node)
  : QObject(ctl), m_session(session), m_ctl(ctl), m_node(std::move(node))
{
  setObjectName(QStringLiteral("NodeRunner"));
  m_clock.setTimerType(Qt::PreciseTimer);
  connect(&m_clock, &QTimer::timeout, this, [this] { compute(Sample::now(), QDateTime::currentMSecsSinceEpoch()); });
  // sources come and go: an input opened later is taken then
  connect(m_session, &DeviceSession::opened, this, [this] { if (m_running) bind(); });
  connect(m_session, &DeviceSession::closed, this, [this] { if (m_running) bind(); });
  connect(m_ctl, &MeterController::linkStateChanged, this, &NodeRunner::follow);
  follow();
}

NodeRunner::~NodeRunner() = default;

QString NodeRunner::inputName(const ValueRef &ref) const
{
  // the entry's name in My devices ("Load I"), with the value for a node of
  // single values; the source when it is not there
  QString name = ref.source;
  if (const DeviceLibrary *library = m_session->library(); library && ref.source.startsWith(QLatin1String("device:")))
    if (const std::optional<MyDevice> d = library->find(ref.source.mid(7)))
      name = d->name;
  return m_node->takesDevices() || ref.id == 0 ? name : tr("%1, value %2").arg(name).arg(ref.id + 1);
}

NodeRunner *NodeRunner::of(const MeterController *ctl)
{
  return ctl && ctl->isNode() ? ctl->findChild<NodeRunner *>(QStringLiteral("NodeRunner"), Qt::FindDirectChildrenOnly)
                              : nullptr;
}

void NodeRunner::setOn(bool on)
{
  m_node->setOn(on);
}

void NodeRunner::reset()
{
  m_node->reset();
}

void NodeRunner::follow()
{
  const bool want = m_ctl->isWanted();
  if (want == m_running)
    return;
  m_running = want;
  if (want)
  {
    bind();
    if (m_node->periodMs() > 0)
      m_clock.start(m_node->periodMs());
  }
  else
  {
    m_clock.stop();
    for (const QMetaObject::Connection &c : std::as_const(m_links))
      disconnect(c);
    m_links.clear();
  }
}

void NodeRunner::bind()
{
  for (const QMetaObject::Connection &c : std::as_const(m_links))
    disconnect(c);
  m_links.clear();
  const QList<ValueRef> inputs = m_node->inputs();
  m_last.resize(inputs.size());
  m_values.resize(inputs.size());
  const bool devices = m_node->takesDevices();
  QStringList missing;
  for (int i = 0; i < inputs.size(); ++i)
  {
    const ValueRef ref = inputs[i];
    MeterController *from = ref.source == m_ctl->source() ? nullptr : m_session->controller(ref.source);
    if (!from)
    {
      m_last[i].reset();
      m_values[i].clear();
      missing << inputName(ref);
      continue;
    }
    m_links << connect(from, &MeterController::reading, this, [this, i, ref, devices](const Reading &r)
    {
      if (devices ? r.source == ref.source : ref.matches(r))
        onReading(i, r);
    });
  }
  // waiting for an input that is not open; otherwise running, until the
  // first computation tells more
  if (!missing.isEmpty())
    m_ctl->setNodeState(MeterConnection::LinkState::Timeout, tr("Input not open: %1").arg(missing.join(", ")));
  else
    m_ctl->setNodeState(MeterConnection::LinkState::Connecting);
}

void NodeRunner::onReading(int input, const Reading &r)
{
  if (input < 0 || input >= m_last.size())
    return;
  // the leading input triggers, at its time; a node on a clock computes on that
  const bool leads = input == m_node->leadingInput() && m_node->periodMs() == 0;
  if (!m_node->takesDevices())
  {
    m_last[input] = r;
    if (leads)
      compute(r.t, r.msecs);
    return;
  }
  // a device: all its values; its main value triggers, once the others of
  // its frame are in (they come right after it, in the same delivery)
  m_values[input][r.id] = r;
  if (r.id != 0)
    return;
  m_last[input] = r;
  if (leads)
    QTimer::singleShot(0, this, [this, t = r.t, wall = r.msecs] { compute(t, wall); });
}

void NodeRunner::compute(qint64 t, qint64 wall)
{
  if (!m_running)
    return;
  LogicNode::Context ctx;
  ctx.t = t;
  ctx.wall = wall;
  const QList<ValueRef> inputs = m_node->inputs();
  const bool devices = m_node->takesDevices();
  QStringList old;
  for (int i = 0; i < inputs.size(); ++i)
  {
    if (devices)
    {
      // each value of the device, if young enough
      QMap<int, Reading> young;
      const QMap<int, Reading> all = m_values.value(i);
      for (auto it = all.cbegin(); it != all.cend(); ++it)
        if (t - it->t <= m_node->maxAgeMs())
          young.insert(it.key(), it.value());
      ctx.values << young;
    }
    const std::optional<Reading> &r = m_last.value(i);
    // the others' last reading, if young enough beside this moment
    if (r && t - r->t <= m_node->maxAgeMs())
      ctx.inputs << r;
    else
    {
      ctx.inputs << std::nullopt;
      old << inputName(inputs[i]);
    }
  }
  const LogicNode::Values values = m_node->isOn() ? m_node->evaluate(ctx) : m_node->bypass(ctx);
  // what is wrong: an input old, else what the node says (a unit that does not fit)
  if (!old.isEmpty())
    m_ctl->setNodeState(MeterConnection::LinkState::Timeout, tr("No current value: %1").arg(old.join(", ")));
  else if (const QString problem = m_node->problem(); !problem.isEmpty())
    m_ctl->setNodeState(MeterConnection::LinkState::Timeout, problem);
  else
    m_ctl->setNodeState(MeterConnection::LinkState::Connected);
  deliver(values, t, wall);
}

void NodeRunner::deliver(const LogicNode::Values &values, qint64 t, qint64 wall)
{
  const QList<LogicNode::Output> outputs = m_node->outputs();
  DmmDecoder::DmmResponse response;
  response.channels.clear();
  response.received = t;
  response.receivedWall = wall;
  const int counts = m_ctl->sourceInfo().display > 0 ? m_ctl->sourceInfo().display : 60000;
  for (int i = 0; i < outputs.size() && i < kMaxValues; ++i)
  {
    const LogicNode::Output &out = outputs[i];
    if (out.kind != LogicNode::OutputKind::Value)
      continue;   // events: none in 27.1
    DmmDecoder::DmmChannel c;
    c.id = i;
    c.name = out.name;
    c.special = out.coupling;
    const QString outUnit = LogicNode::canonicalUnit(out.unit);
    const std::optional<double> v = values.value(i);
    if (v && std::isfinite(*v))
    {
      // the node's number is in its unit as given ("mA": 12 is 12 mA);
      // a unit with a prefix is shown in it, one without as an
      // autoranging meter would
      const SiPrefix::Split unit = SiPrefix::split(outUnit);
      c.dval = *v * SiPrefix::factor(unit.prefix);
      QString prefix;
      c.val = unit.prefix.isEmpty() ? shown(c.dval, unit.baseUnit, counts, &prefix) : fixed(*v, counts);
      c.unit = (unit.prefix.isEmpty() ? prefix : unit.prefix) + unit.baseUnit;
    }
    else
    {
      // no value: a gap, not the last one
      c.noValue = true;
      c.val = QStringLiteral("----");
      c.unit = outUnit;
    }
    response.channels << c;
  }
  if (!response.channels.isEmpty())
    m_ctl->deliver(response);
}
