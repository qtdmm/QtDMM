// Copyright (c) 2026 The QtDMM developers
// SPDX-License-Identifier: GPL-3.0-or-later
#include "ui/dialogs/integralnodedlg.h"

#include <cmath>

#include <QCheckBox>
#include <QComboBox>
#include <QDialogButtonBox>
#include <QDoubleSpinBox>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QLocale>
#include <QPushButton>
#include <QVBoxLayout>

#include "core/calcexpr.h"
#include "core/siprefix.h"

namespace
{
// a number as typed, also with an SI suffix ("1.5k"); none for anything else
std::optional<double> number(const QString &text)
{
  const std::optional<CalcExpr> e = CalcExpr::parse(text);
  return e && e->variables().isEmpty() ? e->eval({}) : std::nullopt;
}
}

IntegralNodeDlg::IntegralNodeDlg(QWidget *parent) :
  QDialog(parent)
{
  setObjectName("ui_integralNodeDlg");
  setWindowTitle(tr("Integral"));
  auto *top = new QVBoxLayout(this);
  auto *form = new QFormLayout;
  top->addLayout(form);
  m_name = new QLineEdit(this);
  m_name->setObjectName("ui_integralName");
  form->addRow(tr("&Name:"), m_name);
  m_input = new QComboBox(this);
  m_input->setObjectName("ui_integralInput");
  m_input->setToolTip(tr("The value summed up over time; each new reading of it adds the area since the one before"));
  form->addRow(tr("&Value:"), m_input);
  m_unit = new QComboBox(this);
  m_unit->setObjectName("ui_integralUnit");
  m_unit->setEditable(true);
  m_unit->setToolTip(tr("The unit of the sum: the value's unit times hours (W: Wh), with a prefix if wanted (kWh)"));
  form->addRow(tr("&Unit:"), m_unit);
  m_maxAge = new QDoubleSpinBox(this);
  m_maxAge->setObjectName("ui_integralMaxAge");
  m_maxAge->setRange(0.1, 3600);
  m_maxAge->setDecimals(1);
  m_maxAge->setSuffix(" s");
  m_maxAge->setToolTip(tr("Two readings further apart than this are a gap: the sum pauses over it"));
  form->addRow(tr("&Gap after:"), m_maxAge);

  m_onRecording = new QCheckBox(tr("Start at 0 when a &recording starts"), this);
  m_onRecording->setObjectName("ui_integralOnRecording");
  top->addWidget(m_onRecording);
  auto *thresholdRow = new QHBoxLayout;
  m_thresholdOn = new QCheckBox(tr("Start at 0 when the value rises &above:"), this);
  m_thresholdOn->setObjectName("ui_integralThresholdOn");
  m_threshold = new QLineEdit(this);
  m_threshold->setObjectName("ui_integralThreshold");
  m_threshold->setPlaceholderText(tr("e.g. 0.5"));
  m_threshold->setToolTip(tr("In the value's base unit (V, A, W, not mA): 0.5 for 500 mA; e.g. a load switched on"));
  m_threshold->setMaximumWidth(120);
  m_thresholdUnit = new QLabel(this);
  m_thresholdUnit->setObjectName("ui_integralThresholdUnit");
  thresholdRow->addWidget(m_thresholdOn);
  thresholdRow->addWidget(m_threshold);
  thresholdRow->addWidget(m_thresholdUnit);
  thresholdRow->addStretch(1);
  top->addLayout(thresholdRow);

  auto *nowRow = new QHBoxLayout;
  m_now = new QLabel(this);
  m_now->setObjectName("ui_integralNow");
  m_zero = new QPushButton(tr("Set to &0"), this);
  m_zero->setObjectName("ui_integralZero");
  nowRow->addWidget(m_now);
  nowRow->addWidget(m_zero);
  nowRow->addStretch(1);
  top->addLayout(nowRow);
  m_now->hide();
  m_zero->hide();

  m_status = new QLabel(this);
  m_status->setObjectName("ui_integralStatus");
  m_status->setWordWrap(true);
  top->addWidget(m_status);
  top->addStretch(1);
  auto *buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
  top->addWidget(buttons);
  m_ok = buttons->button(QDialogButtonBox::Ok);
  connect(buttons, &QDialogButtonBox::accepted, this, &QDialog::accept);
  connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);

  connect(m_input, &QComboBox::currentIndexChanged, this, [this] { propose(); validate(); });
  connect(m_unit, &QComboBox::currentTextChanged, this, [this] { if (!m_proposing) m_unitChosen = true; });
  connect(m_name, &QLineEdit::textEdited, this, [this] { m_nameChosen = true; });
  connect(m_name, &QLineEdit::textChanged, this, &IntegralNodeDlg::validate);
  connect(m_thresholdOn, &QCheckBox::toggled, m_threshold, &QWidget::setEnabled);
  connect(m_thresholdOn, &QCheckBox::toggled, this, &IntegralNodeDlg::validate);
  connect(m_threshold, &QLineEdit::textChanged, this, &IntegralNodeDlg::validate);
  connect(m_zero, &QPushButton::clicked, this, [this]
  {
    if (m_reset)
      m_reset();
    updateState();
  });
  m_live.setInterval(1000);
  connect(&m_live, &QTimer::timeout, this, &IntegralNodeDlg::updateState);
  resize(480, 300);
}

