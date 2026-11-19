// Copyright (c) 2026 The QtDMM developers
// SPDX-License-Identifier: GPL-3.0-or-later
#include "ui/dialogs/generatornodedlg.h"

#include <cmath>

#include <QComboBox>
#include <QDialogButtonBox>
#include <QFormLayout>
#include <QGridLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QLocale>
#include <QPushButton>
#include <QSpinBox>
#include <QStackedWidget>
#include <QToolButton>
#include <QVBoxLayout>

#include "core/sampletypes.h"
#include "core/siprefix.h"

namespace
{
const char *const kUnits[] = { "V", "mV", "A", "mA", "W", "Ohm", "Hz", "°C", "%", "" };
}

GeneratorNodeDlg::GeneratorNodeDlg(QWidget *parent) :
  QDialog(parent)
{
  setObjectName("ui_generatorNodeDlg");
  setWindowTitle(tr("Simulated values"));
  auto *top = new QVBoxLayout(this);
  auto *form = new QFormLayout;
  top->addLayout(form);
  m_name = new QLineEdit(this);
  m_name->setObjectName("ui_generatorName");
  m_name->setPlaceholderText(tr("e.g. Test signal"));
  form->addRow(tr("&Name:"), m_name);
  m_clock = new QSpinBox(this);
  m_clock->setObjectName("ui_generatorClock");
  m_clock->setRange(50, 60000);
  m_clock->setSingleStep(50);
  m_clock->setSuffix(" ms");
  m_clock->setValue(GeneratorNode::kDefaultPeriodMs);
  m_clock->setToolTip(tr("A new reading of all values every so often"));
  form->addRow(tr("&Every:"), m_clock);

  auto *box = new QGroupBox(tr("Values"), this);
  auto *boxLayout = new QVBoxLayout(box);
  boxLayout->addWidget(new QLabel(tr("A shape between min and max with its period in s, or a formula over t (the "
                                     "seconds since the start) and the values above it as y1, y2 ..."), box));
  m_grid = new QGridLayout;
  m_grid->setColumnStretch(5, 1);
  boxLayout->addLayout(m_grid);
  m_add = new QPushButton(tr("Add &value"), box);
  m_add->setObjectName("ui_generatorAdd");
  auto *buttons = new QHBoxLayout;
  buttons->addWidget(m_add);
  buttons->addStretch(1);
  boxLayout->addLayout(buttons);
  top->addWidget(box);
  connect(m_add, &QPushButton::clicked, this, [this]
  {
    GeneratorNode::Wave w;
    w.shape = CalcExpr::Custom;
    addRow(w);
    m_rows.last().formula->setFocus();
    validate();
  });

  m_status = new QLabel(this);
  m_status->setObjectName("ui_generatorStatus");
  m_status->setWordWrap(true);
  top->addWidget(m_status);
  top->addStretch(1);
  auto *bb = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
  top->addWidget(bb);
  m_ok = bb->button(QDialogButtonBox::Ok);
  connect(bb, &QDialogButtonBox::accepted, this, &QDialog::accept);
  connect(bb, &QDialogButtonBox::rejected, this, &QDialog::reject);
  connect(m_name, &QLineEdit::textChanged, this, &GeneratorNodeDlg::validate);

  m_live.setInterval(250);
  connect(&m_live, &QTimer::timeout, this, &GeneratorNodeDlg::updateResults);
  m_live.start();
  m_since.start();
  resize(760, 360);
}

void GeneratorNodeDlg::load(const QString &name, const GeneratorNode &node)
{
  m_name->setText(name);
  m_on = node.isOn();
  m_maxAgeMs = node.maxAgeMs();
  m_clock->setValue(node.periodMs() > 0 ? node.periodMs() : GeneratorNode::kDefaultPeriodMs);
  QList<GeneratorNode::Wave> waves = node.waves();
  if (waves.isEmpty())
  {
    waves << GeneratorNode::Wave();
    m_name->selectAll();
  }
  for (const GeneratorNode::Wave &w : std::as_const(waves))
    addRow(w);
  validate();
}

QString GeneratorNodeDlg::name() const
{
  return m_name->text().simplified();
}

