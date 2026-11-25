// Copyright (c) 2026 The QtDMM developers
// SPDX-License-Identifier: GPL-3.0-or-later
//
// PowerSupply with a fake backend: settings as JSON, limits and rounding,
// the asynchronous setting (confirmed, refused, no answer), channels not
// shown and external ones left alone, all off. PsuFrontPanel on it: the
// controls laid out as the LCD's blocks, the knob by wheel, drag and spin
// box, sending at most every interval, OUT and ALL OUT, a locked knob, an
// error on the LCD, the knob following the device. PsuChannelsEditor:
// what it reads and writes, one channel stays shown.
// TEST_DISPLAY_DUMP=<dir> writes the fronts as PNGs.

#include <QApplication>
#include <QCheckBox>
#include <QDoubleSpinBox>
#include <QElapsedTimer>
#include <QSignalSpy>
#include <QTest>
#include <QToolButton>
#include <QWheelEvent>

#include "core/powersupply.h"
#include "ui/psuchannelseditor.h"
#include "ui/psufrontpanel.h"
#include "ui/rotaryknob.h"
#include "ui/views/psulcdwidget.h"
#include "testutil.h"

namespace
{
/// A supply that answers as told.
class FakeSupply : public PowerSupply
{
public:
  enum class Answer { Now, Never, Refuse };
  explicit FakeSupply(int channels, Capabilities caps = SetVoltage) : m_count(channels), m_caps(caps)
  {
    channelsChanged();
  }
  int channelCount() const override { return m_count; }
  ChannelInfo channel(int ch) const override
  {
    ChannelInfo info;
    info.name = QString("CH%1").arg(ch + 1);
    info.uMax = 30;
    info.caps = m_caps;
    return info;
  }
  Answer answer = Answer::Now;
  QList<std::pair<int, double>> voltages;   ///< what was sent
  QList<std::pair<int, bool>> outputs;
  using PowerSupply::confirmVoltage;
  using PowerSupply::setConnected;
  using PowerSupply::setExternal;
  using PowerSupply::setMeasured;

protected:
  void sendVoltage(int ch, double u) override
  {
    voltages << std::make_pair(ch, u);
    if (answer == Answer::Now)
      confirmVoltage(ch, u);
    else if (answer == Answer::Refuse)
      failVoltage(ch, "Out of range");
  }
  void sendOutput(int ch, bool on) override
  {
    outputs << std::make_pair(ch, on);
    confirmOutput(ch, on);
  }

private:
  int m_count;
  Capabilities m_caps;
};

void wait(int ms)
{
  QElapsedTimer t;
  t.start();
  while (t.elapsed() < ms)
    QCoreApplication::processEvents(QEventLoop::AllEvents, 10);
}

void wheel(QWidget *w, int steps, bool shift = false)
{
  const QPointF at(w->width() / 2.0, w->height() / 2.0);
  QWheelEvent e(at, w->mapToGlobal(at), QPoint(), QPoint(0, 120 * steps), Qt::NoButton,
                shift ? Qt::ShiftModifier : Qt::NoModifier, Qt::NoScrollPhase, false);
  QCoreApplication::sendEvent(w, &e);
}

void mouse(QWidget *w, QEvent::Type type, int y, Qt::MouseButtons buttons)
{
  const QPointF at(w->width() / 2.0, y);
  QMouseEvent e(type, at, w->mapToGlobal(at), type == QEvent::MouseMove ? Qt::NoButton : Qt::LeftButton, buttons,
                Qt::NoModifier);
  QCoreApplication::sendEvent(w, &e);
}

QPoint at(QWidget *w, QWidget *top)
{
  return w->mapTo(top, QPoint(0, 0));
}
}

