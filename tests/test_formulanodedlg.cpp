// Copyright (c) 2026 The QtDMM developers
// SPDX-License-Identifier: GPL-3.0-or-later
//
// The dialog of a formula node (PR 1b): a new one, devices as inputs with
// their values v1, v1.2 ..., the unit proposed from the inputs and taken
// away again, a wrong formula keeps OK off, removing an input renames the
// variables after it, a loop is refused, and what it
// gives back. Then the generator's dialog (PR 2): a sine to start with, a
// formula over the values above, removing one renames, the clock.

#include <QtWidgets>

#include "ui/dialogs/formulanodedlg.h"
#include "ui/dialogs/generatornodedlg.h"
#include "testutil.h"

int main(int argc, char **argv)
{
  if (qEnvironmentVariableIsEmpty("QT_QPA_PLATFORM"))
    qputenv("QT_QPA_PLATFORM", "offscreen");
  QApplication app(argc, argv);

  // two devices: a supply with its voltage, a load with current and a second value
  const QList<FormulaNodeDlg::Choice> choices {
    { "device:u", "Supply", { "Voltage" }, { "V" } },
    { "device:i", "Load", { "Current", "value 2" }, { "A", QString() } },
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
  check(formula && unit && result && !ok->isEnabled(), "a new one: one value, no formula, OK off");

  // a second input: the load, its values listed under it
  dlg.findChild<QPushButton *>("ui_formulaAddInput")->click();
  auto *input2 = dlg.findChild<QComboBox *>("ui_formulaInput2");
  check(input2 != nullptr, "a second input");
  input2->setCurrentIndex(1);
  const QString listed = dlg.findChild<QLabel *>("ui_formulaValues2")->text();
  check(listed.contains("v2 = Current") && listed.contains("v2.2 = value 2"), "its values: " + listed);
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

  // a third input, used with its second value, then v2 removed: v3 becomes v2
  dlg.findChild<QPushButton *>("ui_formulaAddInput")->click();
  dlg.findChild<QComboBox *>("ui_formulaInput3")->setCurrentIndex(1);
  formula->setText("v1 * v3.2 + v3");
  check(ok->isEnabled(), "v3 an input now");
  QList<QToolButton *> removes;
  for (QToolButton *b : dlg.findChildren<QToolButton *>())
    if (b->toolTip() == FormulaNodeDlg::tr("Remove this input"))
      removes << b;
  check(removes.size() == 3, "three inputs to remove");
  if (removes.size() == 3)
    removes[1]->click();
  check(formula->text() == "v1 * v2.2 + v2", "v2 removed, v3 is v2 now: " + formula->text());

  // a loop
  dlg.setLoopCheck([](const QList<ValueRef> &inputs) { return inputs.size() > 1 && inputs[1].source == "device:i"; });
  check(!ok->isEnabled(), "inputs that close a loop: OK off");
  dlg.setLoopCheck({});

  // what it gives back
  dlg.findChild<QLineEdit *>("ui_formulaName")->setText("Power");
  dlg.findChild<QLineEdit *>("ui_formulaName1")->setText("P");
  formula->setText("v1 * v2");
  const std::unique_ptr<FormulaNode> node = dlg.node();
  check(dlg.name() == "Power" && node->inputs().size() == 2 && node->inputs()[1].toString() == "0@device:i"
          && node->formulas().size() == 1 && node->formulas()[0] == FormulaNode::Formula { "P", "mW", "v1 * v2" }
          && node->maxAgeMs() == 3000 && node->parameters()["deviceInputs"].toBool(),
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
  }

  return failed == 0 ? 0 : 1;
}