QList<GeneratorNode::Wave> GeneratorNodeDlg::waves() const
{
  QList<GeneratorNode::Wave> out;
  for (const Row &r : m_rows)
  {
    GeneratorNode::Wave w;
    w.name = r.name->text().simplified();
    w.unit = r.unit->currentText().trimmed();
    w.coupling = r.coupling->currentData().toString();
    w.shape = static_cast<CalcExpr::Waveform>(r.shape->currentData().toInt());
    w.min = r.min->text().trimmed();
    w.max = r.max->text().trimmed();
    w.period = r.period->text().trimmed();
    w.noise = r.noise->text().trimmed().isEmpty() ? QStringLiteral("0") : r.noise->text().trimmed();
    w.formula = r.formula->text().trimmed();
    out << w;
  }
  return out;
}

std::unique_ptr<GeneratorNode> GeneratorNodeDlg::node() const
{
  auto node = std::make_unique<GeneratorNode>();
  node->setPeriodMs(m_clock->value());
  node->setMaxAgeMs(m_maxAgeMs);
  node->setOn(m_on);
  node->setWaves(waves());
  return node;
}

void GeneratorNodeDlg::addRow(const GeneratorNode::Wave &wave)
{
  if (m_rows.size() >= kMaxValues)
    return;
  const int index = int(m_rows.size());
  const QString n = QString::number(index + 1);
  Row r;
  r.label = new QLabel(this);
  r.name = new QLineEdit(wave.name, this);
  r.name->setObjectName("ui_generatorName" + n);
  r.name->setPlaceholderText(tr("Name"));
  r.name->setMaximumWidth(120);
  r.unit = new QComboBox(this);
  r.unit->setObjectName("ui_generatorUnit" + n);
  r.unit->setEditable(true);
  for (const char *u : kUnits)
    r.unit->addItem(QString::fromUtf8(u));
  r.unit->setCurrentText(wave.unit);
  r.coupling = new QComboBox(this);
  r.coupling->setObjectName("ui_generatorCoupling" + n);
  r.coupling->addItem("DC", "DC");
  r.coupling->addItem("AC", "AC");
  r.coupling->addItem("–", QString());
  r.coupling->setCurrentIndex(qMax(0, r.coupling->findData(wave.coupling)));
  r.shape = new QComboBox(this);
  r.shape->setObjectName("ui_generatorShape" + n);
  const QString shapes[] = { tr("Constant"), tr("Random"), tr("Sine"), tr("Triangle"), tr("Square"),
                             tr("Sawtooth"), tr("Discharge"), tr("Formula") };
  for (int i = 0; i <= int(CalcExpr::Custom); ++i)
    r.shape->addItem(shapes[i], i);
  r.shape->setCurrentIndex(int(wave.shape));

  // the shape's numbers, or the formula
  r.how = new QStackedWidget(this);
  auto *numbers = new QWidget(r.how);
  auto *numbersLayout = new QHBoxLayout(numbers);
  numbersLayout->setContentsMargins(0, 0, 0, 0);
  auto field = [numbers, numbersLayout](const QString &text, const QString &label, const QString &tip, const QString &object)
  {
    numbersLayout->addWidget(new QLabel(label, numbers));
    auto *edit = new QLineEdit(text, numbers);
    edit->setObjectName(object);
    edit->setToolTip(tip);
    edit->setMaximumWidth(70);
    numbersLayout->addWidget(edit);
    return edit;
  };
  r.min = field(wave.min, tr("min"), tr("The lowest value"), "ui_generatorMin" + n);
  r.max = field(wave.max, tr("max"), tr("The highest value (the value of a constant)"), "ui_generatorMax" + n);
  r.period = field(wave.period, tr("period"), tr("The period in s; the time constant of a discharge"), "ui_generatorPeriod" + n);
  r.noise = field(wave.noise, tr("noise"), tr("Noise added, peak to peak; 0: none"), "ui_generatorNoise" + n);
  numbersLayout->addStretch(1);
  r.how->addWidget(numbers);
  r.formula = new QLineEdit(wave.formula, r.how);
  r.formula->setObjectName("ui_generatorFormula" + n);
  r.formula->setPlaceholderText(tr("e.g. 12 + 0.1 * sin(2 * pi * t / 5)"));
  r.how->addWidget(r.formula);
  r.how->setCurrentIndex(wave.shape == CalcExpr::Custom ? 1 : 0);
  r.result = new QLabel(this);
  r.result->setObjectName("ui_generatorResult" + n);
  r.result->setMinimumWidth(90);
  r.remove = new QToolButton(this);
  r.remove->setText(QStringLiteral("−"));
  r.remove->setToolTip(tr("Remove this value"));
  m_rows << r;

  QStackedWidget *how = r.how;
  connect(r.shape, &QComboBox::currentIndexChanged, this, [this, how](int i)
  {
    how->setCurrentIndex(i == int(CalcExpr::Custom) ? 1 : 0);
    validate();
  });
  for (QLineEdit *e : { r.min, r.max, r.period, r.noise, r.formula, r.name })
    connect(e, &QLineEdit::textChanged, this, &GeneratorNodeDlg::validate);
  connect(r.unit, &QComboBox::currentTextChanged, this, &GeneratorNodeDlg::updateResults);
  connect(r.remove, &QToolButton::clicked, this, [this, remove = r.remove]
  {
    for (int i = 0; i < m_rows.size(); ++i)
      if (m_rows[i].remove == remove)
      {
        removeRow(i);
        break;
      }
  });
  relayout();
}

