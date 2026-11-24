// Copyright (c) 2026 The QtDMM developers
// SPDX-License-Identifier: GPL-3.0-or-later
#include "ui/dialogs/simpsunodedlg.h"

#include <QDoubleSpinBox>
#include <QFormLayout>
#include <QGroupBox>
#include <QLabel>
#include <QLineEdit>
#include <QSpinBox>
#include <QVBoxLayout>

#include "core/powersupply.h"
#include "ui/psuchannelseditor.h"

SimPsuNodeEditor::SimPsuNodeEditor(QWidget *parent) : NodeEditor(parent), m_node(std::make_unique<SimPsuNode>())
{
  setObjectName("ui_simPsuNodeEditor");
  auto *top = new QVBoxLayout(this);
  top->setContentsMargins(0, 0, 0, 0);
  auto *form = new QFormLayout;
  top->addLayout(form);
  m_name = new QLineEdit(this);
  m_name->setObjectName("ui_simPsuName");
  m_name->setPlaceholderText(tr("e.g. Bench supply"));
  form->addRow(tr("&Name:"), m_name);
  m_channels = new QSpinBox(this);
  m_channels->setObjectName("ui_simPsuChannels");
  m_channels->setRange(1, SimPsuNode::kMaxChannels);
  m_channels->setToolTip(tr("Outputs of the supply, each 0 to %1 V; a dual supply has two").arg(SimPsuNode::kMaxVoltage));
  form->addRow(tr("C&hannels:"), m_channels);

  auto *channels = new QGroupBox(tr("Channels"), this);
  auto *channelsLayout = new QVBoxLayout(channels);
  m_channelRows = new PsuChannelsEditor(channels);
  channelsLayout->addWidget(m_channelRows);
  top->addWidget(channels);

  // a worse supply than an ideal one
  auto *faults = new QGroupBox(tr("A worse supply"), this);
  auto *faultsForm = new QFormLayout(faults);
  auto *hint = new QLabel(tr("All 0: an ideal supply. A new value comes every 250 ms, too seldom for a 100 Hz hum: "
                             "the ripple shows as a band of random values, not as a sine."),
                          faults);
  hint->setWordWrap(true);
  faultsForm->addRow(hint);
  auto number = [faults](double max, int decimals, const QString &suffix, const QString &tip)
  {
    auto *box = new QDoubleSpinBox(faults);
    box->setRange(0, max);
    box->setDecimals(decimals);
    box->setSuffix(suffix);
    box->setToolTip(tip);
    return box;
  };
  m_rise = number(60000, 0, QStringLiteral(" ms"), tr("From 0 V to the setpoint when switched on, and per step up"));
  m_fall = number(60000, 0, QStringLiteral(" ms"), tr("To 0 V when switched off, and per step down"));
  m_overshoot = number(100, 1, QStringLiteral(" %"), tr("Above the setpoint after a rise, in percent of the step; "
                                                        "it settles within about one rise time"));
  m_ripple = number(10000, 1, QStringLiteral(" mVpp"), tr("A band of this width around the voltage while the output is on"));
  m_noise = number(10000, 1, QStringLiteral(" mV"), tr("Random, at most this much either way"));
  m_drift = number(10000, 2, QStringLiteral(" mV/min"), tr("Away from the setpoint since the output went on"));
  m_drift->setMinimum(-10000);
  faultsForm->addRow(tr("&Rise time:"), m_rise);
  faultsForm->addRow(tr("&Fall time:"), m_fall);
  faultsForm->addRow(tr("O&vershoot:"), m_overshoot);
  faultsForm->addRow(tr("Ri&pple:"), m_ripple);
  faultsForm->addRow(tr("N&oise:"), m_noise);
  faultsForm->addRow(tr("&Drift:"), m_drift);
  top->addWidget(faults);
  top->addStretch(1);

  connect(m_name, &QLineEdit::textChanged, this, [this] { setValid(!name().isEmpty()); });
  connect(m_channels, &QSpinBox::valueChanged, this, &SimPsuNodeEditor::setChannels);
  load(tr("Power supply"), SimPsuNode());
}

SimPsuNodeEditor::~SimPsuNodeEditor() = default;

void SimPsuNodeEditor::load(const QString &name, const SimPsuNode &node)
{
  m_node = std::make_unique<SimPsuNode>();
  m_node->fromJson(node.toJson());
  m_name->setText(name);
  const SimPsuNode::Faults f = node.faults();
  m_rise->setValue(f.riseMs);
  m_fall->setValue(f.fallMs);
  m_overshoot->setValue(f.overshoot);
  m_ripple->setValue(f.rippleMvpp);
  m_noise->setValue(f.noiseMv);
  m_drift->setValue(f.driftMvPerMin);
  {
    const QSignalBlocker block(m_channels);
    m_channels->setValue(m_node->channels());
  }
  m_channelRows->load(m_node->powerSupply());
  setValid(!this->name().isEmpty());
}

void SimPsuNodeEditor::setChannels(int count)
{
  // the rows as edited into the node, then one more or fewer
  m_channelRows->apply(m_node->powerSupply());
  m_node->setChannels(count);
  m_channelRows->load(m_node->powerSupply());
}

QString SimPsuNodeEditor::name() const
{
  return m_name->text().trimmed();
}

SimPsuNode::Faults SimPsuNodeEditor::faults() const
{
  SimPsuNode::Faults f;
  f.riseMs = m_rise->value();
  f.fallMs = m_fall->value();
  f.overshoot = m_overshoot->value();
  f.rippleMvpp = m_ripple->value();
  f.noiseMv = m_noise->value();
  f.driftMvPerMin = m_drift->value();
  return f;
}

std::unique_ptr<SimPsuNode> SimPsuNodeEditor::node() const
{
  auto node = std::make_unique<SimPsuNode>();
  node->fromJson(m_node->toJson());
  node->setFaults(faults());
  m_channelRows->apply(node->powerSupply());
  return node;
}

std::unique_ptr<LogicNode> SimPsuNodeEditor::logicNode() const
{
  return node();
}

SimPsuNodeDlg::SimPsuNodeDlg(QWidget *parent) : NodeDlg(new SimPsuNodeEditor, tr("Power supply"), parent)
{
  setObjectName("ui_simPsuNodeDlg");
  resize(720, 560);
}
