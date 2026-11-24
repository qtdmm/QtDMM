// Copyright (c) 2026 The QtDMM developers
// SPDX-License-Identifier: GPL-3.0-or-later
//
// GenFrontPanel on a generator of the test's: keys and knobs per channel,
// ALL OUT with two, the frequency knob in 1-2-5 steps (fine with Shift)
// and reading "2.5k" when typed, a turn sent and shown, the shape keys
// (the duty knob only for a pulse, symmetry only for a ramp, the key lit
// once the device took it), the offset's range following the amplitude,
// EXT locking, ERR on a refused request, OUT; the channels editor there
// and back. TEST_DISPLAY_DUMP=<dir> writes a PNG for visual review.

#include <QApplication>
#include <QDir>
#include <QDoubleSpinBox>
#include <QLineEdit>
#include <QTest>
#include <QToolButton>

#include "core/functiongenerator.h"
#include "ui/genchannelseditor.h"
#include "ui/genfrontpanel.h"
#include "ui/rotaryknob.h"
#include "ui/views/genlcdwidget.h"
#include "testutil.h"

namespace
{
using FG = FunctionGenerator;
using Shape = FG::Shape;
using Value = FG::Value;

class TestGen : public FunctionGenerator
{
public:
  explicit TestGen(int channels = 1) : m_channels(channels)
  {
    channelsChanged();
    for (int ch = 0; ch < channels; ++ch)
    {
      confirmValue(ch, Value::Frequency, 1000);
      confirmValue(ch, Value::Amplitude, 1.0);
      confirmValue(ch, Value::Offset, 0.0);
    }
  }
  int channelCount() const override { return m_channels; }
  ChannelInfo channel(int ch) const override
  {
    ChannelInfo info;
    info.name = QString("CH%1").arg(ch + 1);
    info.arbitrary = { QStringLiteral("Sinc") };
    return info;
  }
  bool quiet = false;
  QString refuse;
  void external(bool on) { setExternal(0, on); }

protected:
  void sendShape(int ch, Shape shape, const QString &arbitrary) override
  {
    if (!quiet)
      confirmShape(ch, shape, arbitrary);
  }
  void sendValue(int ch, Value v, double value) override
  {
    if (!refuse.isEmpty())
      failRequest(ch, refuse);
    else if (!quiet)
      confirmValue(ch, v, value);
  }
  void sendOutput(int ch, bool on) override { confirmOutput(ch, on); }

private:
  int m_channels;
};

void pause(int ms)
{
  waitFor([] { return false; }, ms);
}
}

