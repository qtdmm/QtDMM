// Copyright (c) 2026 The QtDMM developers
// SPDX-License-Identifier: GPL-3.0-or-later
#include "service/noderunner.h"

#include <cmath>

#include <QDateTime>
#include <QJsonDocument>
#include <QJsonObject>

#include "core/devicelibrary.h"
#include "core/functiongenerator.h"
#include "core/outputregistry.h"
#include "core/powersupply.h"
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
  connect(m_session, &DeviceSession::opened, this, [this](const MeterController *ctl)
  {
    if (m_running && ctl && takes(ctl->source()))
      bind();
  });
  connect(m_session, &DeviceSession::closed, this, [this](const QString &source)
  {
    if (m_running && takes(source))
      bind();
  });
  connect(m_ctl, &MeterController::linkStateChanged, this, &NodeRunner::follow);
  // a simulated supply's setpoints and channel settings with its entry, a
  // moment after the knob stopped
  if (PowerSupply *supply = m_node->powerSupply())
  {
    // open now: the emergency off reaches it
    OutputRegistry::instance().add(supply);
    m_store.setSingleShot(true);
    m_store.setInterval(500);
    connect(&m_store, &QTimer::timeout, this, &NodeRunner::store);
    connect(supply, &PowerSupply::setpointChanged, &m_store, qOverload<>(&QTimer::start));
    connect(supply, &PowerSupply::settingsChanged, &m_store, qOverload<>(&QTimer::start));
  }
  // a simulated generator likewise: what it is set to with its entry
  if (FunctionGenerator *generator = m_node->functionGenerator())
  {
    OutputRegistry::instance().add(generator);
    m_store.setSingleShot(true);
    m_store.setInterval(500);
    connect(&m_store, &QTimer::timeout, this, &NodeRunner::store, Qt::UniqueConnection);
    connect(generator, &FunctionGenerator::setpointChanged, &m_store, qOverload<>(&QTimer::start));
    connect(generator, &FunctionGenerator::settingsChanged, &m_store, qOverload<>(&QTimer::start));
  }
  follow();
}

void NodeRunner::store()
{
  DeviceLibrary *library = m_session->library();
  if (!library)
    return;
  const std::optional<MyDevice> entry = library->find(m_ctl->deviceId());
  if (!entry || !entry->isNode())
    return;
  const QString json = m_node->toJsonText();
  if (entry->keys.value("Node/json").toString() == json)
    return;
  QVariantMap keys = entry->keys;
  keys.insert("Node/json", json);
  library->update(m_ctl->deviceId(), keys);
}

NodeRunner::~NodeRunner()
{
  // closed: no longer a device with outputs
  if (PowerSupply *supply = m_node->powerSupply())
    OutputRegistry::instance().remove(supply);
  if (FunctionGenerator *generator = m_node->functionGenerator())
    OutputRegistry::instance().remove(generator);
}

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

void NodeRunner::press(const QString &id)
{
  const bool wasOn = m_node->isOn();
  m_node->press(id);
  // HOLD with the entry: the next start holds too
  DeviceLibrary *library = m_session->library();
  if (m_node->isOn() != wasOn && library)
    if (const std::optional<MyDevice> entry = library->find(m_ctl->deviceId()); entry && entry->isNode())
    {
      QJsonObject json = QJsonDocument::fromJson(entry->keys.value("Node/json").toString().toUtf8()).object();
      json["on"] = m_node->isOn();
      QVariantMap keys = entry->keys;
      keys.insert("Node/json", QString::fromUtf8(QJsonDocument(json).toJson(QJsonDocument::Compact)));
      library->update(m_ctl->deviceId(), keys);
    }
  Q_EMIT controlsChanged();
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
  m_pending.reset();
  const bool devices = m_node->takesDevices();
  // the inputs that are nodes computed from the leading device itself: their
  // result of its frame is waited for, not taken from the frame before
  m_sameFrame.clear();
  const int lead = m_node->leadingInput();
  if (devices && m_node->periodMs() == 0 && lead >= 0 && lead < inputs.size())
    for (int i = 0; i < inputs.size(); ++i)
      if (i != lead && dependsOn(inputs[i].source, inputs[lead].source))
        m_sameFrame << i;
  for (int i = 0; i < inputs.size(); ++i)
  {
    const ValueRef ref = inputs[i];
    MeterController *from = ref.source == m_ctl->source() ? nullptr : m_session->controller(ref.source);
    if (!from)
    {
      m_last[i].reset();
      m_values[i].clear();
      continue;
    }
    // an input that stops delivering (it waits itself): said here too
    m_links << connect(from, &MeterController::linkStateChanged, this, [this, from]
    {
      if (from->linkState() != MeterConnection::LinkState::Connected)
        showWaiting();
    });
    m_links << connect(from, &MeterController::reading, this, [this, i, ref, devices](const Reading &r)
    {
      if (devices ? r.source == ref.source : ref.matches(r))
        onReading(i, r);
    });
  }
  showWaiting();
}

