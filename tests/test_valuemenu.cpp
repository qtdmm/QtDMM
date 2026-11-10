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
#include "ui/graphview.h"
#include "ui/instancewidget.h"
#include "ui/valuemenu.h"
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
  InstanceWidget wid("vm", dir.path(), &session);
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
  // 3. the analog meters and the Poincaré plot choose in their own menu;
  //    here only a new analog meter for the value
  {
    QMenu *menu = menuFor(ValueRef(1, source));
    check(!action(menu, { "&Needle meter" }) && !action(menu, { "&Poincaré plot" }) && !action(menu, { "New &needle meter" }),
          "no entries of the analog meter or the Poincaré plot, no new meter without newMeter");
    delete menu;
    QList<ValueRef> added;
    target.newMeter = [&added](const ValueRef &v) { added << v; };
    menu = menuFor(ValueRef(1, source));
    action(menu, { "New &needle meter" })->trigger();
    check(added == QList<ValueRef>{ ValueRef(1, source) }, "a new analog meter for it");
    delete menu;
    target.newMeter = nullptr;
    target.newPoincare = [&added](const ValueRef &v) { added << v; };
    menu = menuFor(ValueRef(0, source));
    action(menu, { "New &Poincaré plot" })->trigger();
    check(added.size() == 2 && added.last() == ValueRef(0, source), "a new Poincaré plot for it");
    delete menu;
    target.newPoincare = nullptr;
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

  // 3b. a second graph: its own values and time window on the same store;
  // what is done in one makes it the current one; the menu lists both
  {
    GraphWidget second;
    GraphView *view = wid.addGraph(&second, "graph-2");
    GraphWidget *first = wid.graph();
    first->setValues({ ValueRef(0, source) });
    second.setValues({ ValueRef(1, source) });
    check(first->values() == QList<ValueRef>{ ValueRef(0, source) } && second.values() == QList<ValueRef>{ ValueRef(1, source) },
          "each graph its own values");
    check(first->store() == ctl->recorder() && second.store() == ctl->recorder(), "both on the meter's store");
    Q_EMIT first->windowRequested(30);
    wid.setGraphWindow(&second, 60);
    check(first->windowSetting() == 30 && second.windowSetting() == 60,
          QString("each its own window, got %1 and %2").arg(first->windowSetting()).arg(second.windowSetting()));
    Q_EMIT second.used();
    check(wid.graph() == &second, "used: the second graph is the current one");
    Q_EMIT second.windowRequested(120);
    check(first->windowSetting() == 30 && second.windowSetting() == 120,
          QString("a time button of the second: its window alone, got %1 and %2").arg(first->windowSetting()).arg(second.windowSetting()));
    Q_EMIT first->zoomOut(2.0);
    check(wid.graph() == first && first->windowSetting() > 30 && second.windowSetting() == 120,
          QString("a zoom in the first: current again, its window alone, got %1 and %2").arg(first->windowSetting()).arg(second.windowSetting()));

    const GraphWidget *shownGraph = nullptr;
    ValueRef added;
    target.graphs = [&] { return QList<std::pair<QString, GraphWidget *>>{ { "Graph", first }, { "Graph 2", &second } }; };
    target.showGraph = [&](const GraphWidget *g) { shownGraph = g; };
    target.newGraph = [&](const ValueRef &v) { added = v; };
    QMenu *menu = menuFor(ValueRef(1, source));
    QAction *right = action(menu, { "&Graph", "Graph", "&Right axis" });
    check(right && action(menu, { "&Graph", "Graph", "&Off" })->isChecked()
            && action(menu, { "&Graph", "Graph 2", "&Off" }) && !action(menu, { "&Graph", "Graph 2", "&Off" })->isChecked(),
          "two graphs: a submenu each, with what each shows");
    if (right)
      right->trigger();
    check(first->valueShown(ValueRef(1, source)) && first->valueAxis(ValueRef(1, source)) == GraphWidget::AxisRight
            && shownGraph == first && second.values() == QList<ValueRef>{ ValueRef(1, source) },
          "on the first graph's right axis: that graph alone, its window shown");
    if (QAction *a = action(menu, { "&Graph", "&New graph" }))
      a->trigger();
    check(added == ValueRef(1, source), "New graph: with the value");
    delete menu;
    target.graphs = nullptr;
    target.showGraph = nullptr;
    target.newGraph = nullptr;
    wid.removeGraph(view);
    check(wid.graph() == first && wid.graphs().size() == 1, "removed: the first graph is the current one");
    first->setValues({});
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
