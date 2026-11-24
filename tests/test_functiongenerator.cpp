// Copyright (c) 2026 The QtDMM developers
// SPDX-License-Identifier: GPL-3.0-or-later
//
// FunctionGenerator without UI, on a backend of the test's: a request is
// clamped and rounded (seven digits of frequency, the shape's highest
// frequency, offset and amplitude leaving room for each other, the user's
// limits), pending until read back or failed after the timeout, refused
// when the channel is not shown, external, not connected or the shape
// lacks the value; shapes the channel has not; output and allOff(); the
// settings as JSON.

#include <QCoreApplication>

#include <cmath>

#include "core/functiongenerator.h"
#include "testutil.h"

namespace
{
using FG = FunctionGenerator;
using Shape = FG::Shape;
using Value = FG::Value;

/// One channel like the SDS215S; answers at once unless told to keep quiet.
class TestGen : public FunctionGenerator
{
public:
  TestGen() { channelsChanged(); }
  int channelCount() const override { return 1; }
  ChannelInfo channel(int) const override
  {
    ChannelInfo info;
    info.name = QStringLiteral("CH1");
    info.arbitrary = { QStringLiteral("Sinc"), QStringLiteral("Gauss") };
    return info;
  }
  bool quiet = false;
  QList<QString> sent;
  void connected(bool on) { setConnected(on); }
  void external(bool on) { setExternal(0, on); }
  void refuse(const QString &why) { failRequest(0, why); }

protected:
  void sendShape(int ch, Shape shape, const QString &arbitrary) override
  {
    sent << "shape " + shapeName(shape) + ' ' + arbitrary;
    if (quiet)
      return;
    confirmShape(ch, shape, arbitrary);
    // the device lowers a frequency the shape cannot make
    const double f = state(ch).get(Value::Frequency);
    if (!std::isnan(f) && f > maxFrequency(ch, shape))
      confirmValue(ch, Value::Frequency, maxFrequency(ch, shape));
  }
  void sendValue(int ch, Value v, double value) override
  {
    sent << QString("value %1 %2").arg(int(v)).arg(value, 0, 'g', 12);
    if (!quiet)
      confirmValue(ch, v, value);
  }
  void sendOutput(int ch, bool on) override
  {
    sent << QString("output %1").arg(on);
    if (!quiet)
      confirmOutput(ch, on);
  }
};
}

int main(int argc, char **argv)
{
  QCoreApplication app(argc, argv);

  // values, clamped and rounded
  {
    TestGen g;
    check(g.requestValue(0, Value::Frequency, 1234.56789), "frequency sent");
    check(g.state(0).get(Value::Frequency) == 1234.568, "frequency: seven significant digits");
    g.requestValue(0, Value::Frequency, 50e6);
    check(g.state(0).get(Value::Frequency) == 25e6, "a sine goes up to 25 MHz");
    g.requestValue(0, Value::Frequency, 0.01);
    check(g.state(0).get(Value::Frequency) == 0.1, "and down to 0.1 Hz");
    g.requestValue(0, Value::Frequency, 10e6);
    g.requestShape(0, Shape::Square);
    check(g.state(0).shape == Shape::Square && g.state(0).get(Value::Frequency) == 5e6,
          "a square: the device lowered 10 MHz to its 5 MHz");
    check(g.state(0).setState == FG::SetState::Confirmed, "read back: confirmed");

    g.requestValue(0, Value::Amplitude, 2.0);
    g.requestValue(0, Value::Offset, 2.5);
    check(g.state(0).get(Value::Offset) == 2.0, "offset: 2.5 V with 2 Vpp would pass 3 V, so 2 V");
    g.requestValue(0, Value::Amplitude, 3.0);
    check(g.state(0).get(Value::Amplitude) == 2.0, "amplitude: 3 Vpp around 2 V would pass 3 V, so 2 Vpp");
    g.requestValue(0, Value::Offset, -0.12345);
    check(g.state(0).get(Value::Offset) == -0.123, "offset: mV, negative allowed");
    g.requestValue(0, Value::Amplitude, 0.001);
    check(g.state(0).get(Value::Amplitude) == 0.005, "amplitude: at least 5 mVpp");

    FG::ChannelSettings s;
    s.ampMax = 1.0;
    g.setSettings(0, s);
    g.requestValue(0, Value::Amplitude, 2.5);
    check(g.state(0).get(Value::Amplitude) == 1.0, "the user's highest amplitude");

    check(!g.requestValue(0, Value::Duty, 30), "a square has no duty cycle");
    g.requestShape(0, Shape::Pulse);
    g.requestValue(0, Value::Duty, 0.2);
    check(g.state(0).get(Value::Duty) == 1, "pulse: duty at least 1 %");
    g.requestShape(0, Shape::Ramp);
    g.requestValue(0, Value::Symmetry, 33.33);
    check(g.state(0).get(Value::Symmetry) == 33.3, "ramp: symmetry in 0.1 %");
  }

  // shapes
  {
    TestGen g;
    check(g.requestShape(0, Shape::Arbitrary, "Sinc") && g.state(0).arbitrary == "Sinc", "an arbitrary shape the device has");
    check(!g.requestShape(0, Shape::Arbitrary, "Lorentz"), "one it has not: refused");
    check(FG::shapeOf(FG::shapeName(Shape::Pulse)) == Shape::Pulse && FG::shapeOf("unknown") == Shape::Sine,
          "shape names there and back");
  }

  // pending, failed, refused
  {
    TestGen g;
    g.quiet = true;
    g.setConfirmTimeout(150);
    g.requestValue(0, Value::Frequency, 1000);
    check(g.state(0).setState == FG::SetState::Pending && g.state(0).requested[size_t(Value::Frequency)] == 1000
            && std::isnan(g.state(0).get(Value::Frequency)),
          "pending: what was sent, nothing read back yet");
    check(waitFor([&] { return g.state(0).setState == FG::SetState::Failed; }, 1000) && g.state(0).error == "No answer",
          "no answer in time: failed");
    g.requestValue(0, Value::Frequency, 2000);
    g.refuse("Out of range");
    check(g.state(0).setState == FG::SetState::Failed && g.state(0).error == "Out of range", "refused by the device");

    TestGen h;
    h.external(true);
    check(!h.requestValue(0, Value::Frequency, 1000) && !h.requestOutput(0, true), "external: the knobs are locked");
    h.external(false);
    h.connected(false);
    check(!h.requestShape(0, Shape::Square), "not connected: nothing sent");
    h.connected(true);
    FG::ChannelSettings hidden;
    hidden.shown = false;
    h.setSettings(0, hidden);
    check(!h.requestOutput(0, true) && h.shownChannels().isEmpty(), "a channel not shown is never touched");
  }

  // output
  {
    TestGen g;
    check(g.requestOutput(0, true) && g.state(0).output, "output on");
    g.external(true);
    g.allOff();
    check(!g.state(0).output, "allOff() even while external");
  }

  // settings
  {
    TestGen g;
    FG::ChannelSettings s;
    s.color = QColor(0x12, 0x34, 0x56);
    s.ampMax = 2;
    s.offsetStepFine = 0.005;
    g.setSettings(0, s);
    TestGen back;
    back.setSettingsJson(g.settingsJson());
    check(back.settings(0) == s, "settings there and back");
    check(back.color(0) == s.color && TestGen().color(0) == FG::defaultColor(0), "the colour chosen, else the default");
  }

  if (failed == 0)
    qInfo() << "All function generator tests passed.";
  return failed == 0 ? 0 : 1;
}