void GeneratorNodeDlg::removeRow(int index)
{
  if (index < 0 || index >= m_rows.size() || m_rows.size() <= 1)
    return;
  const Row r = m_rows.takeAt(index);
  for (QWidget *w : std::initializer_list<QWidget *> { r.label, r.name, r.unit, r.coupling, r.shape, r.how, r.result, r.remove })
    w->deleteLater();
  // the values after it move up: y3 becomes y2 in the formulas
  for (int j = index; j < m_rows.size(); ++j)
  {
    QString text = m_rows[j].formula->text();
    for (int i = index + 1; i <= m_rows.size(); ++i)
      text = CalcExpr::renameVariable(text, FormulaNode::outputName(i), FormulaNode::outputName(i - 1));
    const QSignalBlocker block(m_rows[j].formula);
    m_rows[j].formula->setText(text);
  }
  relayout();
  validate();
}

void GeneratorNodeDlg::relayout()
{
  for (int i = 0; i < m_rows.size(); ++i)
  {
    const Row &r = m_rows[i];
    r.label->setText(FormulaNode::outputName(i));
    int c = 0;
    for (QWidget *w : std::initializer_list<QWidget *> { r.label, r.name, r.unit, r.coupling, r.shape, r.how, r.result, r.remove })
      m_grid->addWidget(w, i, c++);
    r.remove->setEnabled(m_rows.size() > 1);
  }
  m_add->setEnabled(m_rows.size() < kMaxValues);
}

void GeneratorNodeDlg::validate()
{
  QStringList problems;
  if (name().isEmpty())
    problems << tr("The name is missing.");
  const QList<GeneratorNode::Wave> ws = waves();
  for (int i = 0; i < ws.size(); ++i)
  {
    const QString text = GeneratorNode::formulaOf(ws[i]);
    int pos = -1;
    const QString error = text.isEmpty() ? tr("no formula") : FormulaNode::check(text, i, 0, &pos);
    QLineEdit *edit = ws[i].shape == CalcExpr::Custom ? m_rows[i].formula : nullptr;
    if (edit)
    {
      edit->setToolTip(error);
      edit->setStyleSheet(error.isEmpty() || edit->text().trimmed().isEmpty() ? QString() : QStringLiteral("color: #c0392b"));
    }
    if (!error.isEmpty())
      problems << (edit && pos >= 0 ? tr("%1: %2 (at %3)").arg(FormulaNode::outputName(i), error).arg(pos + 1)
                                    : tr("%1: %2").arg(FormulaNode::outputName(i), error));
  }
  m_status->setText(problems.join("<br>"));
  if (m_ok)
    m_ok->setEnabled(problems.isEmpty());
  // the results from a generator as it is set now, on the dialog's clock
  m_preview = node();
  LogicNode::Context start;
  m_preview->evaluate(start);   // t from 0 at m_since
  updateResults();
}

void GeneratorNodeDlg::updateResults()
{
  if (!m_preview)
    return;
  LogicNode::Context ctx;
  ctx.t = m_since.elapsed();
  const LogicNode::Values values = m_preview->evaluate(ctx);
  const QList<GeneratorNode::Wave> ws = m_preview->waves();
  for (int i = 0; i < m_rows.size() && i < ws.size(); ++i)
  {
    const std::optional<double> v = values.value(i);
    if (!v || !std::isfinite(*v))
    {
      m_rows[i].result->setText(QStringLiteral("= —"));
      continue;
    }
    const SiPrefix::Split unit = SiPrefix::split(LogicNode::canonicalUnit(m_rows[i].unit->currentText()));
    QString prefix;
    const double scaled = SiPrefix::scale(*v * SiPrefix::factor(unit.prefix), unit.baseUnit, &prefix);
    m_rows[i].result->setText(QString("= %1 %2").arg(QLocale().toString(scaled, 'g', 5),
                                                      SiPrefix::displayText(prefix + unit.baseUnit)).trimmed());
  }
}
