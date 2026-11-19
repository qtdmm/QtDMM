// Copyright (c) 2026 The QtDMM developers
// SPDX-License-Identifier: GPL-3.0-or-later
//
// FrontPanel, one meter's LCD with its keys: it shows the meter of its
// value, "no value" while that is not open, the readings when it comes,
// the keys only for a meter that has them, folded on request. A logic
// node has its bar instead (status LED, an integral's 0 and HOLD), and
// every panel the gear for its settings.
#include <QApplication>
#include <QDebug>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QToolButton>

#include "core/devicelibrary.h"
#include "core/reading.h"
#include "logic/formulanode.h"
#include "logic/integralnode.h"
#include "service/devicesession.h"
#include "service/metercontroller.h"
#include "service/noderunner.h"
#include "ui/frontpanel.h"
#include "ui/nodebar.h"
#include "ui/views/lcdwidget.h"
#include "testutil.h"

int main(int argc, char **argv)
{
  if (qEnvironmentVariableIsEmpty("QT_QPA_PLATFORM"))
    qputenv("QT_QPA_PLATFORM", "offscreen");
  QApplication app(argc, argv);
  QTemporaryDir dir;
  DeviceLibrary lib(dir.path());
  const QString sim = lib.add("Bench sim", {{"DMM/model", "QtDMM Simulated meter"}, {"Port settings/device", "calc V/DC 1"},
                                            {"DMM/virtual-waveform", 0}, {"DMM/virtual-min", "1.5"}, {"DMM/virtual-max", "1.5"}});
  DeviceSession session;
  session.setInstanceId("test");
  session.setConfigDir(dir.path());

  FrontPanel panel(&session);
  panel.show();
  QSignalSpy keys(&panel, &FrontPanel::keysChanged);

  // 1. the value of a meter not open: no meter, no value, no keys
  panel.setValue(ValueRef(0, "device:" + sim));
  check(panel.controller() == nullptr, "not open: no meter");
  check(panel.display()->lineValue(0).isEmpty() || panel.display()->lineValue(0).startsWith("-"),
        "not open: no value, got " + panel.display()->lineValue(0));
  check(!panel.keysSupported() && panel.keysHeight() == 0, "not open: no keys");

  // 2. opened: its readings on the LCD
  MeterController *ctl = session.open(sim);
  check(ctl && panel.controller() == ctl, "opened: the panel shows it");
  check(waitFor([&] { return panel.display()->lineValue(0).contains("1.5"); }, 4000),
        "its reading on the LCD, got " + panel.display()->lineValue(0));

  // 3. the simulated meter is a logic node: its bar with the status LED
  // instead of keys, none of its own; the gear asks for its settings
  auto *bar = panel.findChild<NodeBar *>("ui_nodeBar");
  check(panel.keysSupported() && panel.keysShown() && bar && bar->isVisibleTo(&panel) && panel.keysHeight() > 0,
        "simulated meter: its node bar");
  check(waitFor([&] { bar->refresh(); return bar->statusColor().green() > 150 && bar->statusColor().red() < 100; }, 2000),
        "computing: the LED green");
  check(!panel.findChild<QToolButton *>("ui_nodeControl_hold"), "a generator: no HOLD");
  QSignalSpy settings(&panel, &FrontPanel::settingsRequested);
  auto *gear = panel.findChild<QToolButton *>("ui_panelGear");
  check(gear && gear->isVisibleTo(&panel), "the gear on the panel");
  gear->click();
  check(settings.size() == 1 && settings.first().first().toString() == sim, "the gear asks for its settings");
  const qsizetype changes = keys.size();
  panel.setFolded(true);
  check(panel.isFolded() && !bar->isVisibleTo(&panel) && panel.keysHeight() == 0 && keys.size() == changes + 1,
        "folded: the bar gone");
  panel.setFolded(false);
  ctl->connectMeter(false);
  bar->refresh();
  check(bar->statusColor().green() < 100, "disconnected: the LED dark");
  ctl->connectMeter(true);

  // 3b. an integral over it: 0 and HOLD on its bar, HOLD kept in its entry
  {
    IntegralNode in;
    in.setInputs({ ValueRef(0, "device:" + sim) });
    in.setUnit("Vh");
    const QString integral = lib.add("Sum", { { "DMM/model", "QtDMM Integral" }, { "Node/json", in.toJsonText() } });
    MeterController *ictl = session.open(integral);
    FrontPanel ipanel(&session);
    ipanel.show();
    ipanel.setValue(ValueRef(0, "device:" + integral));
    auto *hold = ipanel.findChild<QToolButton *>("ui_nodeControl_hold");
    auto *zero = ipanel.findChild<QToolButton *>("ui_nodeControl_zero");
    check(ictl && hold && zero && hold->isCheckable() && !hold->isChecked(), "integral: 0 and HOLD, not held");
    hold->click();
    const NodeRunner *runner = NodeRunner::of(ictl);
    check(!runner->node()->isOn() && hold->isChecked(), "HOLD: held");
    check(lib.find(integral)->keys.value("Node/json").toString().contains("\"on\":false"), "HOLD kept in its entry");
    auto *ibar = ipanel.findChild<NodeBar *>("ui_nodeBar");
    check(ibar->statusColor().red() > 150 && ibar->statusColor().green() > 150, "held: the LED yellow");
    hold->click();
    check(runner->node()->isOn() && !hold->isChecked()
            && lib.find(integral)->keys.value("Node/json").toString().contains("\"on\":true"),
          "HOLD again: running");
    session.close(integral);
  }

  // 3c. a formula switched off before: opens disconnected and on
  {
    FormulaNode f;
    f.setInputs({ ValueRef(0, "device:" + sim) });
    f.setFormulas({ { "2U", "V", "v1 * 2" } });
    f.setOn(false);
    const QString old = lib.add("Old off", { { "DMM/model", "QtDMM Formula" }, { "Node/json", f.toJsonText() } });
    MeterController *octl = session.open(old);
    check(octl && !octl->isWanted() && NodeRunner::of(octl)->node()->isOn(), "an old formula off: disconnected, on");
    session.close(old);
  }

  // 4. closed: no value again, the value kept
  session.close(sim);
  check(panel.controller() == nullptr && panel.value().source == "device:" + sim, "closed: no meter, value kept");
  check(!panel.display()->lineValue(0).contains("1.5"), "closed: the last reading gone");

  // 5. an empty source: no meter (the window has none of its own)
  session.open(sim);
  panel.setValue(ValueRef(0));
  check(panel.controller() == nullptr, "empty source: no meter");

  if (failed)
  {
    qWarning() << failed << "FrontPanel check(s) failed";
    return 1;
  }
  qInfo() << "All FrontPanel tests passed.";
  return 0;
}
