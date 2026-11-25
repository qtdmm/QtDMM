// Copyright (c) 2026 The QtDMM developers
// SPDX-License-Identifier: GPL-3.0-or-later
//
// SimPsuNode: the ideal supply (setpoint, output off = 0 V), the limits,
// the worse supply (rise and fall time, overshoot, ripple band, drift),
// channels shown and not, stored and loaded (the outputs start off), the
// node as made by its type; SimPsuNodeEditor there and back.

#include <QApplication>
#include <QSpinBox>

#include <cmath>

#include "core/powersupply.h"
#include "core/sampletypes.h"
#include "logic/simpsunode.h"
#include "ui/dialogs/simpsunodedlg.h"
#include "testutil.h"

namespace
{
/// The node's values at @p t (core clock).
LogicNode::Values at(SimPsuNode &node, qint64 t)
{
  LogicNode::Context ctx;
  ctx.t = t;
  return node.evaluate(ctx);
}

double first(SimPsuNode &node, qint64 t)
{
  const LogicNode::Values v = at(node, t);
  return v.isEmpty() || !v.first() ? std::nan("") : *v.first();
}

bool near(double a, double b, double tolerance)
{
  return std::abs(a - b) <= tolerance;
}
}

int main(int argc, char **argv)
{
  if (qEnvironmentVariableIsEmpty("QT_QPA_PLATFORM"))
    qputenv("QT_QPA_PLATFORM", "offscreen");
  QApplication app(argc, argv);

  // made by its type, one channel
  {
    std::unique_ptr<LogicNode> made = LogicNode::create(SimPsuNode::kType);
    check(made && made->powerSupply() && made->periodMs() == SimPsuNode::kDefaultPeriodMs,
          "made by its type: a supply, on the generator's clock");
    const QList<LogicNode::Output> out = made->outputs();
    check(out.size() == 1 && out[0].name == "CH1 U" && out[0].unit == "V", "one channel: CH1 U in V");
  }

  // the ideal supply
  {
    SimPsuNode node;
    PowerSupply *psu = node.powerSupply();
    psu->requestVoltage(0, 5);
    check(node.setpoint(0) == 5 && psu->state(0).setState == PowerSupply::SetState::Confirmed
            && psu->state(0).setpoint == 5,
          "set: taken and read back at once");
    check(first(node, Sample::now()) == 0, "output off: 0 V");
    psu->requestOutput(0, true);
    check(psu->state(0).output && first(node, Sample::now()) == 5, "output on: the setpoint, exactly");
    check(psu->state(0).u == 5, "the front gets the measured value");
    psu->requestVoltage(0, 50);
    check(node.setpoint(0) == SimPsuNode::kMaxVoltage, "above the device's 30 V: 30 V");
    PowerSupply::ChannelSettings limits;
    limits.uMax = 12;
    psu->setSettings(0, limits);
    psu->requestVoltage(0, 20);
    check(node.setpoint(0) == 12, "above the channel's highest: 12 V");
  }

  // rise and fall
  {
    SimPsuNode node;
    SimPsuNode::Faults f;
    f.riseMs = 1000;
    f.fallMs = 400;
    node.setFaults(f);
    PowerSupply *psu = node.powerSupply();
    psu->requestVoltage(0, 10);
    const qint64 t0 = Sample::now();
    psu->requestOutput(0, true);
    check(near(first(node, t0 + 500), 5, 0.3), "rise: half way after half the rise time");
    check(first(node, t0 + 1200) == 10, "rise: the setpoint after the rise time");
    // switched off: on the node's clock, after its last computation at t0 + 1200
    const qint64 t1 = t0 + 1200;
    psu->requestOutput(0, false);
    const double mid = first(node, t1 + 200);
    check(mid > 3 && mid < 7, QString("fall: half way after half the fall time, %1 V").arg(mid));
    check(first(node, t1 + 500) == 0, "fall: 0 V after the fall time");
  }

  // overshoot
  {
    SimPsuNode node;
    SimPsuNode::Faults f;
    f.riseMs = 1000;
    f.overshoot = 10;
    node.setFaults(f);
    node.powerSupply()->requestVoltage(0, 10);
    const qint64 t0 = Sample::now();
    node.powerSupply()->requestOutput(0, true);
    check(near(first(node, t0 + 1000), 11, 0.15), "overshoot: 10 % above at the end of the rise");
    check(near(first(node, t0 + 3000), 10, 0.05), "then back to the setpoint");
  }

  // ripple band and drift
  {
    SimPsuNode node;
    SimPsuNode::Faults f;
    f.rippleMvpp = 100;
    node.setFaults(f);
    node.powerSupply()->requestVoltage(0, 5);
    node.powerSupply()->requestOutput(0, true);
    double lo = 99, hi = -99;
    const qint64 t = Sample::now();
    for (int i = 0; i < 400; ++i)
    {
      const double v = first(node, t + i);
      lo = qMin(lo, v);
      hi = qMax(hi, v);
    }
    check(lo >= 4.95 - 1e-9 && hi <= 5.05 + 1e-9 && hi - lo > 0.08,
          QString("ripple: a band of 100 mVpp around 5 V, %1..%2").arg(lo).arg(hi));
    node.powerSupply()->requestOutput(0, false);
    check(first(node, t + 500) == 0, "ripple only while the output is on");
  }
  {
    SimPsuNode node;
    SimPsuNode::Faults f;
    f.driftMvPerMin = 60;
    node.setFaults(f);
    node.powerSupply()->requestVoltage(0, 5);
    const qint64 t0 = Sample::now();
    node.powerSupply()->requestOutput(0, true);
    check(near(first(node, t0 + 60000), 5.06, 0.001), "drift: 60 mV after a minute on");
  }

  // channels shown
  {
    SimPsuNode node;
    node.setChannels(3);
    PowerSupply *psu = node.powerSupply();
    PowerSupply::ChannelSettings hidden;
    hidden.shown = false;
    psu->setSettings(1, hidden);
    const QList<LogicNode::Output> out = node.outputs();
    check(out.size() == 2 && out[0].name == "CH1 U" && out[1].name == "CH3 U", "a channel not shown: no value of it");
    psu->requestVoltage(2, 7);
    psu->requestOutput(2, true);
    const LogicNode::Values v = at(node, Sample::now());
    check(v.size() == 2 && v[1] && *v[1] == 7, "the values of the channels shown, in order");
    check(!psu->requestOutput(1, true) && !node.output(1), "a channel not shown: never switched");
  }

  // a changed bench: setpoints and HOLD are none, the channels are
  {
    SimPsuNode node;
    const QString setUp = node.setUpText();
    node.powerSupply()->requestVoltage(0, 7);
    node.setOn(!node.isOn());
    check(node.setUpText() == setUp, "set-up: a setpoint turned is no change");
    node.setChannels(2);
    check(node.setUpText() != setUp, "set-up: the channels are");
  }

  // stored and loaded
  {
    SimPsuNode node;
    node.setChannels(2);
    SimPsuNode::Faults f;
    f.riseMs = 200;
    f.rippleMvpp = 20;
    f.driftMvPerMin = -3;
    node.setFaults(f);
    PowerSupply *psu = node.powerSupply();
    psu->requestVoltage(0, 3.3);
    psu->requestVoltage(1, 12);
    psu->requestOutput(1, true);
    PowerSupply::ChannelSettings s;
    s.color = QColor(0x11, 0x22, 0x33);
    s.uMax = 15;
    psu->setSettings(1, s);
    std::unique_ptr<LogicNode> back = LogicNode::fromJsonText(node.toJsonText());
    auto *loaded = dynamic_cast<SimPsuNode *>(back.get());
    check(loaded && loaded->channels() == 2 && loaded->setpoint(0) == 3.3 && loaded->setpoint(1) == 12
            && loaded->faults() == f && loaded->powerSupply()->settings(1) == s,
          "stored: channels, setpoints, faults, the channels' settings");
    check(loaded && !loaded->output(1) && !loaded->powerSupply()->state(1).output
            && loaded->powerSupply()->state(1).setpoint == 12,
          "loaded: the outputs off, the setpoint on the front");
  }

  // the editor
  {
    SimPsuNode node;
    node.setChannels(2);
    node.powerSupply()->requestVoltage(1, 9);
    SimPsuNode::Faults f;
    f.noiseMv = 5;
    node.setFaults(f);
    SimPsuNodeEditor editor;
    editor.load("Bench", node);
    check(editor.valid() && editor.name() == "Bench", "editor: loaded, valid");
    auto *count = editor.findChild<QSpinBox *>("ui_simPsuChannels");
    count->setValue(4);
    const std::unique_ptr<SimPsuNode> made = editor.node();
    check(made->channels() == 4 && made->setpoint(1) == 9 && made->faults() == f && made->outputs().size() == 4,
          "editor: four channels, the setpoint kept, the faults as set");
    check(editor.model() == "QtDMM Power supply", "its model");
  }

  if (failed == 0)
    qInfo() << "All simulated power supply tests passed.";
  return failed == 0 ? 0 : 1;
}
