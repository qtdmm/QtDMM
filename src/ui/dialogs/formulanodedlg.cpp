// Copyright (c) 2026 The QtDMM developers
// SPDX-License-Identifier: GPL-3.0-or-later
#include "ui/dialogs/formulanodedlg.h"

#include <cmath>

#include <QButtonGroup>
#include <QComboBox>
#include <QDoubleSpinBox>
#include <QFormLayout>
#include <QGridLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QLocale>
#include <QLineEdit>
#include <QPushButton>
#include <QRadioButton>
#include <QToolButton>
#include <QVBoxLayout>

#include "core/calcexpr.h"
#include "core/sampletypes.h"
#include "ui/sourcelist.h"
#include "core/siprefix.h"

namespace
{
// more inputs would not fit a formula one reads
constexpr int kMaxInputs = 8;
// the units offered; any other may be typed
const char *const kUnits[] = { "", "V", "mV", "A", "mA", "W", "mW", "kW", "Ohm", "kOhm", "S", "Hz", "s", "J", "C", "%" };
}

FormulaNodeEditor::FormulaNodeEditor(QWidget *parent) :
  NodeEditor(parent)
{
  setObjectName("ui_formulaNodeEditor");
  auto *top = new QVBoxLayout(this);
  top->setContentsMargins(0, 0, 0, 0);
  auto *form = new QFormLayout;
  top->addLayout(form);
  m_name = new QLineEdit(this);
  m_name->setObjectName("ui_formulaName");
  m_name->setPlaceholderText(tr("e.g. Power"));
  form->addRow(tr("&Name:"), m_name);

  // the inputs: devices of My devices ticked, v1, v2 ... in their order; one leads
  auto *inputBox = new QGroupBox(tr("Inputs"), this);
  auto *inputLayout = new QVBoxLayout(inputBox);
  auto *inputBoxHint = new QLabel(tr("Computed whenever the leading input brings a new reading; "
                                    "the others give their last one."), inputBox);
  inputBoxHint->setWordWrap(true);   // not the dialog's minimum width
  inputLayout->addWidget(inputBoxHint);
  m_sources = new SourceList(SourceList::Devices, inputBox);
  m_sources->setObjectName("ui_formulaSources");
  inputLayout->addWidget(m_sources);
  top->addWidget(inputBox);
  connect(m_sources, &SourceList::inputsChanged, this, &FormulaNodeEditor::followInputs);

  // the outputs: y1 ... each name, formula, unit and its result now
  auto *outputBox = new QGroupBox(tr("Values"), this);
  auto *outputLayout = new QVBoxLayout(outputBox);
  auto *outputBoxHint = new QLabel(tr("Formulas over the inputs' values (v1, v1.2 ... as named in the list) in their base units "
                                     "(V, A, Ohm); a value may use the ones above it as y1, y2 ..."), outputBox);
  outputBoxHint->setWordWrap(true);   // not the dialog's minimum width
  outputLayout->addWidget(outputBoxHint);
  m_outputGrid = new QGridLayout;
  m_outputGrid->setColumnStretch(2, 1);
  outputLayout->addLayout(m_outputGrid);
  m_addOutput = new QPushButton(tr("Add &value"), outputBox);
  m_addOutput->setObjectName("ui_formulaAddOutput");
  auto *outputButtons = new QHBoxLayout;
  outputButtons->addWidget(m_addOutput);
  outputButtons->addStretch(1);
  outputLayout->addLayout(outputButtons);
  top->addWidget(outputBox);
  connect(m_addOutput, &QPushButton::clicked, this, [this]
  {
    addOutput({}, false);
    m_outputs.last().formula->setFocus();
    validate();
  });

  auto *ageForm = new QFormLayout;
  top->addLayout(ageForm);
  m_maxAge = new QDoubleSpinBox(this);
  m_maxAge->setObjectName("ui_formulaMaxAge");
  m_maxAge->setRange(0.1, 3600);
  m_maxAge->setDecimals(1);
  m_maxAge->setSuffix(" s");
  m_maxAge->setValue(3);
  m_maxAge->setToolTip(tr("An input whose last reading is older than this has no value: the result has a gap"));
  ageForm->addRow(tr("&Maximum age of an input:"), m_maxAge);

  m_status = new QLabel(this);
  m_status->setObjectName("ui_formulaStatus");
  m_status->setWordWrap(true);
  top->addWidget(m_status);
  top->addStretch(1);
  connect(m_name, &QLineEdit::textChanged, this, &FormulaNodeEditor::validate);

  m_live.setInterval(1000);
  connect(&m_live, &QTimer::timeout, this, &FormulaNodeEditor::updateResults);
  m_live.start();
}

