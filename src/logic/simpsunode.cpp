// Copyright (c) 2026 The QtDMM developers
// SPDX-License-Identifier: GPL-3.0-or-later
#include "logic/simpsunode.h"

#include <QJsonArray>
#include <QRandomGenerator>

#include <cmath>

#include "core/powersupply.h"
#include "core/sampletypes.h"

/// The PowerSupply the front works on: requests go to the node, which takes
/// them at once (a simulation reads back what it was told).
class SimPowerSupply : public PowerSupply
{
public:
  explicit SimPowerSupply(SimPsuNode *node) : m_node(node) {}

  int channelCount() const override { return m_node->channels(); }
  ChannelInfo channel(int ch) const override
  {
    ChannelInfo info;
    info.name = QStringLiteral("CH%1").arg(ch + 1);
    info.uMin = 0;
    info.uMax = SimPsuNode::kMaxVoltage;
    info.uDecimals = SimPsuNode::kDecimals;
    info.caps = SetVoltage;
    return info;
  }
  /// The channels changed: states and settings follow, the setpoints as
  /// read back, the outputs as they are.
  void follow()
  {
    channelsChanged();
    for (int ch = 0; ch < channelCount(); ++ch)
    {
      confirmVoltage(ch, m_node->setpoint(ch));
      confirmOutput(ch, m_node->output(ch));
    }
  }
  void measured(int ch, double u) { setMeasured(ch, u); }

protected:
  void sendVoltage(int ch, double u) override
  {
    m_node->setSetpoint(ch, u);
    confirmVoltage(ch, m_node->setpoint(ch));
  }
  void sendOutput(int ch, bool on) override
  {
    m_node->switchOutput(ch, on);
    confirmOutput(ch, m_node->output(ch));
  }

private:
  SimPsuNode *m_node;
};

// ---------------------------------------------------------------- faults

QJsonObject SimPsuNode::Faults::toJson() const
{
  return QJsonObject{ { "rise", riseMs },         { "fall", fallMs },      { "overshoot", overshoot },
                      { "ripple", rippleMvpp },   { "noise", noiseMv },    { "drift", driftMvPerMin } };
}

SimPsuNode::Faults SimPsuNode::Faults::fromJson(const QJsonObject &o)
{
  // none negative: a fall time of -1 s is no supply
  auto get = [&o](const char *key) { return qMax(0.0, o.value(key).toDouble(0)); };
  Faults f;
  f.riseMs = get("rise");
  f.fallMs = get("fall");
  f.overshoot = get("overshoot");
  f.rippleMvpp = get("ripple");
  f.noiseMv = get("noise");
  f.driftMvPerMin = o.value("drift").toDouble(0);   // a supply may drift down
  return f;
}

// ---------------------------------------------------------------- the node

SimPsuNode::SimPsuNode() : m_supply(std::make_unique<SimPowerSupply>(this))
{
  setPeriodMs(kDefaultPeriodMs);
  setChannels(1);
}

SimPsuNode::~SimPsuNode() = default;

PowerSupply *SimPsuNode::powerSupply()
{
  return m_supply.get();
}

void SimPsuNode::setChannels(int count)
{
  m_ch.resize(qBound(1, count, kMaxChannels));
  m_supply->follow();
}

double SimPsuNode::setpoint(int ch) const
{
  return ch >= 0 && ch < channels() ? m_ch[ch].setpoint : PowerSupply::kNone;
}

void SimPsuNode::setSetpoint(int ch, double u)
{
  if (ch < 0 || ch >= channels() || std::isnan(u))
    return;
  // by division: 3.3 stays 3.3, not 3.3000000000000003
  const double scale = std::pow(10.0, kDecimals);
  m_ch[ch].setpoint = qBound(0.0, std::round(u * scale) / scale, kMaxVoltage);
  retarget(m_ch[ch], qMax(Sample::now(), m_now));
}

bool SimPsuNode::output(int ch) const
{
  return ch >= 0 && ch < channels() && m_ch[ch].out;
}

