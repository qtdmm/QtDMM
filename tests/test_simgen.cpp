// Copyright (c) 2026 The QtDMM developers
// SPDX-License-Identifier: GPL-3.0-or-later
//
// SimGenNode: made by its type, the values a channel gives (chosen, at
// most four in all; by default Vrms, f, mean, u - with two channels Vrms
// and f each), what a true RMS meter reads of each shape (sine, square, pulse
// with its duty, ramp, an arbitrary one), output off, u(t) only for a slow
// signal, a shape lowering the frequency, the worse generator (frequency
// and amplitude error, cutoff), channels shown and not, stored and loaded
// (the outputs start off), SimGenNodeEditor there and back.

#include <QApplication>
#include <QCheckBox>
#include <QSpinBox>

#include <cmath>

#include "core/functiongenerator.h"
#include "core/sampletypes.h"
#include "logic/simgennode.h"
#include "ui/dialogs/simgennodedlg.h"
#include "testutil.h"

namespace
{
using Shape = FunctionGenerator::Shape;
using Value = FunctionGenerator::Value;

LogicNode::Values at(SimGenNode &node, qint64 t)
{
  LogicNode::Context ctx;
  ctx.t = t;
  return node.evaluate(ctx);
}

bool near(double a, double b, double tolerance)
{
  return std::abs(a - b) <= tolerance;
}

double value(const LogicNode::Values &v, int i)
{
  return i < v.size() && v[i] ? *v[i] : std::nan("");
}

// the values with Vrms, Vpp, f, mean given: 0 1 2 3; u alone at 0
enum { Rms, Pp, F, Mean };
const SimGenNode::Given kFour { true, true, true, true, false };
const SimGenNode::Given kNowOnly { false, false, false, false, true };
}