void FormulaNodeEditor::setChoices(const QList<Choice> &choices)
{
  m_sources->setSources(choices);
}

void FormulaNodeEditor::setCurrent(std::function<std::optional<Reading>(const ValueRef &)> current)
{
  m_current = std::move(current);
  updateResults();
}

void FormulaNodeEditor::setLoopCheck(std::function<bool(const QList<ValueRef> &)> makesLoop)
{
  m_makesLoop = std::move(makesLoop);
  // one that would close a loop is greyed out in the list
  m_sources->setBlocked([this](const QString &source)
  {
    return m_makesLoop && m_makesLoop({ ValueRef(0, source) })
             ? tr("It takes the values of this calculated value, directly or through others: a loop.")
             : QString();
  });
  validate();
}

void FormulaNodeEditor::load(const QString &name, const FormulaNode &node)
{
  m_name->setText(name);
  m_on = node.isOn();
  m_periodMs = node.periodMs();
  m_maxAge->setValue(node.maxAgeMs() / 1000.0);
  QStringList in;
  for (const ValueRef &ref : node.inputs())
    in << ref.source;
  m_sources->setInputs(in, node.leadingInput());
  QList<FormulaNode::Formula> formulas = node.formulas();
  if (formulas.isEmpty())
    addOutput({}, false);
  for (const FormulaNode::Formula &f : std::as_const(formulas))
    addOutput(f, !f.unit.isEmpty());
  // a new one: its formula first, the name proposed is typed over
  if (formulas.isEmpty())
  {
    m_name->selectAll();
    m_outputs.first().formula->setFocus();
  }
  validate();
  updateResults();
}

QString FormulaNodeEditor::name() const
{
  return m_name->text().simplified();
}

QList<ValueRef> FormulaNodeEditor::inputs() const
{
  QList<ValueRef> out;
  for (const QString &source : m_sources->inputs())
    out << ValueRef(0, source);
  return out;
}

QList<FormulaNode::Formula> FormulaNodeEditor::formulas() const
{
  QList<FormulaNode::Formula> out;
  for (const OutputRow &row : m_outputs)
    out << FormulaNode::Formula { row.name->text().simplified(), row.unit->currentText().trimmed(),
                                  row.formula->text().trimmed(), row.coupling };
  return out;
}

std::unique_ptr<FormulaNode> FormulaNodeEditor::node() const
{
  auto node = std::make_unique<FormulaNode>();
  node->setInputs(inputs());
  node->setLeadingInput(m_sources->leading());
  node->setMaxAgeMs(int(std::lround(m_maxAge->value() * 1000)));
  node->setPeriodMs(m_periodMs);
  node->setOn(m_on);
  node->setFormulas(formulas());
  return node;
}

