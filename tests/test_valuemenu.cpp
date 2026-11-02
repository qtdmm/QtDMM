// Copyright (c) 2026 The QtDMM developers
// SPDX-License-Identifier: GPL-3.0-or-later
//
// ValueMenu, where a value of an open meter is shown: the window's meter's
// values go to InstanceWidget, another meter's LCD places to its front
// panel, its graph curve on the axis chosen, the analog meter and the
// Poincaré plot by its ValueRef.
#include <QApplication>
#include <QDebug>
#include <QMenu>
#include <QTemporaryDir>

#include "core/devicelibrary.h"
#include "recording/recordingstore.h"
#include "service/devicesession.h"
#include "service/metercontroller.h"
#include "ui/frontpanel.h"
#include "ui/instancewidget.h"
#include "ui/valuemenu.h"
#include "ui/views/analogmeter.h"
#include "ui/views/graphwidget.h"
#include "ui/views/lcdwidget.h"
#include "testutil.h"

// The action @p path ("&Graph/&Right axis") of @p menu, nullptr when none.
static QAction *action(QMenu *menu, const QStringList &path)
{
  for (QAction *a : menu->actions())
    if (a->text() == path.first())
    {
      if (path.size() == 1)
        return a;
      return a->menu() ? action(a->menu(), path.mid(1)) : nullptr;
    }
  return nullptr;
}