int main(int argc, char **argv)
{
  if (qEnvironmentVariableIsEmpty("QT_QPA_PLATFORM"))
    qputenv("QT_QPA_PLATFORM", "offscreen");
  QApplication app(argc, argv);

  // keys and knobs
  {
    TestGen g;
    GenFrontPanel front(&g);
    front.resize(front.sizeHint());
    front.show();
    check(front.knob(0, Value::Frequency) && front.knob(0, Value::Amplitude) && front.knob(0, Value::Offset),
          "the knobs of a channel");
    check(front.shapeKey(0, Shape::Sine) && front.shapeKey(0, Shape::Pulse) && front.shapeKey(0, Shape::Arbitrary),
          "the shape keys, and the menu of the other shapes");
    check(!front.allOffKey(), "one channel: no ALL OUT");
    check(front.shapeKey(0, Shape::Sine)->isChecked(), "the sine lit");
    check(front.knob(0, Value::Frequency)->value() == 1000, "the frequency knob at what the device has");
    check(!front.knob(0, Value::Duty)->isVisibleTo(&front) && !front.knob(0, Value::Symmetry)->isVisibleTo(&front),
          "a sine: no duty, no symmetry knob");
    TestGen two(2);
    GenFrontPanel dual(&two);
    check(dual.allOffKey() && dual.knob(1, Value::Frequency), "two channels: both, and ALL OUT");
  }

  // the frequency knob in decades
  {
    TestGen g;
    GenFrontPanel front(&g);
    front.show();
    RotaryKnob *f = front.knob(0, Value::Frequency);
    f->setFocus();
    QTest::keyClick(f, Qt::Key_Up);
    check(f->value() == 2000, QString("up from 1 kHz: 2 kHz, %1").arg(f->value()));
    QTest::keyClick(f, Qt::Key_Up);
    check(f->value() == 5000, "then 5 kHz");
    QTest::keyClick(f, Qt::Key_Down, Qt::ShiftModifier);
    check(f->value() == 4990, QString("with Shift down by 1 in the third digit: %1").arg(f->value()));
    check(waitFor([&] { return g.state(0).get(Value::Frequency) == 4990; }), "sent and read back");
    check(front.display()->text(0, GenLcdWidget::Frequency) == "4.990000" && front.display()->frequencyUnit(0) == "kHz",
          "on the LCD: " + front.display()->text(0, GenLcdWidget::Frequency));
    // typed with a prefix
    f->edit();
    auto *box = f->findChild<QDoubleSpinBox *>();
    check(box && box->isVisible(), "a double click opens the spin box");
    if (box)
    {
      auto *line = box->findChild<QLineEdit *>();
      line->selectAll();
      QTest::keyClicks(line, "2.5k");
      QTest::keyClick(box, Qt::Key_Return);
      check(f->value() == 2500, QString("\"2.5k\" read as 2500 Hz: %1").arg(f->value()));
    }
  }

  // shapes, ranges, the offset following the amplitude
  {
    TestGen g;
    GenFrontPanel front(&g);
    front.show();
    QTest::mouseClick(front.shapeKey(0, Shape::Pulse), Qt::LeftButton);
    check(g.state(0).shape == Shape::Pulse && front.shapeKey(0, Shape::Pulse)->isChecked()
            && !front.shapeKey(0, Shape::Sine)->isChecked(),
          "pulse: sent, its key lit");
    check(front.knob(0, Value::Duty)->isVisibleTo(&front) && !front.knob(0, Value::Symmetry)->isVisibleTo(&front),
          "a pulse: the duty knob");
    QTest::mouseClick(front.shapeKey(0, Shape::Ramp), Qt::LeftButton);
    check(front.knob(0, Value::Symmetry)->isVisibleTo(&front) && !front.knob(0, Value::Duty)->isVisibleTo(&front),
          "a ramp: the symmetry knob");
    check(front.knob(0, Value::Offset)->maximum() == 2.5, QString("offset up to 3 - 1/2 V: %1").arg(front.knob(0, Value::Offset)->maximum()));
    RotaryKnob *a = front.knob(0, Value::Amplitude);
    a->setFocus();
    for (int i = 0; i < 10; ++i)
      QTest::keyClick(a, Qt::Key_Up);
    check(waitFor([&] { return g.state(0).get(Value::Amplitude) == 2.0; }), "amplitude 2 Vpp sent");
    check(std::abs(front.knob(0, Value::Offset)->maximum() - 2.0) < 1e-9, "the offset's range follows: 2 V");

    g.quiet = true;
    QTest::mouseClick(front.shapeKey(0, Shape::Square), Qt::LeftButton);
    check(!front.shapeKey(0, Shape::Square)->isChecked() && front.shapeKey(0, Shape::Ramp)->isChecked(),
          "not taken yet: the key stays as the device has it");
    g.quiet = false;
  }

  // EXT, ERR, OUT
  {
    TestGen g;
    GenFrontPanel front(&g);
    front.show();
    QTest::mouseClick(front.outputKey(0), Qt::LeftButton);
    check(g.state(0).output, "OUT: on");
    g.external(true);
    check(!front.knob(0, Value::Frequency)->isEnabled() && !front.shapeKey(0, Shape::Square)->isEnabled()
            && !front.outputKey(0)->isEnabled(),
          "EXT: knobs and keys locked");
    g.external(false);
    g.refuse = "Out of range";
    RotaryKnob *o = front.knob(0, Value::Offset);
    o->setFocus();
    QTest::keyClick(o, Qt::Key_Up);
    pause(250);
    check(front.display()->toolTip().contains("Out of range"), "refused: ERR and why: " + front.display()->toolTip());
    if (!qEnvironmentVariable("TEST_DISPLAY_DUMP").isEmpty())
    {
      QDir().mkpath(qEnvironmentVariable("TEST_DISPLAY_DUMP"));
      front.resize(560, 380);
      front.grab().save(qEnvironmentVariable("TEST_DISPLAY_DUMP") + "/gen_front.png");
    }
  }

  // the channels editor
  {
    TestGen g(2);
    FG::ChannelSettings s;
    s.color = QColor(0x10, 0x80, 0x20);
    s.ampMax = 1.5;
    s.offsetStepFine = 0.002;
    g.setSettings(1, s);
    GenChannelsEditor editor;
    editor.load(&g);
    const QList<FG::ChannelSettings> back = editor.settings();
    check(back.size() == 2 && back[1] == s && back[0] == FG::ChannelSettings(), "editor: there and back, defaults stay");
  }

  if (failed == 0)
    qInfo() << "All generator front tests passed.";
  return failed == 0 ? 0 : 1;
}
