// Copyright (c) 2026 The QtDMM developers
// SPDX-License-Identifier: GPL-3.0-or-later
#include "ui/valuemenu.h"

#include <QActionGroup>
#include <QMenu>
#include <QPointer>

#include "service/metercontroller.h"
#include "ui/frontpanel.h"
#include "ui/graphmodel.h"

namespace
{
constexpr int kOff = -2;   ///< the entry "Off" of a graph: not GraphModel::kAxisAuto
}

void ValueMenu::fill(QMenu *menu, const Target &target, const ValueRef &value)
{
  menu->setToolTipsVisible(true);
  auto checkable = [](QMenu *m, const QString &text, bool on)
  {
    QAction *a = m->addAction(text);
    a->setCheckable(true);
    a->setChecked(on);
    return a;
  };
  const ValueRef ref = value;
  const int id = value.id;
  const auto show = target.show;
  const QString source = ref.source;
  FrontPanel *panel = target.panel ? target.panel(source) : nullptr;

  // the LCD of the meter's front panel: three places, big and the two on
  // the small line. A choice takes the place; the value there goes where
  // this one was, or off the display when it had none
  QMenu *lcd = menu->addMenu(tr("&LCD"));
  auto *places = new QActionGroup(lcd);
  QPointer<FrontPanel> open(panel);   // it goes with its device, also while the menu is open
  auto mainOf = [open]() { return open ? open->value().id : -1; };
  auto smallOf = [open]() { return open ? open->secondaries() : QList<int>(); };
  const int values = panel && panel->controller() ? panel->controller()->valueCount() : 1;
  const QList<int> small = smallOf();
  const int taken[] = { mainOf(), int(small.value(0, -1)), int(small.value(1, -1)) };
  const QString texts[] = { tr("&Main value"), tr("Secondary value &left"), tr("Secondary value &right") };
  lcd->setEnabled(panel);
  for (int place = 0; place < 3; ++place)
  {
    QAction *a = checkable(lcd, texts[place], taken[place] == id);
    places->addAction(a);
    // a meter with two values has one place on the small line
    a->setEnabled(place == 0 || place < values);
    QObject::connect(a, &QAction::triggered, menu, [open, id, place, show, source, mainOf, smallOf]
    {
      if (!open)
        return;
      const QList<int> small = smallOf();
      QList<int> now { mainOf(), int(small.value(0, -1)), int(small.value(1, -1)) };
      const qsizetype from = now.indexOf(id);
      if (from >= 0)
        now[from] = now[place];
      now[place] = id;
      QList<int> line;
      for (int i = 1; i < 3; ++i)
        if (now[i] >= 0)
          line << now[i];
      if (now[0] >= 0)
      {
        open->setValue(ValueRef(now[0], source));
        // its min/max memory follows the big line
        if (MeterController *ctl = open->controller())
          ctl->setMinMaxValue(now[0]);
      }
      open->setSecondaries(line);
      show(View::Lcd, source);
    });
  }
  // the analog meters: one entry, with more than one a submenu by their windows
  // the analog meters and the Poincaré plot choose their value with
  // Sources... in their own menu; here a new analog meter for it
  if (target.newMeter)
    QObject::connect(menu->addAction(tr("New &needle meter")), &QAction::triggered, menu,
                     [add = target.newMeter, ref] { add(ref); });
  if (target.newPoincare)
    QObject::connect(menu->addAction(tr("New &Poincaré plot")), &QAction::triggered, menu,
                     [add = target.newPoincare, ref] { add(ref); });
  if (target.newHistogram)
    QObject::connect(menu->addAction(tr("New &histogram")), &QAction::triggered, menu,
                     [add = target.newHistogram, ref] { add(ref); });
  if (target.newHeatmap)
    QObject::connect(menu->addAction(tr("New heatmap")), &QAction::triggered, menu,
                     [add = target.newHeatmap, ref] { add(ref); });
  // the graphs: off, or on an axis (Automatic: the axis of its unit, else
  // the next free one)
  QList<std::pair<QString, GraphModel *>> graphs;
  if (target.graphs)
    graphs = target.graphs();
  // one graph: its axes at once; several: a submenu each, by their windows
  QMenu *graphMenu = menu->addMenu(tr("&Graph"));
  for (const auto &[title, graph] : std::as_const(graphs))
  {
    QMenu *curves = graphs.size() > 1 ? graphMenu->addMenu(QString(title).replace('&', "&&")) : graphMenu;
    auto *sides = new QActionGroup(curves);
    const bool shown = graph->values().contains(ref);
    const int chosen = graph->valueAxis(ref);
    const std::pair<int, QString> choices[] = { { kOff, tr("&Off") },
                                                { GraphModel::kAxisAuto, tr("&Automatic") },
                                                { 0, tr("&Left axis") },
                                                { 1, tr("&Right axis") },
                                                { 2, tr("&Third axis") } };
    for (const auto &[axis, text] : choices)
    {
      QAction *a = checkable(curves, text, axis == kOff ? !shown : shown && chosen == axis);
      sides->addAction(a);
      QObject::connect(a, &QAction::triggered, menu, [model = QPointer<GraphModel>(graph), ref, axis = axis, show,
                                                      showGraph = target.showGraph]
      {
        if (!model)
          return;
        QList<ValueRef> values = model->values();
        if (axis == kOff)
        {
          values.removeAll(ref);
          model->setValues(values);
          return;
        }
        model->setValueAxis(ref, axis);
        if (!values.contains(ref))
          model->setValues(values << ref);
        if (showGraph)
          showGraph(model);
        else
          show(View::Graph, ref.source);
      });
    }
  }
  if (target.newGraph)
  {
    graphMenu->addSeparator();
    QObject::connect(graphMenu->addAction(tr("&New graph")), &QAction::triggered, menu,
                     [add = target.newGraph, ref] { add(ref); });
  }
  // the readings table: all values at first; copying follows it, the
  // export keeps them all
  menu->addSeparator();
  QAction *table = checkable(menu, tr("Readings &table"), target.tableShown(ref));
  table->setToolTip(tr("Show this value's readings in the readings table"));
  QObject::connect(table, &QAction::triggered, menu, [set = target.setTableShown, ref, show](bool on)
  {
    set(ref, on);
    if (on)
      show(View::Table, ref.source);
  });
  // the rules on it, kept with its meter
  if (target.addAlarm || target.editLimits)
    menu->addSeparator();
  if (target.addAlarm)
    QObject::connect(menu->addAction(tr("&New alarm...")), &QAction::triggered, menu,
                     [add = target.addAlarm, ref] { add(ref); });
  if (target.editLimits)
    QObject::connect(menu->addAction(tr("Li&mits...")), &QAction::triggered, menu,
                     [edit = target.editLimits, ref] { edit(ref); });
}