void NodeRunner::showWaiting()
{
  // an input not open; else one open but not delivering, with its reason;
  // otherwise running, until the first computation tells more
  QStringList missing;
  QStringList waiting;
  for (const ValueRef &ref : m_node->inputs())
  {
    const MeterController *from = ref.source == m_ctl->source() ? nullptr : m_session->controller(ref.source);
    if (!from)
      missing << inputName(ref);
    else if (from->linkState() != MeterConnection::LinkState::Connected)
    {
      // a meter's reason is long (its port, the system's error, a hint):
      // in its own panel and log; here its state in a word. A node's is
      // its own chain: in its bar's tooltip, here just its name.
      const QString why = from->isNode() ? QString() : from->shortState();
      waiting << (why.isEmpty() ? inputName(ref) : tr("%1: %2").arg(inputName(ref), why));
    }
  }
  if (!missing.isEmpty())
    m_ctl->setNodeState(MeterConnection::LinkState::Timeout, tr("Input not open: %1").arg(missing.join(", ")));
  else if (!waiting.isEmpty())
    m_ctl->setNodeState(MeterConnection::LinkState::Timeout, tr("Waiting for %1").arg(waiting.join("; ")));
  else if (m_ctl->linkState() != MeterConnection::LinkState::Connected)
    m_ctl->setNodeState(MeterConnection::LinkState::Connecting);
}

bool NodeRunner::takes(const QString &source) const
{
  for (const ValueRef &ref : m_node->inputs())
    if (ref.source == source)
      return true;
  return false;
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
    QTimer::singleShot(0, this, [this, t = r.t, wall = r.msecs] { computeFrame(t, wall); });
  else if (m_pending && r.t == m_pending->first && m_sameFrame.contains(input))
    QTimer::singleShot(0, this, [this, t = r.t]
    {
      // its other values are in now too; the frame it was waited for
      if (m_pending && m_pending->first == t && frameComplete(t))
      {
        const qint64 wall = m_pending->second;
        m_pending.reset();
        compute(t, wall);
      }
    });
}

void NodeRunner::computeFrame(qint64 t, qint64 wall)
{
  // a frame still waiting when the next comes: computed with what there is
  if (m_pending && m_pending->first != t)
  {
    const auto [pt, pwall] = *m_pending;
    m_pending.reset();
    compute(pt, pwall);
  }
  if (frameComplete(t))
  {
    m_pending.reset();
    compute(t, wall);
  }
  else
    m_pending = std::pair(t, wall);
}

bool NodeRunner::frameComplete(qint64 t) const
{
  for (int i : m_sameFrame)
    if (!m_last.value(i) || m_last.value(i)->t != t)
      return false;
  return true;
}

bool NodeRunner::dependsOn(const QString &source, const QString &device) const
{
  // through the nodes' inputs; a loop cannot be opened, the depth guards anyway
  QStringList todo { source };
  QStringList seen;
  while (!todo.isEmpty() && seen.size() < 64)
  {
    const QString s = todo.takeFirst();
    if (seen.contains(s))
      continue;
    seen << s;
    for (const ValueRef &ref : m_session->nodeInputs(s))
    {
      if (ref.source == device)
        return true;
      todo << ref.source;
    }
  }
  return false;
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
  // held (an integral's HOLD): the H on the display, as on a meter
  response.hold = m_node->holds() && !m_node->isOn();
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
