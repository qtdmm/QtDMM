// Copyright (c) 2026 The QtDMM developers
// SPDX-License-Identifier: GPL-3.0-or-later
//
// The dialog of a formula node (PR 1b): a new one, devices ticked as inputs
// (SourceList) with their values v1, v1.2 ..., the unit proposed from the
// inputs and taken away again, a wrong formula keeps OK off, moving an input
// renames its variables, another leads, a loop greyed out, and what it
// gives back. Then the generator's dialog (PR 2): a sine to start with, a
// formula over the values above, removing one renames, the clock. And the
// integral's (PR 3): unit and name from the value, the threshold, its state.

#include <QSignalSpy>
#include <QtWidgets>

#include "ui/dialogs/formulanodedlg.h"
#include "ui/dialogs/generatornodedlg.h"
#include "ui/dialogs/integralnodedlg.h"
#include "ui/sourcelist.h"
#include "testutil.h"

int main(int argc, char **argv)
{
  if (qEnvironmentVariableIsEmpty("QT_QPA_PLATFORM"))
    qputenv("QT_QPA_PLATFORM", "offscreen");
  QApplication app(argc, argv);

  // three devices: a supply with its voltage, a load with current and a
  // second value, a probe not connected
  const QList<FormulaNodeDlg::Choice> choices {
    { "device:u", "Supply", { "Voltage" }, { "V" } },
    { "device:i", "Load", { "Current", "value 2" }, { "A", QString() } },
    { "device:w", "Probe", { "Voltage", "value 2" }, { "V", QString() }, false },
  };
  FormulaNodeDlg dlg;
  dlg.setChoices(choices);
  dlg.setCurrent([](const ValueRef &ref) -> std::optional<Reading>
  {
    if (ref.source == "device:u" && ref.id > 0)
      return std::nullopt;
    Reading r;
    r.msecs = 1;
    r.value = ref.source == "device:u" ? 12.0 : ref.id == 0 ? 0.5 : 2.0;
    return r;
  });
  dlg.load("Calculated", FormulaNode());
  auto *ok = dlg.findChild<QDialogButtonBox *>()->button(QDialogButtonBox::Ok);
  auto *formula = dlg.findChild<QLineEdit *>("ui_formula1");
  auto *unit = dlg.findChild<QComboBox *>("ui_formulaUnit1");
  auto *result = dlg.findChild<QLabel *>("ui_formulaResult1");
  auto *sources = dlg.findChild<SourceList *>("ui_formulaSources");
  auto *tree = dlg.findChild<QTreeWidget *>("ui_sourceTree");
  auto *list = dlg.findChild<QListWidget *>("ui_sourceInputs");
  check(formula && unit && result && sources && tree && list && !ok->isEnabled() && sources->inputs().isEmpty(),
        "a new one: no input, no formula, OK off");
  check(tree->topLevelItemCount() == 3 && tree->topLevelItem(2)->text(0).contains("not connected"),
        "all of My devices, one not connected said so: " + tree->topLevelItem(2)->text(0));

  // ticked: the supply, then the load - v1, v2; the load's values named under it
  tree->topLevelItem(0)->setCheckState(0, Qt::Checked);
  tree->topLevelItem(1)->setCheckState(0, Qt::Checked);
  check(sources->inputs() == QStringList({ "device:u", "device:i" }) && list->count() == 2
          && list->item(0)->text().startsWith("v1") && list->item(0)->text().contains("leads"),
        "ticked: v1 Supply (leads), v2 Load");
  // the list's view hears of the rows (the model's signals not blocked), or it shows none
  {
    QSignalSpy inserted(list->model(), &QAbstractItemModel::rowsInserted);
    tree->topLevelItem(1)->setCheckState(0, Qt::Unchecked);
    tree->topLevelItem(1)->setCheckState(0, Qt::Checked);
    check(!inserted.isEmpty() && list->count() == 2, "the inputs' list tells its view of its rows");
  }
  check(tree->topLevelItem(1)->child(0)->text(0).startsWith("v2 Current") && tree->topLevelItem(1)->child(1)->text(0).startsWith("v2.2 value 2")
          && !(tree->topLevelItem(1)->child(0)->flags() & Qt::ItemIsUserCheckable),
        "its values under it, named, not to tick: " + tree->topLevelItem(1)->child(1)->text(0));
  formula->setText("v1");
  check(unit->currentText() == "V", "v1 alone: V proposed, " + unit->currentText());
  formula->setText("v1 * v2");
  check(unit->currentText() == "W" && ok->isEnabled(), "v1 * v2: W proposed, OK on: " + unit->currentText());
  check(result->text().contains("6") && result->text().contains("W"), "its result now: " + result->text());
  formula->setText("v1 * v1");
  check(unit->currentText().isEmpty(), "V * V: the proposal taken away: " + unit->currentText());
  formula->setText("v1 * v2.1 * v2.2");
  check(ok->isEnabled() && result->text().contains("12"), "v2.1 is v2, v2.2 its second value: " + result->text());

  // a unit chosen stays
  formula->setText("v1 * v2");
  unit->setCurrentText("mW");
  formula->setText("v1 * v2 * 2");
  check(unit->currentText() == "mW", "a unit chosen stays: " + unit->currentText());

  // a variable that is no input, a value beyond four
  formula->setText("v1 * v3");
  check(!ok->isEnabled() && dlg.findChild<QLabel *>("ui_formulaStatus")->text().contains("v3"),
        "v3 unknown: OK off, said so");
  formula->setText("v2.5");
  check(!ok->isEnabled(), "v2.5: no fifth value");

  // the probe ticked as v3, used; moved up to v2: the formula follows
  tree->topLevelItem(2)->setCheckState(0, Qt::Checked);
  formula->setText("v1 * v3.2 + v3");
  check(ok->isEnabled(), "v3 an input now");
  sources->moveInput(2, 1);
  check(sources->inputs() == QStringList({ "device:u", "device:w", "device:i" }) && formula->text() == "v1 * v2.2 + v2",
        "the probe moved to v2, the formula follows: " + formula->text());
  // the load unticked: its v3 is unknown now
  formula->setText("v1 * v2 + v3");
  tree->topLevelItem(1)->setCheckState(0, Qt::Checked);   // as it is
  tree->topLevelItem(1)->setCheckState(0, Qt::Unchecked);
  check(sources->inputs() == QStringList({ "device:u", "device:w" }) && !ok->isEnabled(), "the load unticked: v3 unknown");
  // another leads
  list->setCurrentRow(1);
  dlg.findChild<QPushButton *>("ui_sourceLead")->click();
  check(sources->leading() == 1 && list->item(1)->text().contains("leads"), "v2 leads now");

  // a loop: that source greyed out, the inputs refused
  dlg.setLoopCheck([](const QList<ValueRef> &inputs)
  {
    for (const ValueRef &r : inputs)
      if (r.source == "device:i")
        return true;
    return false;
  });
  check(!(tree->topLevelItem(1)->flags() & Qt::ItemIsEnabled) && !tree->topLevelItem(1)->toolTip(0).isEmpty(),
        "the load would close a loop: greyed out, why: " + tree->topLevelItem(1)->toolTip(0));
  dlg.setLoopCheck({});

  // what it gives back
  dlg.findChild<QLineEdit *>("ui_formulaName")->setText("Power");
  dlg.findChild<QLineEdit *>("ui_formulaName1")->setText("P");
  formula->setText("v1 * v2");
  const std::unique_ptr<FormulaNode> node = dlg.node();
  check(dlg.name() == "Power" && node->inputs().size() == 2 && node->inputs()[1].toString() == "0@device:w"
          && node->leadingInput() == 1 && node->formulas().size() == 1
          && node->formulas()[0] == FormulaNode::Formula { "P", "mW", "v1 * v2" } && node->maxAgeMs() == 3000
          && node->parameters()["deviceInputs"].toBool(),
        "the node: " + node->toJsonText());

  // loaded again: as it was
  FormulaNodeDlg again;
  again.setChoices(choices);
  again.load(dlg.name(), *node);
  check(again.node()->toJsonText() == node->toJsonText() && again.name() == "Power", "loaded again: the same");

  // the generator's dialog
  {
    GeneratorNodeDlg gen;
    gen.load("Simulated", GeneratorNode());
    auto *genOk = gen.findChild<QDialogButtonBox *>()->button(QDialogButtonBox::Ok);
    auto *shape1 = gen.findChild<QComboBox *>("ui_generatorShape1");
    check(shape1 && shape1->currentData().toInt() == CalcExpr::Sine && genOk->isEnabled(), "a new one: a sine, OK on");
    QApplication::processEvents();
    check(gen.findChild<QLabel *>("ui_generatorResult1")->text().contains("V"), "where it is now: "
          + gen.findChild<QLabel *>("ui_generatorResult1")->text());
    gen.findChild<QPushButton *>("ui_generatorAdd")->click();
    auto *f2 = gen.findChild<QLineEdit *>("ui_generatorFormula2");
    check(f2 && gen.findChild<QComboBox *>("ui_generatorShape2")->currentData().toInt() == CalcExpr::Custom,
          "a value added: a formula");
    f2->setText("y2 + 1");
    check(!genOk->isEnabled(), "y2 in value 2: OK off");
    f2->setText("y1 * 2");
    check(genOk->isEnabled(), "y1 * 2: OK on");
    gen.findChild<QPushButton *>("ui_generatorAdd")->click();
    gen.findChild<QLineEdit *>("ui_generatorFormula3")->setText("y2 + t");
    gen.findChild<QComboBox *>("ui_generatorUnit3")->setCurrentText("°C");
    gen.findChild<QSpinBox *>("ui_generatorClock")->setValue(500);
    // value 1 removed: y2 of value 3 is y1 now; value 2's y1 is unknown
    QList<QToolButton *> removes;
    for (QToolButton *b : gen.findChildren<QToolButton *>())
      if (b->toolTip() == GeneratorNodeDlg::tr("Remove this value"))
        removes << b;
    if (!removes.isEmpty())
      removes.first()->click();
    check(gen.findChild<QLineEdit *>("ui_generatorFormula3")->text() == "y1 + t" && !genOk->isEnabled(),
          "value 1 removed: y2 is y1 now, y1 in the first: OK off");
    f2->setText("2 * t");
    const std::unique_ptr<GeneratorNode> n = gen.node();
    check(genOk->isEnabled() && n->periodMs() == 500 && n->waves().size() == 2 && n->waves()[0].formula == "2 * t"
            && n->waves()[1].unit == "°C" && n->outputs()[1].unit == "°C",
          "the generator: " + n->toJsonText());
    GeneratorNodeDlg again;
    again.load(gen.name(), *n);
    check(again.node()->toJsonText() == n->toJsonText(), "loaded again: the same");
    // a stored one (the gear of its front panel): its results go on live
    auto *result = again.findChild<QLabel *>("ui_generatorResult1");
    QApplication::processEvents();
    const QString before = result->text();
    QElapsedTimer wait;
    wait.start();
    while (wait.elapsed() < 700)
      QApplication::processEvents(QEventLoop::AllEvents, 50);
    check(result->text() != before, "a stored one's results go on: " + before + " -> " + result->text());
    // the same with the assistant's sine, stored as JSON and read back
    GeneratorNodeDlg fresh;
    fresh.load("Simulated", GeneratorNode());
    const std::unique_ptr<LogicNode> sine = LogicNode::fromJsonText(fresh.node()->toJsonText());
    GeneratorNodeDlg storedSine;
    storedSine.load("Simulated", *dynamic_cast<const GeneratorNode *>(sine.get()));
    auto *sineResult = storedSine.findChild<QLabel *>("ui_generatorResult1");
    QApplication::processEvents();
    const QString sineBefore = sineResult->text();
    wait.restart();
    while (wait.elapsed() < 700)
      QApplication::processEvents(QEventLoop::AllEvents, 50);
    check(sineResult->text() != sineBefore, "a stored sine goes on: " + sineBefore + " -> " + sineResult->text()
          + " " + sine->toJsonText());
  }

  // the integral's dialog: one value of a device, chosen in the tree
  {
    const QList<SourceList::Source> loads {
      { "device:p", "Load", { "Power", "Current" }, { "W", "A" } },
      { "device:x", "Sensor", { "value 1" }, { QString() } },
    };
    auto pick = [](QWidget &dlg, int device, int value)
    {
      auto *t = dlg.findChild<QTreeWidget *>("ui_sourceTree");
      t->setCurrentItem(t->topLevelItem(device)->child(value));
    };
    IntegralNodeDlg in;
    in.setSources(loads);
    in.load("Calculated", IntegralNode());
    auto *inOk = in.findChild<QDialogButtonBox *>()->button(QDialogButtonBox::Ok);
    auto *unitBox = in.findChild<QComboBox *>("ui_integralUnit");
    check(!inOk->isEnabled(), "a new one: no value yet, OK off");
    check(unitBox->currentText().isEmpty(), "a new one: no unit before its value gives one: " + unitBox->currentText());
    check(!(in.findChild<QTreeWidget *>("ui_sourceTree")->topLevelItem(0)->flags() & Qt::ItemIsSelectable),
          "a device itself is no value to choose");
    pick(in, 0, 0);
    check(unitBox->currentText() == "Wh" && in.name() == "Energy" && inOk->isEnabled(),
          "a power: Wh, named Energy: " + unitBox->currentText() + " " + in.name());
    check(in.findChild<QLabel *>("ui_integralThresholdUnit")->text() == "W", "the threshold in W, said beside it");
    check(!in.findChild<QPushButton *>("ui_integralZero")->isVisibleTo(&in), "a new one: no state, no Set to 0");
    pick(in, 0, 1);
    check(unitBox->currentText() == "Ah" && in.name() == "Charge"
            && in.findChild<QLabel *>("ui_integralThresholdUnit")->text() == "A", "a current: Ah, named Charge, threshold in A");
    unitBox->setCurrentText("mAh");
    pick(in, 1, 0);
    check(unitBox->currentText() == "mAh" && in.name() == "Integral", "a unit chosen stays; an unknown value: Integral");
    in.findChild<QCheckBox *>("ui_integralThresholdOn")->setChecked(true);
    in.findChild<QLineEdit *>("ui_integralThreshold")->setText("abc");
    check(!inOk->isEnabled(), "a threshold that is no number: OK off");
    in.findChild<QLineEdit *>("ui_integralThreshold")->setText("1.5k");
    in.findChild<QCheckBox *>("ui_integralOnRecording")->setChecked(false);
    check(inOk->isEnabled(), "1.5k: a number");
    const std::unique_ptr<IntegralNode> n = in.node();
    check(n->threshold() == 1500.0 && !n->resetsOnRecording() && n->unit() == "mAh" && n->inputs()[0].toString() == "0@device:x",
          "the integral: " + n->toJsonText());
    // a new one with its value given (Integrate... of a value's menu): chosen, proposed
    {
      IntegralNode given;
      given.setInputs({ ValueRef(1, "device:p") });
      IntegralNodeDlg menu;
      menu.setSources(loads);
      menu.load("Calculated", given);
      auto *t = menu.findChild<QTreeWidget *>("ui_sourceTree");
      check(menu.name() == "Charge" && menu.findChild<QComboBox *>("ui_integralUnit")->currentText() == "Ah"
              && t->currentItem() == t->topLevelItem(0)->child(1),
            "Integrate... of a current: chosen, Charge in Ah: " + menu.name());
    }
    // an open one: where it is now, Set to 0
    IntegralNodeDlg open;
    bool zeroed = false;
    open.setSources(loads);
    open.setState([]() -> std::optional<Reading>
                  {
                    Reading r;
                    r.msecs = 1;
                    r.value = 1234.5;
                    r.baseUnit = "Wh";
                    return r;
                  },
                  [&zeroed] { zeroed = true; });
    open.load("Energy", *n);
    check(open.findChild<QLabel *>("ui_integralNow")->text().contains("1,2345") || open.findChild<QLabel *>("ui_integralNow")->text().contains("1.2345"),
          "where it is now: " + open.findChild<QLabel *>("ui_integralNow")->text());
    open.findChild<QPushButton *>("ui_integralZero")->click();
    check(zeroed, "Set to 0");
    check(open.node()->unit() == "mAh" && open.name() == "Energy", "a stored one keeps its name and unit");
  }

  return failed == 0 ? 0 : 1;
}