int main(int argc, char **argv)
{
  if (qEnvironmentVariableIsEmpty("QT_QPA_PLATFORM"))
    qputenv("QT_QPA_PLATFORM", "offscreen");
  QApplication app(argc, argv);

  // made by its type
  {
    std::unique_ptr<LogicNode> made = LogicNode::create(SimGenNode::kType);
    check(made && made->functionGenerator() && made->periodMs() == SimGenNode::kDefaultPeriodMs,
          "made by its type: a generator, on the clock of 250 ms");
    const QList<LogicNode::Output> out = made->outputs();
    check(out.size() == 4 && out[0].name == "CH1 Vrms" && out[0].coupling == "AC" && out[1].unit == "Hz"
            && out[2].name == "CH1 mean" && out[2].coupling == "DC" && out[3].name == "CH1 u",
          "one channel by default: Vrms, f, mean, u");
    auto *gen = dynamic_cast<SimGenNode *>(made.get());
    gen->setChannels(2);
    const QList<LogicNode::Output> two = gen->outputs();
    check(two.size() == 4 && two[0].name == "CH1 Vrms" && two[1].name == "CH1 f" && two[2].name == "CH2 Vrms"
            && two[3].name == "CH2 f",
          "two channels by default: Vrms and f of each");
    gen->setGiven(0, { true, true, true, true, true });
    check(gen->outputs().size() == kMaxValues && gen->outputs().last().name == "CH1 mean",
          "more than four chosen: the rest left out");
  }

  // what a meter reads of each shape
  {
    SimGenNode::Setting s;
    s.amplitude = 2;
    s.offset = 0.5;
    check(near(SimGenNode::levels(s).rms, std::sqrt(0.5), 1e-3) && near(SimGenNode::levels(s).mean, 0.5, 1e-6),
          "sine 2 Vpp: 0.707 V RMS, the offset as mean");
    s.shape = Shape::Square;
    s.offset = 0;
    check(near(SimGenNode::levels(s).rms, 1.0, 1e-3), "square 2 Vpp: 1 V RMS");
    s.shape = Shape::Pulse;
    s.duty = 25;
    const SimGenNode::Levels pulse = SimGenNode::levels(s);
    check(near(pulse.mean, -0.5, 1e-3) && near(pulse.rms, 2 * std::sqrt(0.25 * 0.75), 1e-3),
          QString("pulse 25 %: mean -0.5 V, RMS 0.866 V (%1, %2)").arg(pulse.mean).arg(pulse.rms));
    s.shape = Shape::Ramp;
    s.symmetry = 30;
    check(near(SimGenNode::levels(s).rms, 1 / std::sqrt(3.0), 1e-3), "ramp 2 Vpp, any symmetry: 0.577 V RMS");
    s.shape = Shape::Arbitrary;
    s.arbitrary = "Gauss";
    const SimGenNode::Levels gauss = SimGenNode::levels(s);
    check(gauss.mean < -0.5 && gauss.rms > 0.1 && gauss.rms < 0.7, "an arbitrary shape: a narrow bump, its own levels");
  }

  // the node's values, output off and on, slow and fast
  {
    SimGenNode node;
    node.setGiven(0, kFour);
    FunctionGenerator *gen = node.functionGenerator();
    gen->requestValue(0, Value::Amplitude, 2);
    LogicNode::Values v = at(node, Sample::now());
    check(value(v, Rms) == 0 && value(v, Pp) == 0 && value(v, F) == 1000 && value(v, Mean) == 0,
          "output off: 0 V, the frequency stays");
    check(gen->requestOutput(0, true) && node.output(0), "output on");
    v = at(node, Sample::now());
    check(near(value(v, Rms), std::sqrt(0.5), 1e-3) && value(v, Pp) == 2, "on: 0.707 V RMS, 2 Vpp");
    node.setGiven(0, kNowOnly);
    v = at(node, Sample::now());
    check(v.size() == 1 && !v[0], "u at 1 kHz: no value");
    gen->requestValue(0, Value::Frequency, 0.25);
    v = at(node, 1000);   // a quarter period of 0.25 Hz: the top of the sine
    check(v.size() == 1 && v[0] && near(*v[0], 1.0, 1e-6), "0.25 Hz: u(t) given, the curve's top after a quarter period");
    gen->requestValue(0, Value::Frequency, 10e6);
    gen->requestShape(0, Shape::Square);
    check(gen->state(0).get(Value::Frequency) == 5e6 && node.setting(0).frequency == 5e6,
          "a square: 10 MHz lowered to its 5 MHz");
  }

  // a worse generator
  {
    SimGenNode node;
    node.setGiven(0, kFour);
    SimGenNode::Faults f;
    f.frequencyPpm = 100;
    f.amplitudePct = 10;
    node.setFaults(f);
    node.functionGenerator()->requestValue(0, Value::Amplitude, 2);
    node.functionGenerator()->requestOutput(0, true);
    LogicNode::Values v = at(node, Sample::now());
    check(near(value(v, F), 1000.1, 1e-6) && near(value(v, Pp), 2.2, 1e-9), "100 ppm and 10 %: 1000.1 Hz, 2.2 Vpp");
    f = SimGenNode::Faults();
    f.cutoffHz = 1000;
    node.setFaults(f);
    v = at(node, Sample::now());
    check(near(value(v, Rms), std::sqrt(0.5) / std::sqrt(2.0), 1e-3), "at the cutoff: -3 dB");
  }

  // channels shown
  {
    SimGenNode node;
    node.setChannels(2);
    FunctionGenerator::ChannelSettings hidden;
    hidden.shown = false;
    node.functionGenerator()->setSettings(0, hidden);
    const QList<LogicNode::Output> out = node.outputs();
    check(out.size() == 2 && out[0].name == "CH2 Vrms", "a channel not shown: no values of it");
    check(!node.functionGenerator()->requestOutput(0, true), "and it is never switched");
  }

  // stored and loaded
  {
    SimGenNode node;
    node.setChannels(2);
    SimGenNode::Faults f;
    f.noiseMv = 3;
    node.setFaults(f);
    FunctionGenerator *gen = node.functionGenerator();
    gen->requestShape(1, Shape::Pulse);
    gen->requestValue(1, Value::Frequency, 2500);
    gen->requestValue(1, Value::Duty, 20);
    gen->requestOutput(1, true);
    gen->requestShape(0, Shape::Arbitrary, "StairUp");
    FunctionGenerator::ChannelSettings s;
    s.ampMax = 1.5;
    gen->setSettings(1, s);
    node.setGiven(1, { true, true, false, false, false });
    std::unique_ptr<LogicNode> back = LogicNode::fromJsonText(node.toJsonText());
    auto *loaded = dynamic_cast<SimGenNode *>(back.get());
    check(loaded && loaded->channels() == 2 && loaded->setting(1) == node.setting(1) && loaded->setting(0).arbitrary == "StairUp"
            && loaded->faults() == f && loaded->functionGenerator()->settings(1) == s
            && loaded->given(1) == node.given(1) && loaded->given(0) == SimGenNode::defaultGiven(2),
          "stored: channels, settings, faults, the channels' settings, what each gives");
    check(loaded && !loaded->output(1) && !loaded->functionGenerator()->state(1).output
            && loaded->functionGenerator()->state(1).get(Value::Duty) == 20,
          "loaded: the outputs off, the duty on the front");
    // a changed bench: the waveform turned is none, a setting is
    const QString setUp = node.setUpText();
    gen->requestValue(1, Value::Frequency, 100);
    gen->requestShape(1, Shape::Sine);
    check(node.setUpText() == setUp && node.toJsonText() != back->toJsonText(), "set-up: the waveform turned is no change");
    s.ampMax = 2;
    gen->setSettings(1, s);
    check(node.setUpText() != setUp, "set-up: a channel's setting is");
  }

  // the editor
  {
    SimGenNode node;
    node.functionGenerator()->requestValue(0, Value::Frequency, 440);
    SimGenNode::Faults f;
    f.cutoffHz = 20000;
    node.setFaults(f);
    SimGenNodeEditor editor;
    editor.load("Signal", node);
    check(editor.valid() && editor.name() == "Signal", "editor: loaded, valid");
    editor.findChild<QSpinBox *>("ui_simGenChannels")->setValue(2);
    const std::unique_ptr<SimGenNode> made = editor.node();
    check(made->channels() == 2 && made->setting(0).frequency == 440 && made->faults() == f && made->outputs().size() == 4
            && made->given(1) == SimGenNode::defaultGiven(2),
          "editor: two channels, the frequency kept, the faults as set, Vrms and f of each");
    // at most four ticked: the others locked
    const QList<QCheckBox *> boxes = editor.findChildren<QCheckBox *>();
    int ticked = 0, locked = 0;
    for (QCheckBox *b : boxes)
      if (b->text().contains("Peak") || b->text().contains("Mean") || b->text().contains("RMS") || b->text().contains("Frequency")
          || b->text().contains("Voltage"))
      {
        ticked += b->isChecked();
        locked += !b->isEnabled();
      }
    check(ticked == 4 && locked == 6, QString("four ticked, the other six locked (%1, %2)").arg(ticked).arg(locked));
    check(editor.model() == "QtDMM Function generator", "its model");
  }

  if (failed == 0)
    qInfo() << "All simulated generator tests passed.";
  return failed == 0 ? 0 : 1;
}
