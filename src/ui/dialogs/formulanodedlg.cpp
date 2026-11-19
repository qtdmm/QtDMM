// Copyright (c) 2026 The QtDMM developers
// SPDX-License-Identifier: GPL-3.0-or-later
#include "ui/dialogs/formulanodedlg.h"

#include <cmath>

#include <QButtonGroup>
#include <QComboBox>
#include <QDialogButtonBox>
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
#include "core/siprefix.h"

namespace
{
// more inputs would not fit a formula one reads
constexpr int kMaxInputs = 8;
// the units offered; any other may be typed
const char *const kUnits[] = { "", "V", "mV", "A", "mA", "W", "mW", "kW", "Ohm", "kOhm", "S", "Hz", "s", "J", "C", "%" };
}

FormulaNodeDlg::FormulaNodeDlg(QWidget *parent) :
  QDialog(parent)
{
  setObjectName("ui_formulaNodeDlg");
  setWindowTitle(tr("Calculated value"));
  auto *top = new QVBoxLayout(this);
  auto *form = new QFormLayout;
  top->addLayout(form);
  m_name = new QLineEdit(this);
  m_name->setObjectName("ui_formulaName");
  m_name->setPlaceholderText(tr("e.g. Power"));
  form->addRow(tr("&Name:"), m_name);

  // the inputs: v1, v2 ... each a value of My devices; one leads
  auto *inputBox = new QGroupBox(tr("Inputs"), this);
  auto *inputLayout = new QVBoxLayout(inputBox);
  inputLayout->addWidget(new QLabel(tr("Computed whenever the leading input brings a new reading; "
                                       "the others give their last one."), inputBox));
  m_inputGrid = new QGridLayout;
  m_inputGrid->setColumnStretch(1, 1);
  inputLayout->addLayout(m_inputGrid);
  m_leading = new QButtonGroup(this);
  m_addInput = new QPushButton(tr("Add &input"), inputBox);
  m_addInput->setObjectName("ui_formulaAddInput");
  auto *inputButtons = new QHBoxLayout;
  inputButtons->addWidget(m_addInput);
  inputButtons->addStretch(1);
  inputLayout->addLayout(inputButtons);
  top->addWidget(inputBox);
  connect(m_addInput, &QPushButton::clicked, this, [this]
  {
    addInput(m_choices.isEmpty() ? QString() : m_choices.first().source);
    m_inputs.last().value->setFocus();
    validate();
  });
  connect(m_leading, &QButtonGroup::idClicked, this, &FormulaNodeDlg::validate);

  // the outputs: y1 ... each name, formula, unit and its result now
  auto *outputBox = new QGroupBox(tr("Values"), this);
  auto *outputLayout = new QVBoxLayout(outputBox);
  outputLayout->addWidget(new QLabel(tr("Formulas over the inputs' values (v1, v1.2 ... as listed under each) in their base units "
                                        "(V, A, Ohm); a value may use the ones above it as y1, y2 ..."), outputBox));
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
  auto *buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
  top->addWidget(buttons);
  m_ok = buttons->button(QDialogButtonBox::Ok);
  connect(buttons, &QDialogButtonBox::accepted, this, &QDialog::accept);
  connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
  connect(m_name, &QLineEdit::textChanged, this, &FormulaNodeDlg::validate);

  m_live.setInterval(1000);
  connect(&m_live, &QTimer::timeout, this, &FormulaNodeDlg::updateResults);
  m_live.start();
  resize(640, 480);
}

void FormulaNodeDlg::setChoices(const QList<Choice> &choices)
{
  m_choices = choices;
}

void FormulaNodeDlg::setCurrent(std::function<std::optional<Reading>(const ValueRef &)> current)
{
  m_current = std::move(current);
  updateResults();
}

void FormulaNodeDlg::setLoopCheck(std::function<bool(const QList<ValueRef> &)> makesLoop)
{
  m_makesLoop = std::move(makesLoop);
  validate();
}