void FormulaNodeEditor::addOutput(const FormulaNode::Formula &formula, bool unitChosen)
{
  if (m_outputs.size() >= kMaxValues)
    return;
  const int index = int(m_outputs.size());
  OutputRow row;
  row.label = new QLabel(FormulaNode::outputName(index), this);
  row.name = new QLineEdit(formula.name, this);
  row.name->setObjectName(QString("ui_formulaName%1").arg(index + 1));
  row.name->setPlaceholderText(tr("Name"));
  row.name->setMaximumWidth(140);
  row.formula = new QLineEdit(formula.text, this);
  row.formula->setObjectName(QString("ui_formula%1").arg(index + 1));
  row.formula->setPlaceholderText(index == 0 ? tr("e.g. v1 * v2") : tr("e.g. y1 / 1000"));
  row.unit = new QComboBox(this);
  row.unit->setObjectName(QString("ui_formulaUnit%1").arg(index + 1));
  row.unit->setEditable(true);
  for (const char *u : kUnits)
    row.unit->addItem(QString::fromUtf8(u));
  row.unit->setCurrentText(formula.unit);
  row.unit->setToolTip(tr("The unit of the result, with a prefix when it is not in the base unit (mW: 0.012 W is 12 mW)"));
  row.result = new QLabel(this);
  row.result->setObjectName(QString("ui_formulaResult%1").arg(index + 1));
  row.result->setMinimumWidth(90);
  row.remove = new QToolButton(this);
  row.remove->setText(QStringLiteral("−"));
  row.remove->setToolTip(tr("Remove this value"));
  row.unitChosen = unitChosen;
  row.coupling = formula.coupling;
  m_outputs << row;
  connect(row.formula, &QLineEdit::textChanged, this, [this, edit = row.formula]
  {
    for (int i = 0; i < m_outputs.size(); ++i)
      if (m_outputs[i].formula == edit)
        propose(i);
    validate();
    updateResults();
  });
  connect(row.unit, &QComboBox::currentTextChanged, this, [this, unit = row.unit]
  {
    if (m_proposing)
      return;
    for (int i = 0; i < m_outputs.size(); ++i)
      if (m_outputs[i].unit == unit)
      {
        m_outputs[i].unitChosen = true;
        // the ones after it may take it as yN
        for (int j = i + 1; j < m_outputs.size(); ++j)
          propose(j);
      }
    updateResults();
  });
  connect(row.name, &QLineEdit::textChanged, this, &FormulaNodeEditor::validate);
  connect(row.remove, &QToolButton::clicked, this, [this, remove = row.remove]
  {
    for (int i = 0; i < m_outputs.size(); ++i)
      if (m_outputs[i].remove == remove)
      {
        removeOutput(i);
        break;
      }
  });
  relayout();
}

void FormulaNodeEditor::removeOutput(int index)
{
  if (index < 0 || index >= m_outputs.size() || m_outputs.size() <= 1)
    return;
  const OutputRow row = m_outputs.takeAt(index);
  for (QWidget *w : { static_cast<QWidget *>(row.label), static_cast<QWidget *>(row.name),
                      static_cast<QWidget *>(row.formula), static_cast<QWidget *>(row.unit),
                      static_cast<QWidget *>(row.result), static_cast<QWidget *>(row.remove) })
    w->deleteLater();
  for (int j = index; j < m_outputs.size(); ++j)
  {
    QString text = m_outputs[j].formula->text();
    for (int i = index + 1; i <= m_outputs.size(); ++i)
      text = CalcExpr::renameVariable(text, FormulaNode::outputName(i), FormulaNode::outputName(i - 1));
    const QSignalBlocker block(m_outputs[j].formula);
    m_outputs[j].formula->setText(text);
  }
  relayout();
  validate();
  updateResults();
}

void FormulaNodeEditor::followInputs(const QList<int> &from)
{
  // the formulas follow the inputs' new places: through names of their own,
  // so v1 -> v2 and v2 -> v1 do not meet; an input gone keeps its name (an
  // error there)
  for (OutputRow &out : m_outputs)
  {
    QString text = out.formula->text();
    for (int i = 0; i < from.size(); ++i)
      if (from[i] >= 0 && from[i] != i)
        text = FormulaNode::renameInput(text, from[i], 100 + i);
    for (int i = 0; i < from.size(); ++i)
      if (from[i] >= 0 && from[i] != i)
        text = FormulaNode::renameInput(text, 100 + i, i);
    if (text != out.formula->text())
    {
      const QSignalBlocker block(out.formula);
      out.formula->setText(text);
    }
  }
  for (int i = 0; i < m_outputs.size(); ++i)
    propose(i);
  validate();
  updateResults();
}

void FormulaNodeEditor::relayout()
{
  for (int i = 0; i < m_outputs.size(); ++i)
  {
    const OutputRow &row = m_outputs[i];
    row.label->setText(FormulaNode::outputName(i));
    m_outputGrid->addWidget(row.label, i, 0);
    m_outputGrid->addWidget(row.name, i, 1);
    m_outputGrid->addWidget(row.formula, i, 2);
    m_outputGrid->addWidget(row.unit, i, 3);
    m_outputGrid->addWidget(row.result, i, 4);
    m_outputGrid->addWidget(row.remove, i, 5);
    row.remove->setEnabled(m_outputs.size() > 1);
  }
  m_addOutput->setEnabled(m_outputs.size() < kMaxValues);
}

