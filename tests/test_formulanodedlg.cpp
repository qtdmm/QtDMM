// Copyright (c) 2026 The QtDMM developers
// SPDX-License-Identifier: GPL-3.0-or-later
//
// The dialog of a formula node (PR 1b): a new one, the unit proposed from
// the inputs and taken away again, a wrong formula keeps OK off, removing
// an input renames the variables after it, a loop is refused, and what it
// gives back.

#include <QtWidgets>

#include "ui/dialogs/formulanodedlg.h"
#include "testutil.h"

int main(int argc, char **argv)
{
  if (qEnvironmentVariableIsEmpty("QT_QPA_PLATFORM"))
    qputenv("QT_QPA_PLATFORM", "offscreen");
  QApplication app(argc, argv);

  const QList<FormulaNodeDlg::Choice> choices {
    { ValueRef(0, "device:u"), "Supply: Voltage", "V" },
    { ValueRef(0, "device:i"), "Load: Current", "A" },
    { ValueRef(1, "device:i"), "Load: value 2", QString() },
  };
  FormulaNodeDlg dlg;
  dlg.setChoices(choices);
  dlg.setCurrent([](const ValueRef &ref) -> std::optional<Reading>
  {
    Reading r;
    r.msecs = 1;
    r.value = ref.source == "device:u" ? 12.0 : 0.5;
    return r;
  });
  dlg.load("Calculated", FormulaNode());
  auto *ok = dlg.findChild<QDialogButtonBox *>()->button(QDialogButtonBox::Ok);
  auto *formula = dlg.findChild<QLineEdit *>("ui_formula1");
  auto *unit = dlg.findChild<QComboBox *>("ui_formulaUnit1");
  auto *result = dlg.findChild<QLabel *>("ui_formulaResult1");
  check(formula && unit && result && !ok->isEnabled(), "a new one: one value, no formula, OK off");

  // a second input: the current, then v1 * v2 in W
  dlg.findChild<QPushButton *>("ui_formulaAddInput")->click();
  auto *input2 = dlg.findChild<QComboBox *>("ui_formulaInput2");
  check(input2 != nullptr, "a second input");
  input2->setCurrentIndex(1);
  formula->setText("v1");
  check(unit->currentText() == "V", "v1 alone: V proposed, " + unit->currentText());
  formula->setText("v1 * v2");
  check(unit->currentText() == "W" && ok->isEnabled(), "v1 * v2: W proposed, OK on: " + unit->currentText());
  check(result->text().contains("6") && result->text().contains("W"), "its result now: " + result->text());
  formula->setText("v1 * v1");
  check(unit->currentText().isEmpty(), "V * V: the proposal taken away: " + unit->currentText());

  // a unit chosen stays
  formula->setText("v1 * v2");
  unit->setCurrentText("mW");
  formula->setText("v1 * v2 * 2");
  check(unit->currentText() == "mW", "a unit chosen stays: " + unit->currentText());

  // a variable that is no input
  formula->setText("v1 * v3");
  check(!ok->isEnabled() && dlg.findChild<QLabel *>("ui_formulaStatus")->text().contains("v3"),
        "v3 unknown: OK off, said so");

  // a third input, v3 used, then v2 removed: v3 becomes v2
  dlg.findChild<QPushButton *>("ui_formulaAddInput")->click();
  dlg.findChild<QComboBox *>("ui_formulaInput3")->setCurrentIndex(2);
  formula->setText("v1 * v3");
  check(ok->isEnabled(), "v3 an input now");
  QList<QToolButton *> removes;
  for (QToolButton *b : dlg.findChildren<QToolButton *>())
    if (b->toolTip() == FormulaNodeDlg::tr("Remove this input"))
      removes << b;
  check(removes.size() == 3, "three inputs to remove");
  if (removes.size() == 3)
    removes[1]->click();
  check(formula->text() == "v1 * v2", "v2 removed, v3 is v2 now: " + formula->text());

  // a loop
  dlg.setLoopCheck([](const QList<ValueRef> &inputs) { return inputs.size() > 1 && inputs[1].id == 1; });
  check(!ok->isEnabled(), "inputs that close a loop: OK off");
  dlg.setLoopCheck({});

  // what it gives back
  dlg.findChild<QLineEdit *>("ui_formulaName")->setText("Power");
  dlg.findChild<QLineEdit *>("ui_formulaName1")->setText("P");
  const std::unique_ptr<FormulaNode> node = dlg.node();
  check(dlg.name() == "Power" && node->inputs().size() == 2 && node->inputs()[1].toString() == "1@device:i"
          && node->formulas().size() == 1 && node->formulas()[0] == FormulaNode::Formula { "P", "mW", "v1 * v2" }
          && node->maxAgeMs() == 3000,
        "the node: " + node->toJsonText());

  // loaded again: as it was
  FormulaNodeDlg again;
  again.setChoices(choices);
  again.load(dlg.name(), *node);
  check(again.node()->toJsonText() == node->toJsonText() && again.name() == "Power", "loaded again: the same");

  return failed == 0 ? 0 : 1;
}