void FormulaNodeDlg::load(const QString &name, const FormulaNode &node)
{
  m_name->setText(name);
  m_on = node.isOn();
  m_periodMs = node.periodMs();
  m_maxAge->setValue(node.maxAgeMs() / 1000.0);
  QStringList in;
  for (const ValueRef &ref : node.inputs())
    in << ref.source;
  if (in.isEmpty())
    in << (m_choices.isEmpty() ? QString() : m_choices.first().source);
  for (const QString &source : std::as_const(in))
    addInput(source);
  if (QAbstractButton *lead = m_leading->button(qMax(0, node.leadingInput())))
    lead->setChecked(true);
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

QString FormulaNodeDlg::name() const
{
  return m_name->text().simplified();
}

QList<ValueRef> FormulaNodeDlg::inputs() const
{
  QList<ValueRef> out;
  for (const InputRow &row : m_inputs)
    out << ValueRef(0, row.value->currentData().toString());
  return out;
}

QList<FormulaNode::Formula> FormulaNodeDlg::formulas() const
{
  QList<FormulaNode::Formula> out;
  for (const OutputRow &row : m_outputs)
    out << FormulaNode::Formula { row.name->text().simplified(), row.unit->currentText().trimmed(),
                                  row.formula->text().trimmed(), row.coupling };
  return out;
}

std::unique_ptr<FormulaNode> FormulaNodeDlg::node() const
{
  auto node = std::make_unique<FormulaNode>();
  node->setInputs(inputs());
  node->setLeadingInput(qMax(0, m_leading->checkedId()));
  node->setMaxAgeMs(int(std::lround(m_maxAge->value() * 1000)));
  node->setPeriodMs(m_periodMs);
  node->setOn(m_on);
  node->setFormulas(formulas());
  return node;
}

void FormulaNodeDlg::addInput(const QString &source)
{
  if (m_inputs.size() >= kMaxInputs)
    return;
  const int index = int(m_inputs.size());
  InputRow row;
  row.label = new QLabel(FormulaNode::inputName(index), this);
  row.value = new QComboBox(this);
  row.value->setObjectName(QString("ui_formulaInput%1").arg(index + 1));
  for (const Choice &c : std::as_const(m_choices))
  {
    // one not connected: there, grey, the node waits for it
    row.value->addItem(c.connected ? c.text : tr("%1 (not connected)").arg(c.text), c.source);
    if (!c.connected)
      row.value->setItemData(row.value->count() - 1, row.value->palette().color(QPalette::Disabled, QPalette::Text),
                             Qt::ForegroundRole);
  }
  int at = row.value->findData(source);
  if (at < 0)
  {
    // gone from My devices: kept, and said so
    row.value->addItem(tr("%1 (not in My devices)").arg(source), source);
    at = row.value->count() - 1;
  }
  row.value->setCurrentIndex(at);
  row.values = new QLabel(this);
  row.values->setWordWrap(true);
  row.values->setObjectName(QString("ui_formulaValues%1").arg(index + 1));
  QFont small = row.values->font();
  small.setPointSizeF(small.pointSizeF() * 0.9);
  row.values->setFont(small);
  row.lead = new QRadioButton(tr("leads"), this);
  row.lead->setToolTip(tr("A new reading of this input computes the values"));
  m_leading->addButton(row.lead, index);
  if (index == 0)
    row.lead->setChecked(true);
  row.remove = new QToolButton(this);
  row.remove->setText(QStringLiteral("−"));
  row.remove->setToolTip(tr("Remove this input"));
  m_inputs << row;
  showValues(index);
  connect(row.value, &QComboBox::currentIndexChanged, this, [this, combo = row.value]
  {
    for (int i = 0; i < m_inputs.size(); ++i)
      if (m_inputs[i].value == combo)
        showValues(i);
    // a unit not chosen yet follows the input
    for (int i = 0; i < m_outputs.size(); ++i)
      propose(i);
    validate();
    updateResults();
  });
  connect(row.remove, &QToolButton::clicked, this, [this, remove = row.remove]
  {
    for (int i = 0; i < m_inputs.size(); ++i)
      if (m_inputs[i].remove == remove)
      {
        removeInput(i);
        break;
      }
  });
  relayout();
}

void FormulaNodeDlg::showValues(int index)
{
  if (index < 0 || index >= m_inputs.size())
    return;
  const QString source = m_inputs[index].value->currentData().toString();
  QStringList parts;
  for (const Choice &c : std::as_const(m_choices))
    if (c.source == source)
      for (int v = 0; v < c.values.size(); ++v)
        parts << QString("%1 = %2").arg(FormulaNode::inputName(index, v), c.values[v]);
  m_inputs[index].values->setText(parts.join(QStringLiteral("  ·  ")));
}

void FormulaNodeDlg::addOutput(const FormulaNode::Formula &formula, bool unitChosen)
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
  connect(row.name, &QLineEdit::textChanged, this, &FormulaNodeDlg::validate);
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

void FormulaNodeDlg::removeInput(int index)
{
  if (index < 0 || index >= m_inputs.size() || m_inputs.size() <= 1)
    return;
  const bool led = m_inputs[index].lead->isChecked();
  const InputRow row = m_inputs.takeAt(index);
  m_leading->removeButton(row.lead);
  for (QWidget *w : std::initializer_list<QWidget *> { row.label, row.value, row.values, row.lead, row.remove })
    w->deleteLater();
  // the inputs after it move up: v3 becomes v2, in the formulas too
  for (OutputRow &out : m_outputs)
  {
    QString text = out.formula->text();
    for (int i = index + 1; i <= m_inputs.size(); ++i)
      text = FormulaNode::renameInput(text, i, i - 1);
    const QSignalBlocker block(out.formula);
    out.formula->setText(text);
  }
  for (int i = 0; i < m_inputs.size(); ++i)
    m_leading->setId(m_inputs[i].lead, i);
  if (led)
    m_inputs.first().lead->setChecked(true);
  relayout();
  validate();
  updateResults();
}

void FormulaNodeDlg::removeOutput(int index)
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

void FormulaNodeDlg::relayout()
{
  for (int i = 0; i < m_inputs.size(); ++i)
  {
    const InputRow &row = m_inputs[i];
    row.label->setText(FormulaNode::inputName(i));
    m_inputGrid->addWidget(row.label, 2 * i, 0);
    m_inputGrid->addWidget(row.value, 2 * i, 1);
    m_inputGrid->addWidget(row.lead, 2 * i, 2);
    m_inputGrid->addWidget(row.remove, 2 * i, 3);
    m_inputGrid->addWidget(row.values, 2 * i + 1, 1, 1, 3);
    showValues(i);
    row.remove->setEnabled(m_inputs.size() > 1);
  }
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
  m_addInput->setEnabled(m_inputs.size() < kMaxInputs);
  m_addOutput->setEnabled(m_outputs.size() < kMaxValues);
}

QMap<QString, QString> FormulaNodeDlg::units() const
{
  QMap<QString, QString> out;
  for (int i = 0; i < m_inputs.size(); ++i)
  {
    const QString source = m_inputs[i].value->currentData().toString();
    for (const Choice &c : m_choices)
      if (c.source == source)
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

void FormulaNodeDlg::propose(int output)
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

void FormulaNodeDlg::validate()
{
  QStringList problems;
  if (name().isEmpty())
    problems << tr("The name is missing.");
  for (int i = 0; i < m_outputs.size(); ++i)
  {
    const QString text = m_outputs[i].formula->text();
    int pos = -1;
    const QString error = text.trimmed().isEmpty() ? tr("no formula") : FormulaNode::check(text, i, int(m_inputs.size()), &pos);
    m_outputs[i].formula->setToolTip(error);
    m_outputs[i].formula->setStyleSheet(error.isEmpty() || text.trimmed().isEmpty() ? QString()
                                                                                    : QStringLiteral("color: #c0392b"));
    if (!error.isEmpty())
      problems << (pos >= 0 ? tr("%1: %2 (at %3)").arg(FormulaNode::outputName(i), error).arg(pos + 1)
                            : tr("%1: %2").arg(FormulaNode::outputName(i), error));
  }
  if (m_makesLoop && m_makesLoop(inputs()))
    problems << tr("An input takes the values of this calculated value, directly or through others: a loop.");
  m_status->setText(problems.join("<br>"));
  if (m_ok)
    m_ok->setEnabled(problems.isEmpty());
}

void FormulaNodeDlg::updateResults()
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