int main(int argc, char **argv)
{
  if (qEnvironmentVariableIsEmpty("QT_QPA_PLATFORM"))
    qputenv("QT_QPA_PLATFORM", "offscreen");
  QApplication app(argc, argv);

  // ---- settings
  {
    PowerSupply::ChannelSettings s;
    check(s.shown && !s.color.isValid() && std::isnan(s.uMin), "settings: shown, the device's colour and limits");
    s.shown = false;
    s.color = QColor(0x12, 0x34, 0x56);
    s.uMax = 12.5;
    s.stepCoarse = 0.5;
    const PowerSupply::ChannelSettings back = PowerSupply::ChannelSettings::fromJson(s.toJson());
    check(back == s && std::isnan(back.uMin), "settings: JSON there and back, NaN stays the device's");
    FakeSupply f(2);
    f.setSettings(1, s);
    FakeSupply g(2);
    g.setSettingsJson(f.settingsJson());
    check(g.settings(1) == s && g.settings(0) == PowerSupply::ChannelSettings(), "settings of all channels as JSON");
    check(g.shownChannels() == QList<int>({ 0 }), "channels shown: " + QString::number(g.shownChannels().size()));
    check(PowerSupply::defaultColor(0) != PowerSupply::defaultColor(1) && g.color(1) == s.color
            && g.color(0) == PowerSupply::defaultColor(0),
          "colours: one per channel, the chosen one");
  }

  // ---- requests
  {
    FakeSupply f(3, PowerSupply::SetVoltage);
    PowerSupply::ChannelSettings s;
    s.uMin = 1;
    s.uMax = 12;
    f.setSettings(0, s);
    check(f.minVoltage(0) == 1 && f.maxVoltage(0) == 12 && f.maxVoltage(1) == 30, "limits: the settings' within the device's");
    s.uMax = 99;
    f.setSettings(2, s);
    check(f.maxVoltage(2) == 30, "a limit beyond the device's: the device's");
    QSignalSpy states(&f, &PowerSupply::stateChanged);
    check(f.requestVoltage(0, 20) && f.voltages.last().second == 12, "clamped to the highest: 12");
    check(f.requestVoltage(0, 3.456) && qFuzzyCompare(f.voltages.last().second, 3.46), "rounded to the resolution: 3.46");
    check(f.state(0).setState == PowerSupply::SetState::Confirmed && qFuzzyCompare(f.state(0).setpoint, 3.46)
            && states.count() >= 2,
          "confirmed when read back");

    f.answer = FakeSupply::Answer::Refuse;
    f.requestVoltage(1, 5);
    check(f.state(1).setState == PowerSupply::SetState::Failed && f.state(1).error == "Out of range",
          "refused: failed, with the device's reason");
    f.answer = FakeSupply::Answer::Never;
    f.setConfirmTimeout(80);
    f.requestVoltage(1, 6);
    check(f.state(1).setState == PowerSupply::SetState::Pending && f.state(1).requested == 6 && f.state(1).error.isEmpty(),
          "sent: pending, the value asked for");
    wait(200);
    check(f.state(1).setState == PowerSupply::SetState::Failed && f.state(1).error == "No answer",
          "no answer: failed after the timeout, " + f.state(1).error);

    // left alone: not shown, external, disconnected
    f.answer = FakeSupply::Answer::Now;
    PowerSupply::ChannelSettings hidden;
    hidden.shown = false;
    f.setSettings(2, hidden);
    const int sent = int(f.voltages.size());
    check(!f.requestVoltage(2, 5) && !f.requestOutput(2, true) && f.voltages.size() == sent,
          "a channel not shown: nothing sent");
    f.requestOutput(0, true);
    f.requestOutput(1, true);
    f.outputs.clear();
    f.allOff();
    check(f.outputs == QList<std::pair<int, bool>>({ { 0, false }, { 1, false } }) && !f.state(0).output,
          "all off: the channels shown, not the hidden one");
    f.setExternal(1, true);
    check(!f.requestVoltage(1, 4), "external: the knob's request refused");
    f.setExternal(1, false);
    f.setConnected(false);
    check(!f.requestVoltage(0, 4) && !f.requestOutput(0, true), "not connected: nothing sent");
    FakeSupply readOnly(1, PowerSupply::MeasureCurrent);
    check(!readOnly.requestVoltage(0, 4), "a channel that cannot set: nothing sent");
  }

  // ---- the front: layout
  for (int n = 1; n <= 4; ++n)
  {
    FakeSupply f(n);
    PsuFrontPanel panel(&f);
    panel.resize(700, 500);
    panel.show();
    QCoreApplication::processEvents();
    bool ok = panel.display()->channelCount() == n && (panel.allOffKey() != nullptr) == (n > 1);
    for (int ch = 0; ch < n; ++ch)
      ok = ok && panel.knob(ch) && panel.outputKey(ch);
    if (n == 2)
      ok = ok && at(panel.knob(0), &panel).y() == at(panel.knob(1), &panel).y()
           && at(panel.knob(1), &panel).x() > at(panel.knob(0), &panel).x();
    if (n >= 3)
      ok = ok && at(panel.knob(2), &panel).y() > at(panel.knob(0), &panel).y()
           && at(panel.knob(0), &panel).y() == at(panel.knob(1), &panel).y()
           && qAbs(at(panel.knob(2), &panel).x() - at(panel.knob(0), &panel).x()) < 4;
    check(ok, QString("front with %1 channel(s): a knob and OUT each, laid out as the blocks, ALL OUT with more than one").arg(n));
    if (!qEnvironmentVariableIsEmpty("TEST_DISPLAY_DUMP"))
    {
      panel.setCaption(QString("Simulated power supply (%1 ch)").arg(n));
      f.setMeasured(0, 12.0);
      f.requestOutput(0, true);
      f.requestVoltage(0, 12.0);
      panel.resize(panel.sizeHint().expandedTo(QSize(n == 1 ? 360 : 600, n > 2 ? 520 : 330)));
      QCoreApplication::processEvents();
      panel.grab().save(qEnvironmentVariable("TEST_DISPLAY_DUMP") + QString("/psu_front_%1.png").arg(n));
    }
  }
  {
    FakeSupply f(3);
    PowerSupply::ChannelSettings hidden;
    hidden.shown = false;
    f.setSettings(1, hidden);
    PsuFrontPanel panel(&f);
    check(!panel.knob(1) && panel.knob(2) && panel.display()->channelCount() == 2, "a channel not shown: not on the front");
    f.setSettings(1, PowerSupply::ChannelSettings());
    check(panel.knob(1) && panel.display()->channelCount() == 3, "shown again: the front follows the settings");
  }

  // ---- the front: turning
  {
    FakeSupply f(2);
    PowerSupply::ChannelSettings s;
    s.stepCoarse = 0.5;
    s.stepFine = 0.01;
    f.setSettings(0, s);
    PsuFrontPanel panel(&f);
    panel.resize(600, 400);
    panel.show();
    QCoreApplication::processEvents();
    RotaryKnob *knob = panel.knob(0);
    check(knob->coarseStep() == 0.5 && knob->fineStep() == 0.01 && knob->maximum() == 30, "the knob: steps and range of the settings");
    wheel(knob, 2);
    check(f.voltages.size() == 1 && f.voltages.last().second == 1.0 && panel.display()->showsSetpoint(0)
            && panel.display()->text(0, 0) == "1.00",
          "wheel: two coarse steps, sent at once, the setpoint on the LCD");
    wheel(knob, 1, true);
    wheel(knob, 1, true);
    check(f.voltages.size() == 1, "turning on: not sent again within the interval");
    wait(PsuFrontPanel::kSendInterval * 3);
    check(f.voltages.size() == 2 && qFuzzyCompare(f.voltages.last().second, 1.02), "then the last value: 1.02");
    // drag up two steps
    mouse(knob, QEvent::MouseButtonPress, 30, Qt::LeftButton);
    mouse(knob, QEvent::MouseMove, 30 - 2 * RotaryKnob::kDragPixels, Qt::LeftButton);
    mouse(knob, QEvent::MouseButtonRelease, 30 - 2 * RotaryKnob::kDragPixels, Qt::NoButton);
    wait(PsuFrontPanel::kSendInterval * 3);
    check(qFuzzyCompare(f.voltages.last().second, 2.02), "drag up: two coarse steps, " + QString::number(f.voltages.last().second));
    // the spin box: Enter takes, Esc leaves
    knob->edit();
    auto *box = knob->findChild<QDoubleSpinBox *>();
    check(box && knob->isEditing(), "double click: the spin box on the knob");
    box->setValue(7.25);
    QTest::keyClick(box, Qt::Key_Return);
    wait(PsuFrontPanel::kSendInterval * 3);
    check(!knob->isEditing() && qFuzzyCompare(f.voltages.last().second, 7.25), "Enter: 7.25 sent");
    const int sent = int(f.voltages.size());
    knob->edit();
    box->setValue(9);
    QTest::keyClick(box, Qt::Key_Escape);
    wait(PsuFrontPanel::kSendInterval * 3);
    check(!knob->isEditing() && f.voltages.size() == sent && qFuzzyCompare(knob->value(), 7.25), "Esc: nothing sent");
    // the knob follows the device
    f.confirmVoltage(0, 4.5);
    check(qFuzzyCompare(knob->value(), 4.5), "the setpoint read back: the knob follows, " + QString::number(knob->value()));

    // too low a window folds the controls itself, with room they come back;
    // folded by the person they stay folded
    {
      int changed = 0;
      QObject::connect(&panel, &OutputFront::controlsShownChanged, [&] { ++changed; });
      const QSize was = panel.size();
      panel.show();   // resize events come to a widget shown
      panel.resize(600, 600);
      check(panel.controlsShown(), "room: the controls shown");
      panel.resize(600, 60);
      check(!panel.controlsShown() && !panel.isFolded() && changed == 1, "too low: folded by itself, not by the person");
      panel.resize(600, 600);
      check(panel.controlsShown() && changed == 2, "room again: shown again");
      panel.setFolded(true);
      panel.resize(600, 60);
      panel.resize(600, 600);
      check(!panel.controlsShown() && panel.isFolded(), "folded by the person: stays folded");
      panel.setFolded(false);
      panel.resize(was);
      panel.hide();
    }

    // OUT and ALL OUT
    panel.outputKey(1)->click();
    check(f.state(1).output && !f.state(0).output, "OUT: that channel on");
    panel.outputKey(0)->click();
    panel.allOffKey()->click();
    check(!f.state(0).output && !f.state(1).output, "ALL OUT: all off");

    // locked, failed, measured
    f.setExternal(1, true);
    check(!panel.knob(1)->isEnabled(), "external: the knob locked");
    f.answer = FakeSupply::Answer::Refuse;
    wheel(knob, 1);
    check(panel.display()->channelError(0) == "Out of range", "refused: ERR on the block, " + panel.display()->channelError(0));
    panel.display()->setSetpointHold(50);
    wheel(knob, 1);
    f.setMeasured(0, 4.49);
    check(panel.display()->showsSetpoint(0) && panel.display()->text(0, 0) != "4.49", "turning: the setpoint over the measured value");
    wait(150);
    check(panel.display()->text(0, 0) == "4.49", "then the measured value: " + panel.display()->text(0, 0));
    f.setConnected(false);
    check(!panel.knob(0)->isEnabled() && !panel.outputKey(0)->isEnabled(), "not connected: nothing to operate");
  }

  // ---- the settings
  {
    FakeSupply f(3);
    PsuChannelsEditor editor;
    editor.load(&f);
    QList<PowerSupply::ChannelSettings> s = editor.settings();
    check(s.size() == 3 && s[0] == PowerSupply::ChannelSettings(), "editor: unchanged settings stay the device's");
    const QList<QCheckBox *> boxes = editor.findChildren<QCheckBox *>();
    boxes[0]->setChecked(false);
    boxes[1]->setChecked(false);
    check(!boxes[2]->isEnabled() && boxes[0]->isEnabled(), "the last channel shown cannot be unticked");
    QList<QDoubleSpinBox *> spins = editor.findChildren<QDoubleSpinBox *>();
    spins[1]->setValue(12);   // channel 1's highest
    editor.apply(&f);
    check(f.shownChannels() == QList<int>({ 2 }) && f.maxVoltage(0) == 12 && std::isnan(f.settings(0).uMin),
          "apply: shown, the highest set, the lowest still the device's");
  }

  if (failed == 0)
    qInfo() << "All power supply tests passed.";
  return failed == 0 ? 0 : 1;
}
