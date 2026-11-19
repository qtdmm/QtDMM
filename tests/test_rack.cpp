// Copyright (c) 2026 The QtDMM developers
// SPDX-License-Identifier: GPL-3.0-or-later
//
// The rack (PR 4): an insert for each logic node open on the bench, none
// for a device; the LED switches a node and is kept in its entry; a node
// waiting for an input shows the warning with why; the order, set and by a
// move; the gear, "+ Insert", "Remove", "Show in the rack". The logic
// status window (PR 5a): an LED per node in the rack's order, switching
// and kept, the count, the warning count leading to the first waiting one.

#include <QSignalSpy>
#include <QtWidgets>

#include "core/devicelibrary.h"
#include "logic/formulanode.h"
#include "logic/generatornode.h"
#include "service/devicesession.h"
#include "service/metercontroller.h"
#include "service/noderunner.h"
#include "ui/rack/nodestatus.h"
#include "ui/rack/rackwindow.h"
#include "testutil.h"

int main(int argc, char **argv)
{
  if (qEnvironmentVariableIsEmpty("QT_QPA_PLATFORM"))
    qputenv("QT_QPA_PLATFORM", "offscreen");
  QApplication app(argc, argv);

  QTemporaryDir dir;
  DeviceSession session;
  session.setInstanceId("rack");
  session.setConfigDir(dir.path());
  DeviceLibrary *lib = session.library();

  // a generator, a formula over it and a device that is not there
  GeneratorNode gen;
  gen.setWaves({ GeneratorNode::Wave() });
  gen.setPeriodMs(40);
  const QString g = lib->add("Signal", { { "DMM/model", "QtDMM Generator" }, { "Node/json", gen.toJsonText() } });
  FormulaNode f;
  f.setInputs({ ValueRef(0, "device:" + g), ValueRef(0, "device:gone") });
  f.setFormulas({ { "P", "W", "v1 * v2" } });
  const QString p = lib->add("Power", { { "DMM/model", "QtDMM Formula" }, { "Node/json", f.toJsonText() } });
  FormulaNode twice;
  twice.setInputs({ ValueRef(0, "device:" + g) });
  twice.setFormulas({ { "2U", "V", "v1 * 2" } });
  const QString t = lib->add("Twice", { { "DMM/model", "QtDMM Formula" }, { "Node/json", twice.toJsonText() } });

  RackWindow rack(&session);
  NodeStatus status(&session);
  check(status.nodes().isEmpty(), "status: no node, no LED");
  check(rack.inserts().isEmpty() && rack.findChild<QLabel *>("ui_rackEmpty")->isVisibleTo(&rack), "nothing open: empty, said so");

  QString error;
  for (const QString &id : { g, p, t })
    check(session.open(id, &error) != nullptr, "opened: " + error);
  check(rack.inserts().size() == 3 && !rack.findChild<QLabel *>("ui_rackEmpty")->isVisibleTo(&rack), "three inserts");
  check(rack.order() == QStringList({ g, p, t }), "in the order they were opened");

  rack.show();
  QApplication::processEvents();
  // its values come in; the formula waiting for the device not there warns
  RackInsert *power = rack.inserts().value(1);
  check(power && power->id() == p, "the second is Power");
  waitFor([&] { return session.controller("device:" + t)->lastReading().msecs > 0; }, 3000);
  waitFor([] { return false; }, 400);   // a refresh
  check(power->findChild<QLabel *>("ui_rackWarning")->isVisibleTo(power)
          && !power->findChild<QLabel *>("ui_rackWarning")->toolTip().isEmpty(),
        "waiting: the warning, why in its tooltip: " + power->findChild<QLabel *>("ui_rackWarning")->toolTip());
  RackInsert *twiceInsert = rack.inserts().value(2);
  check(!twiceInsert->findChild<QLabel *>("ui_rackWarning")->isVisibleTo(twiceInsert)
          && twiceInsert->findChild<QLabel *>("ui_rackValues")->text().contains("V"),
        "running: no warning, its value: " + twiceInsert->findChild<QLabel *>("ui_rackValues")->text());

  // the status window: an LED each, the count, Power waiting
  status.show();
  status.refresh();
  check(status.nodes() == QStringList({ g, p, t }), "status: an LED for each node: " + status.nodes().join(","));
  auto *countLabel = status.findChild<QLabel *>("ui_nodeStatusCount");
  check(countLabel->text() == "3 of 3 on", "status: all on: " + countLabel->text());
  auto *warn = status.findChild<QToolButton *>("ui_nodeStatusWarning");
  check(warn->isVisibleTo(&status) && warn->text().endsWith('1') && status.waiting() == QStringList({ p }),
        "status: one waits: " + warn->text());
  check(status.findChild<QToolButton *>("ui_nodeStatusLed_" + p)->toolTip().contains('\n'),
        "status: why in the LED's tooltip");
  QSignalSpy wanted(&status, &NodeStatus::rackRequested);
  warn->click();
  check(wanted.size() == 1 && wanted.first().first().toString() == p, "status: the warning asks for the rack at Power");
  status.findChild<QToolButton *>("ui_nodeStatusRack")->click();
  check(wanted.size() == 2 && wanted.last().first().toString().isEmpty(), "status: Rack asks for the rack");
  QSignalSpy statusSwitched(&status, &NodeStatus::switched);
  status.findChild<QToolButton *>("ui_nodeStatusLed_" + t)->click();
  check(!NodeRunner::of(session.controller("device:" + t))->node()->isOn() && statusSwitched.size() == 1
          && countLabel->text() == "2 of 3 on",
        "status: its LED switched Twice off: " + countLabel->text());
  status.findChild<QToolButton *>("ui_nodeStatusLed_" + t)->click();
  check(NodeRunner::of(session.controller("device:" + t))->node()->isOn(), "status: and on again");
  status.setOrder({ t, g });
  check(status.nodes() == QStringList({ t, g, p }), "status: in the rack's order: " + status.nodes().join(","));

  // the LED: off in the runner and in the entry
  twiceInsert->findChild<QToolButton *>("ui_rackLed")->click();
  check(!NodeRunner::of(session.controller("device:" + t))->node()->isOn(), "the LED switched it off");
  check(lib->find(t)->keys.value("Node/json").toString().contains("\"on\":false"), "kept in its entry");
  twiceInsert->findChild<QToolButton *>("ui_rackLed")->click();
  check(NodeRunner::of(session.controller("device:" + t))->node()->isOn()
          && lib->find(t)->keys.value("Node/json").toString().contains("\"on\":true"),
        "on again");

  // the order: moved, set, a new one at the end
  QSignalSpy moved(&rack, &RackWindow::orderChanged);
  rack.moveInsert(t, 0);
  check(rack.order() == QStringList({ t, g, p }) && moved.size() == 1 && rack.inserts().first()->id() == t,
        "Twice moved to the top: " + rack.order().join(","));
  rack.moveInsert(t, 3);
  check(rack.order() == QStringList({ g, p, t }), "and to the end: " + rack.order().join(","));
  rack.setOrder({ p, t });
  check(rack.order().mid(0, 3) == QStringList({ p, t, g }), "an order set; one not in it after: " + rack.order().join(","));

  // a node closed: its insert gone; a device: none
  session.close(t);
  check(rack.inserts().size() == 2, "closed: its insert gone");
  check(status.nodes().size() == 2, "closed: its status LED gone");

  // the gear, Remove, + Insert
  QSignalSpy settings(&rack, &RackWindow::settingsRequested);
  rack.inserts().first()->findChild<QToolButton *>("ui_rackGear")->click();
  check(settings.size() == 1 && settings.first().first().toString() == p, "the gear asks for the settings");
  QSignalSpy add(&rack, &RackWindow::addRequested);
  QToolButton *plus = rack.findChild<QToolButton *>("ui_rackAdd");
  check(plus && plus->menu() && plus->menu()->actions().size() == 3, "+ Insert: three kinds");
  plus->menu()->actions().last()->trigger();
  check(add.size() == 1 && add.first().first().toString() == "integral", "+ Insert > Integral");

  rack.showInsert(g);
  check(rack.isVisible(), "Show in the rack: shown");

  return failed == 0 ? 0 : 1;
}