int main(int argc, char **argv)
{
  if (qEnvironmentVariableIsEmpty("QT_QPA_PLATFORM"))
    qputenv("QT_QPA_PLATFORM", "offscreen");
  QApplication app(argc, argv);
  QTemporaryDir dir;
  DeviceLibrary lib(dir.path());
  const QString b = lib.add("Sim B", {{"DMM/model", "QtDMM Simulated meter"}, {"Port settings/device", "calc A/DC 2"},
                                      {"DMM/virtual-waveform", 0}, {"DMM/virtual-min", "0.25"},
                                      {"DMM/virtual-max", "0.25"}, {"DMM/virtual-values", 2}});
  DeviceSession session;
  session.setConfigDir(dir.path());
  AnalogMeter meter;   // outlives the window's MeterView
  InstanceWidget wid("vm", dir.path(), &session);
  wid.addMeter(&meter);
  MeterController *ctl = session.open(b);
  check(ctl != nullptr, "Sim B open");
  if (!ctl)
    return 1;
  const QString source = ctl->source();
  FrontPanel panel(&session);
  panel.setValue(ValueRef(0, source));
  panel.display()->setValueCount(2);   // MainWindow::applyLcdLook() does so

  QList<std::pair<ValueMenu::View, QString>> shown;
  QList<ValueRef> table;
  ValueMenu::Target target;
  target.wid = &wid;
  target.panel = [&](const QString &s) { return s == source ? &panel : nullptr; };
  target.tableShown = [&](const ValueRef &v) { return table.contains(v); };
  target.setTableShown = [&](const ValueRef &v, bool on) { on ? table.append(v) : void(table.removeAll(v)); };
  target.show = [&](ValueMenu::View view, const QString &s) { shown.append({ view, s }); };
  auto menuFor = [&](const ValueRef &v)
  {
    auto *menu = new QMenu;
    ValueMenu::fill(menu, target, v);
    return menu;
  };

  // 1. another meter's value 2 as the main value of its front panel
  {
    QMenu *menu = menuFor(ValueRef(1, source));
    QAction *main = action(menu, { "&LCD", "&Main value" });
    check(main && main->isEnabled() && !main->isChecked(), "LCD main value offered");
    if (main)
      main->trigger();
    check(panel.value() == ValueRef(1, source), "its front panel shows value 2: " + panel.value().toString());
    check(panel.secondaries() == QList<int>{ 0 }, "the main one went to the small line");
    check(!shown.isEmpty() && shown.last().first == ValueMenu::View::Lcd && shown.last().second == source,
          "its front panel opened");
    delete menu;
  }
  // 2. on the graph's right axis, then off
  {
    QMenu *menu = menuFor(ValueRef(0, source));
    action(menu, { "&Graph", "&Right axis" })->trigger();
    check(wid.graph()->valueShown(ValueRef(0, source)) && wid.graph()->valueAxis(ValueRef(0, source)) == GraphWidget::AxisRight,
          "in the graph on the right axis");
    delete menu;
    menu = menuFor(ValueRef(0, source));
    check(action(menu, { "&Graph", "&Right axis" })->isChecked(), "the menu says right");
    action(menu, { "&Graph", "&Off" })->trigger();
    check(!wid.graph()->valueShown(ValueRef(0, source)), "off the graph");
    delete menu;
    menu = menuFor(ValueRef(0, source));
    check(action(menu, { "&Graph", "&Off" })->isChecked() && !action(menu, { "&Graph", "&Right axis" })->isChecked()
            && !action(menu, { "&Graph", "&Left axis" })->isChecked(),
          "off: the menu says off, no axis beside it");
    delete menu;
  }
  // 3. the analog meter and the Poincaré plot by its ValueRef
  {
    QMenu *menu = menuFor(ValueRef(1, source));
    action(menu, { "&Analog meter" })->trigger();
    action(menu, { "&Poincaré plot" })->trigger();
    check(wid.meterValue() == ValueRef(1, source) && wid.poincareValue() == ValueRef(1, source),
          "analog meter and Poincaré plot show it");
    delete menu;
    menu = menuFor(ValueRef(1, source));
    check(action(menu, { "&Analog meter" })->isChecked(), "the menu says so");
    delete menu;
  }
  // 5. the rules on a value: a new alarm, the limits, by its ValueRef
  {
    QList<ValueRef> alarmFor, limitsFor;
    target.addAlarm = [&](const ValueRef &v) { alarmFor << v; };
    target.editLimits = [&](const ValueRef &v) { limitsFor << v; };
    QMenu *menu = menuFor(ValueRef(1, source));
    action(menu, { "&New alarm..." })->trigger();
    action(menu, { "Li&mits..." })->trigger();
    check(alarmFor == QList<ValueRef>{ ValueRef(1, source) } && limitsFor == alarmFor, "the meter's value");
    delete menu;
  }

  // 4. the graph's store follows its values: Sim B's value, Sim B's store
  //    (its value the main curve); another meter's value first, its store;
  //    Sim B closed, the next value's meter; none left, the empty store
  {
    const QString c = lib.add("Sim C", {{"DMM/model", "QtDMM Simulated meter"}, {"Port settings/device", "calc V/DC 3"}});
    MeterController *other = session.open(c);
    check(other != nullptr, "Sim C open");
    if (!other)
      return 1;
    GraphWidget *graph = wid.graph();
    graph->setValues({ ValueRef(0, source) });
    check(graph->store() == ctl->recorder() && graph->baseSource() == source && graph->mainShown()
            && graph->otherValues().isEmpty(),
          "only Sim B's value: its store is the graph's, the value its main curve");
    graph->setValues({ ValueRef(0, source), ValueRef(0, other->source()) });
    check(graph->store() == ctl->recorder() && graph->otherValues() == QList<ValueRef>{ ValueRef(0, other->source()) },
          "Sim C's value beside: a curve from its store");
    session.close(b);
    check(graph->store() == other->recorder() && graph->values().contains(ValueRef(0, source)),
          "Sim B closed: Sim C's store, Sim B's value kept");
    session.close(c);
    check(graph->baseSource().isEmpty() && graph->store()->count() == 0,
          QString("none left: the empty store, got %1 with %2").arg(graph->baseSource()).arg(graph->store()->count()));
  }

  if (failed)
  {
    qWarning() << failed << "ValueMenu check(s) failed";
    return 1;
  }
  qInfo() << "All ValueMenu tests passed.";
  return 0;
}
