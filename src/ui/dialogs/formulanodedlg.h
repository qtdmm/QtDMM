// Copyright (c) 2026 The QtDMM developers
// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <QList>
#include <QTimer>
#include <functional>
#include <memory>
#include <optional>

#include "core/reading.h"
#include "core/valueref.h"
#include "logic/formulanode.h"
#include "ui/dialogs/nodeeditor.h"
#include "ui/sourcelist.h"

class QButtonGroup;
class QComboBox;
class QDoubleSpinBox;
class QGridLayout;
class QLabel;
class QLineEdit;
class QPushButton;
class QRadioButton;
class QToolButton;

/// The settings of a formula node (FormulaNode): its name, its inputs
/// v1..vn - any device of My devices, also one not connected, which the
/// node then waits for; its values are v1, v1.2 ... -, which of them leads, up to kMaxValues outputs
/// with name, formula and unit, and the maximum age of an input. The unit
/// is proposed from the inputs' units (V * A -> W) until one is chosen;
/// each output shows its result with the inputs as they are now. A formula
/// that does not parse, a variable that is no input and inputs that close a
/// loop keep OK off and say why.
class FormulaNodeEditor : public NodeEditor
{
  Q_OBJECT
public:
  /// A device one may take as an input, with its values.
  using Choice = SourceList::Source;

  explicit FormulaNodeEditor(QWidget *parent = nullptr);

  /// The values to choose from, in that order.
  void        setChoices(const QList<Choice> &choices);
  /// The current reading of a device's value, for the results shown; none: none.
  void        setCurrent(std::function<std::optional<Reading>(const ValueRef &ref)> current);
  /// Whether these inputs would close a loop (editing a node others take).
  void        setLoopCheck(std::function<bool(const QList<ValueRef> &inputs)> makesLoop);

  /// Fills in @p name and @p node; a new node: one input, one output.
  void        load(const QString &name, const FormulaNode &node);
  QString     name() const override;
  /// The node as set, its on/off as loaded.
  std::unique_ptr<FormulaNode> node() const;
  std::unique_ptr<LogicNode> logicNode() const override;
  QString     model() const override { return QStringLiteral("QtDMM Formula"); }

private:
  struct OutputRow
  {
    QLabel      *label;
    QLineEdit   *name;
    QLineEdit   *formula;
    QComboBox   *unit;
    QLabel      *result;
    QToolButton *remove;
    bool         unitChosen = false;   ///< no more proposals once the unit was chosen
    QString      coupling;             ///< kept as loaded (no field for it)
  };

  void        addOutput(const FormulaNode::Formula &formula, bool unitChosen);
  void        removeOutput(int index);
  /// The rows anew in their grids, after one came or went.
  void        relayout();
  /// The inputs ticked anew (SourceList::inputsChanged): the formulas'
  /// variables follow their places.
  void        followInputs(const QList<int> &from);
  QList<ValueRef> inputs() const;
  QList<FormulaNode::Formula> formulas() const;
  /// The units of v1.. and y1.. for the proposal.
  QMap<QString, QString> units() const;
  void        propose(int output);
  /// Checks everything: the reason in the status line, OK on or off.
  void        validate();
  /// The results with the current readings.
  void        updateResults();

  QLineEdit      *m_name;
  SourceList     *m_sources;
  QGridLayout    *m_outputGrid;
  QPushButton    *m_addOutput;
  QDoubleSpinBox *m_maxAge;
  QLabel         *m_status;
  QList<OutputRow> m_outputs;
  bool             m_on = true;
  int              m_periodMs = 0;
  bool             m_proposing = false;
  QTimer           m_live;
  std::function<std::optional<Reading>(const ValueRef &)> m_current;
  std::function<bool(const QList<ValueRef> &)> m_makesLoop;
};

/// FormulaNodeEditor in a dialog of its own (Info and settings of a
/// calculated value).
class FormulaNodeDlg : public NodeDlg
{
  Q_OBJECT
public:
  using Choice = FormulaNodeEditor::Choice;

  explicit FormulaNodeDlg(QWidget *parent = nullptr);
  FormulaNodeEditor *formulaEditor() const { return static_cast<FormulaNodeEditor *>(editor()); }
  void        setChoices(const QList<Choice> &choices) { formulaEditor()->setChoices(choices); }
  void        setCurrent(std::function<std::optional<Reading>(const ValueRef &ref)> current)
  { formulaEditor()->setCurrent(std::move(current)); }
  void        setLoopCheck(std::function<bool(const QList<ValueRef> &inputs)> makesLoop)
  { formulaEditor()->setLoopCheck(std::move(makesLoop)); }
  void        load(const QString &name, const FormulaNode &node) { formulaEditor()->load(name, node); }
  QString     name() const { return formulaEditor()->name(); }
  std::unique_ptr<FormulaNode> node() const { return formulaEditor()->node(); }
};