void IntegralNodeDlg::setChoices(const QList<Choice> &choices)
{
  m_choices = choices;
  const QSignalBlocker block(m_input);
  m_input->clear();
  for (const Choice &c : choices)
    m_input->addItem(c.text, c.ref.toString());
}

void IntegralNodeDlg::setState(std::function<std::optional<Reading>()> current, std::function<void()> reset)
{
  m_current = std::move(current);
  m_reset = std::move(reset);
  m_now->setVisible(bool(m_current));
  m_zero->setVisible(bool(m_reset));
  updateState();
  if (m_current)
    m_live.start();
}

void IntegralNodeDlg::load(const QString &name, const IntegralNode &node)
{
  m_on = node.isOn();
  m_maxAge->setValue(node.maxAgeMs() / 1000.0);
  m_onRecording->setChecked(node.resetOnRecording());
  m_thresholdOn->setChecked(node.threshold().has_value());
  m_threshold->setEnabled(node.threshold().has_value());
  if (node.threshold())
    m_threshold->setText(QString::number(*node.threshold()));
  const bool known = !node.inputs().isEmpty();
  if (known)
  {
    const ValueRef ref = node.inputs().first();
    int at = m_input->findData(ref.toString());
    if (at < 0)
    {
      // gone from My devices: kept, and said so
      m_input->addItem(tr("%1 (not in My devices)").arg(ref.toString()), ref.toString());
      at = m_input->count() - 1;
    }
    const QSignalBlocker block(m_input);
    m_input->setCurrentIndex(at);
  }
  // a stored one (it has its name) keeps its name and unit; a new one, also
  // with its value given ("Integrate..."), is proposed them
  const bool stored = !node.name().isEmpty();
  m_unitChosen = stored;
  m_nameChosen = stored;
  m_proposing = true;
  m_unit->setCurrentText(node.unit());
  m_proposing = false;
  m_name->setText(name);
  propose();
  if (!stored)
    m_name->selectAll();
  validate();
}

QString IntegralNodeDlg::name() const
{
  return m_name->text().simplified();
}

std::unique_ptr<IntegralNode> IntegralNodeDlg::node() const
{
  auto node = std::make_unique<IntegralNode>();
  node->setInputs({ ValueRef::fromString(m_input->currentData().toString()) });
  node->setMaxAgeMs(int(std::lround(m_maxAge->value() * 1000)));
  node->setOn(m_on);
  node->setName(name());
  node->setUnit(LogicNode::canonicalUnit(m_unit->currentText()));
  node->setResetOnRecording(m_onRecording->isChecked());
  node->setThreshold(m_thresholdOn->isChecked() ? number(m_threshold->text()) : std::nullopt);
  return node;
}

void IntegralNodeDlg::propose()
{
  const int i = m_input->currentIndex();
  const QString base = i >= 0 && i < m_choices.size() ? LogicNode::canonicalUnit(m_choices[i].unit) : QString();
  const QString unit = IntegralNode::integratedUnit(base);
  // the threshold is in the value's base unit: said beside it
  m_thresholdUnit->setText(SiPrefix::displayText(base));
  m_proposing = true;
  // a value whose unit is not known yet (its device not connected): the
  // unit as it is
  if (!m_unitChosen && !base.isEmpty())
  {
    // the sum's unit with and without the prefixes one wants
    m_unit->clear();
    m_unit->addItems({ unit, "k" + unit, "m" + unit });
    m_unit->setCurrentText(unit);
  }
  m_proposing = false;
  if (!m_nameChosen)
    m_name->setText(base == QLatin1String("W") ? tr("Energy") : base == QLatin1String("A") ? tr("Charge") : tr("Integral"));
}

void IntegralNodeDlg::validate()
{
  QStringList problems;
  if (name().isEmpty())
    problems << tr("The name is missing.");
  if (m_input->currentIndex() < 0)
    problems << tr("No value to integrate: add a device to My devices first.");
  if (m_thresholdOn->isChecked() && !number(m_threshold->text()))
    problems << tr("The threshold is no number.");
  m_status->setText(problems.join("<br>"));
  if (m_ok)
    m_ok->setEnabled(problems.isEmpty());
}

void IntegralNodeDlg::updateState()
{
  if (!m_current)
    return;
  const std::optional<Reading> r = m_current();
  if (!r || r->noNumber())
  {
    m_now->setText(tr("Now: —"));
    return;
  }
  QString prefix;
  const double scaled = SiPrefix::scale(r->value, r->baseUnit, &prefix);
  m_now->setText(tr("Now: %1 %2").arg(QLocale().toString(scaled, 'g', 6), SiPrefix::displayText(prefix + r->baseUnit)));
}