void SimPsuNode::switchOutput(int ch, bool on)
{
  if (ch < 0 || ch >= channels() || m_ch[ch].out == on)
    return;
  // the clock of the computations: never behind the last one
  const qint64 t = qMax(Sample::now(), m_now);
  m_ch[ch].out = on;
  m_ch[ch].onSince = on ? t : -1;
  retarget(m_ch[ch], t);
}

double SimPsuNode::voltage(int ch) const
{
  return ch >= 0 && ch < channels() ? m_ch[ch].shown : PowerSupply::kNone;
}

void SimPsuNode::reset()
{
  // the steps under way done
  for (Channel &c : m_ch)
  {
    c.to = c.out ? c.setpoint : 0;
    c.from = c.to;
    c.level = c.to;
    c.since = -1;
  }
}

// ---------------------------------------------------------------- the model

void SimPsuNode::retarget(Channel &c, qint64 t)
{
  const double target = c.out ? c.setpoint : 0;
  if (target == c.to)
    return;
  c.from = levelAt(c, t);
  c.to = target;
  c.rising = c.to > c.from;
  c.duration = c.rising ? m_faults.riseMs : m_faults.fallMs;
  c.since = t;
}

double SimPsuNode::levelAt(Channel &c, qint64 t) const
{
  if (c.since < 0 || c.duration <= 0)
    return c.level = c.to;
  const double f = double(t - c.since) / c.duration;
  const double step = c.to - c.from;
  const double over = c.rising ? m_faults.overshoot / 100 : 0;
  // up: a straight rise to the overshoot's top, then back to the setpoint
  // (e^-3 per rise time); down: straight to the target
  if (f < 1)
    c.level = c.from + step * (1 + over) * qMax(0.0, f);
  else
    c.level = c.to + step * over * std::exp(-3 * (f - 1));
  return c.level;
}

QList<LogicNode::Output> SimPsuNode::outputs() const
{
  QList<Output> list;
  for (int ch : m_supply->shownChannels())
  {
    Output o;
    o.name = QStringLiteral("CH%1 U").arg(ch + 1);
    o.unit = QStringLiteral("V");
    o.coupling = QStringLiteral("DC");
    list << o;
  }
  return list;
}

LogicNode::Values SimPsuNode::evaluate(const Context &ctx)
{
  m_now = ctx.t;
  QRandomGenerator *random = QRandomGenerator::global();
  Values values;
  for (int ch = 0; ch < channels(); ++ch)
  {
    Channel &c = m_ch[ch];
    double v = levelAt(c, ctx.t);
    // what a worse supply adds while it drives the output
    if (c.out)
    {
      const double band = m_faults.rippleMvpp / 1000;
      const double noise = m_faults.noiseMv / 1000;
      v += band * (random->generateDouble() - 0.5);
      v += noise * (2 * random->generateDouble() - 1);
      if (c.onSince >= 0)
        v += m_faults.driftMvPerMin / 1000 * double(ctx.t - c.onSince) / 60000;
    }
    c.shown = v;
    m_supply->measured(ch, v);
    if (m_supply->settings(ch).shown)
      values << v;
  }
  return values;
}

// ---------------------------------------------------------------- stored

QJsonObject SimPsuNode::parameters() const
{
  QJsonArray setpoints;
  for (const Channel &c : m_ch)
    setpoints.append(c.setpoint);
  return QJsonObject{ { "channels", channels() },
                      { "setpoints", setpoints },
                      { "faults", m_faults.toJson() },
                      { "psu", m_supply->settingsJson() } };
}

void SimPsuNode::setParameters(const QJsonObject &p)
{
  m_faults = Faults::fromJson(p.value("faults").toObject());
  m_ch = QList<Channel>(qBound(1, p.value("channels").toInt(1), kMaxChannels));
  // the outputs always start off
  const QJsonArray setpoints = p.value("setpoints").toArray();
  for (int ch = 0; ch < channels() && ch < setpoints.size(); ++ch)
    m_ch[ch].setpoint = qBound(0.0, setpoints[ch].toDouble(0), kMaxVoltage);
  m_supply->follow();
  m_supply->setSettingsJson(p.value("psu").toObject());
}
