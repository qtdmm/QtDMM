// Copyright (c) 2026 The QtDMM developers
// SPDX-License-Identifier: GPL-3.0-or-later
//
// SourcesDlg, "Sources..." of a view: the open meters with their values,
// several ticked for the graph, one for the analog meter.
#include <QApplication>
#include <QComboBox>
#include <QDebug>
#include <QTreeWidget>

#include "ui/dialogs/panelvaluesdlg.h"
#include "ui/dialogs/sourcesdlg.h"
#include "testutil.h"

int main(int argc, char **argv)
{
  if (qEnvironmentVariableIsEmpty("QT_QPA_PLATFORM"))
    qputenv("QT_QPA_PLATFORM", "offscreen");
  QApplication app(argc, argv);
  const QList<SourcesDlg::Meter> meters { { QString(), "Sim A", { "Voltage DC (V)" } },
                                          { "device:b", "Sim B", { "Current DC (A)", "Voltage DC (V)" } } };
  auto value = [](SourcesDlg &dlg, int meter, int id) { return dlg.tree()->topLevelItem(meter)->child(id); };

  // 0. Values... of a front panel: the places as far as the device has
  //    values, a value chosen twice swaps
  {
    PanelValuesDlg dlg({ "V", "A", "W", "Hz" }, 0, { 1 });
    check(dlg.place(2) && !dlg.place(3), "the big line and two on the small one");
    check(dlg.main() == 0 && dlg.secondaries() == QList<int>({ 1 }), "as the panel has them");
    auto choose = [&](int place, int id)
    {
      QComboBox *box = dlg.place(place);
      box->setCurrentIndex(box->findData(id));
      Q_EMIT box->activated(box->currentIndex());
    };
    choose(2, 2);
    check(dlg.secondaries() == QList<int>({ 1, 2 }), "a third value on the right");
    choose(0, 1);
    check(dlg.main() == 1 && dlg.secondaries() == QList<int>({ 0, 2 }), "the value on the left to the big line: they swap");
    choose(1, -1);
    check(dlg.main() == 1 && dlg.secondaries() == QList<int>({ 2 }), "None leaves its place empty");
    PanelValuesDlg two({ "V", "A" }, 1, { 0 });
    check(two.place(1) && !two.place(2) && two.main() == 1, "two values: one place on the small line");
  }

  // 1. several: what was ticked, more ticked
  {
    SourcesDlg dlg("Graph", true, meters);
    check(dlg.tree()->topLevelItemCount() == 2 && dlg.tree()->topLevelItem(1)->childCount() == 2,
          "two meters, Sim B with two values");
    dlg.setChosen({ ValueRef(0), ValueRef(1, "device:b") });
    check(dlg.chosen() == QList<ValueRef>({ ValueRef(0), ValueRef(1, "device:b") }), "the values chosen before");
    value(dlg, 1, 0)->setCheckState(0, Qt::Checked);
    check(dlg.chosen().size() == 3, "a third one ticked");
  }
  // 2. one: ticking another unticks the one before
  {
    SourcesDlg dlg("Analog meter", false, meters);
    dlg.setChosen({ ValueRef(0) });
    value(dlg, 1, 1)->setCheckState(0, Qt::Checked);
    check(dlg.chosen() == QList<ValueRef>{ ValueRef(1, "device:b") }, "one value only");
  }

  if (failed)
  {
    qWarning() << failed << "SourcesDlg check(s) failed";
    return 1;
  }
  qInfo() << "All SourcesDlg tests passed.";
  return 0;
}