QMap<QString, QString> FormulaNodeEditor::units() const
{
  QMap<QString, QString> out;
  const QStringList in = m_sources->inputs();
  for (int i = 0; i < in.size(); ++i)
  {
    for (const Choice &c : m_sources->sources())
      if (c.source == in[i])
        for (int v = 0; v < c.units.size(); ++v)
        {
          out.insert(FormulaNode::inputName(i, v), c.units[v]);
          if (v == 0)
            out.insert(FormulaNode::inputName(i) + ".1", c.units[v]);
        }
  }
  for (int i = 0; i < m_outputs.size(); ++i)
    out.insert(FormulaNode::outputName(i), m_outputs[i].unit->currentText().trimmed());
  return out;
}

void FormulaNodeEditor::propose(int output)
{
  if (output < 0 || output >= m_outputs.size() || m_outputs[output].unitChosen)
    return;
  // half typed: the proposal stays; a whole formula that tells no unit
  // (V * V) takes it away
  const QString text = m_outputs[output].formula->text();
  if (!CalcExpr::parse(text))
    return;
  const std::optional<QString> unit = FormulaNode::suggestUnit(text, units());
  m_proposing = true;
  m_outputs[output].unit->setCurrentText(unit.value_or(QString()));
  m_proposing = false;
  // a later value may take this one's unit
  for (int j = output + 1; j < m_outputs.size(); ++j)
    propose(j);
}

void FormulaNodeEditor::validate()
{
  QStringList problems;
  if (name().isEmpty())
    problems << tr("The name is missing.");
  for (int i = 0; i < m_outputs.size(); ++i)
  {
    const QString text = m_outputs[i].formula->text();
    int pos = -1;
    const QString error = text.trimmed().isEmpty() ? tr("no formula") : FormulaNode::check(text, i, int(m_sources->inputs().size()), &pos);
    m_outputs[i].formula->setToolTip(error);
    m_outputs[i].formula->setStyleSheet(error.isEmpty() || text.trimmed().isEmpty() ? QString()
                                                                                    : QStringLiteral("color: #c0392b"));
    if (!error.isEmpty())
      problems << (pos >= 0 ? tr("%1: %2 (at %3)").arg(FormulaNode::outputName(i), error).arg(pos + 1)
                            : tr("%1: %2").arg(FormulaNode::outputName(i), error));
  }
  if (m_sources->inputs().size() > kMaxInputs)
    problems << tr("At most %1 inputs.").arg(kMaxInputs);
  if (m_makesLoop && m_makesLoop(inputs()))
    problems << tr("An input takes the values of this calculated value, directly or through others: a loop.");
  m_status->setText(problems.join("<br>"));
  setValid(problems.isEmpty());
}

void FormulaNodeEditor::updateResults()
{
  if (m_outputs.isEmpty())
    return;
  const std::unique_ptr<FormulaNode> n = node();
  LogicNode::Context ctx;
  for (const ValueRef &ref : n->inputs())
  {
    // each value of the device as it is now
    QMap<int, Reading> now;
    for (int v = 0; v < kMaxValues && m_current; ++v)
      if (const std::optional<Reading> r = m_current(ValueRef(v, ref.source)))
        now.insert(v, *r);
    ctx.values << now;
    ctx.inputs << (now.contains(0) ? std::optional(now[0]) : std::nullopt);
  }
  const LogicNode::Values values = n->evaluate(ctx);
  for (int i = 0; i < m_outputs.size(); ++i)
  {
    const std::optional<double> v = values.value(i);
    if (!v || !std::isfinite(*v))
    {
      m_outputs[i].result->setText(QStringLiteral("= —"));
      continue;
    }
    // shown as a meter would: the base value with the prefix it takes
    const SiPrefix::Split unit = SiPrefix::split(m_outputs[i].unit->currentText().trimmed());
    QString prefix;
    const double scaled = SiPrefix::scale(*v * SiPrefix::factor(unit.prefix), unit.baseUnit, &prefix);
    const QString number = QLocale().toString(scaled, 'g', 5);
    m_outputs[i].result->setText(QString("= %1 %2").arg(number, SiPrefix::displayText(prefix + unit.baseUnit)).trimmed());
  }
}

std::unique_ptr<LogicNode> FormulaNodeEditor::logicNode() const
{
  return node();
}

FormulaNodeDlg::FormulaNodeDlg(QWidget *parent) :
  NodeDlg(new FormulaNodeEditor, tr("Calculated value"), parent)
{
  setObjectName("ui_formulaNodeDlg");
  resize(640, 480);
}
