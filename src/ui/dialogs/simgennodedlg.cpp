// Copyright (c) 2026 The QtDMM developers
// SPDX-License-Identifier: GPL-3.0-or-later
#include "ui/dialogs/simgennodedlg.h"

#include <QCheckBox>
#include <QDoubleSpinBox>
#include <QGridLayout>
#include <QFormLayout>
#include <QGroupBox>
#include <QLabel>
#include <QLineEdit>
#include <QSpinBox>
#include <QVBoxLayout>

#include "core/functiongenerator.h"
#include "core/sampletypes.h"
#include "ui/genchannelseditor.h"

SimGenNodeEditor::SimGenNodeEditor(QWidget *parent) : NodeEditor(parent), m_node(std::make_unique<SimGenNode>())
{
  setObjectName("ui_simGenNodeEditor");
  auto *top = new QVBoxLayout(this);
  top->setContentsMargins(0, 0, 0, 0);
  auto *form = new QFormLayout;
  top->addLayout(form);
  m_name = new QLineEdit(this);
  m_name->setObjectName("ui_simGenName");
  m_name->setPlaceholderText(tr("e.g. Signal generator"));
  form->addRow(tr("&Name:"), m_name);
  m_channels = new QSpinBox(this);
  m_channels->setObjectName("ui_simGenChannels");
  m_channels->setRange(1, SimGenNode::kMaxChannels);
  m_channels->setToolTip(tr("Outputs of the generator; the one in the OWON SDS215S has one"));
  form->addRow(tr("C&hannels:"), m_channels);

  auto *channels = new QGroupBox(tr("Channels"), this);
  auto *channelsLayout = new QVBoxLayout(channels);
  m_channelRows = new GenChannelsEditor(channels);
  channelsLayout->addWidget(m_channelRows);
  top->addWidget(channels);

  // which values each channel gives: a meter's readings at its output
  auto *given = new QGroupBox(tr("Values"), this);
  auto *givenLayout = new QVBoxLayout(given);
  auto *givenHint = new QLabel(tr("What a meter at the output reads, at most %1 in all: RMS of the AC part, peak to peak, "
                                  "frequency, mean (the DC part), and the voltage itself up to 0.5 Hz (a curve in the graph).")
                                 .arg(kMaxValues),
                               given);
  givenHint->setWordWrap(true);
  givenLayout->addWidget(givenHint);
  m_givenGrid = new QGridLayout;
  givenLayout->addLayout(m_givenGrid);
  top->addWidget(given);

  // a worse generator than an ideal one
  auto *faults = new QGroupBox(tr("A worse generator"), this);
  auto *faultsForm = new QFormLayout(faults);
  auto *hint = new QLabel(tr("All 0: an ideal generator."), faults);
  hint->setWordWrap(true);
  faultsForm->addRow(hint);
  auto number = [faults](double min, double max, int decimals, const QString &suffix, const QString &tip)
  {
    auto *box = new QDoubleSpinBox(faults);
    box->setRange(min, max);
    box->setDecimals(decimals);
    box->setSuffix(suffix);
    box->setToolTip(tip);
    return box;
  };
  m_ppm = number(-100000, 100000, 1, QStringLiteral(" ppm"), tr("The frequency off by this, either way"));
  m_amplitude = number(-50, 50, 2, QStringLiteral(" %"), tr("The amplitude off by this, either way"));
  m_cutoff = number(0, 100e6, 0, QStringLiteral(" Hz"),
                    tr("Above it the amplitude falls (first order, -3 dB here); 0: flat up to the top"));
  m_noise = number(0, 1000, 1, QStringLiteral(" mV"), tr("Random on the voltages, at most this much either way"));
  faultsForm->addRow(tr("&Frequency error:"), m_ppm);
  faultsForm->addRow(tr("&Amplitude error:"), m_amplitude);
  faultsForm->addRow(tr("C&utoff:"), m_cutoff);
  faultsForm->addRow(tr("N&oise:"), m_noise);
  top->addWidget(faults);
  top->addStretch(1);

  connect(m_name, &QLineEdit::textChanged, this, [this] { setValid(!name().isEmpty()); });
  connect(m_channels, &QSpinBox::valueChanged, this, &SimGenNodeEditor::setChannels);
  load(tr("Function generator"), SimGenNode());
}

SimGenNodeEditor::~SimGenNodeEditor() = default;

void SimGenNodeEditor::load(const QString &name, const SimGenNode &node)
{
  m_node = std::make_unique<SimGenNode>();
  m_node->fromJson(node.toJson());
  m_name->setText(name);
  const SimGenNode::Faults f = node.faults();
  m_ppm->setValue(f.frequencyPpm);
  m_amplitude->setValue(f.amplitudePct);
  m_cutoff->setValue(f.cutoffHz);
  m_noise->setValue(f.noiseMv);
  {
    const QSignalBlocker block(m_channels);
    m_channels->setValue(m_node->channels());
  }
  m_channelRows->load(m_node->functionGenerator());
  loadGiven();
  setValid(!this->name().isEmpty());
}

void SimGenNodeEditor::loadGiven()
{
  for (const auto &row : std::as_const(m_given))
    for (QCheckBox *box : row)
      delete box;
  m_given.clear();
  const QStringList labels = { tr("RMS"), tr("Peak to peak"), tr("Frequency"), tr("Mean"), tr("Voltage (slow)") };
  for (int ch = 0; ch < m_node->channels(); ++ch)
  {
    const SimGenNode::Given g = m_node->given(ch);
    std::array<QCheckBox *, SimGenNode::kQuantities> row {};
    for (int q = 0; q < SimGenNode::kQuantities; ++q)
    {
      // the channel's name only on its first box, as a row label
      row[q] = new QCheckBox(q == 0 ? QStringLiteral("CH%1: %2").arg(ch + 1).arg(labels[q]) : labels[q], this);
      row[q]->setChecked(g[q]);
      m_givenGrid->addWidget(row[q], ch, q);
      connect(row[q], &QCheckBox::toggled, this, &SimGenNodeEditor::limitGiven);
    }
    m_given << row;
  }
  limitGiven();
}

void SimGenNodeEditor::limitGiven()
{
  int ticked = 0;
  for (const auto &row : std::as_const(m_given))
    for (QCheckBox *box : row)
      ticked += box->isChecked();
  for (const auto &row : std::as_const(m_given))
    for (QCheckBox *box : row)
      box->setEnabled(box->isChecked() || ticked < kMaxValues);
}

void SimGenNodeEditor::setChannels(int count)
{
  // the rows as edited into the node, then one more or fewer
  m_channelRows->apply(m_node->functionGenerator());
  m_node->setChannels(count);
  m_channelRows->load(m_node->functionGenerator());
  loadGiven();   // the defaults for the new count
}

QString SimGenNodeEditor::name() const
{
  return m_name->text().trimmed();
}

SimGenNode::Faults SimGenNodeEditor::faults() const
{
  SimGenNode::Faults f;
  f.frequencyPpm = m_ppm->value();
  f.amplitudePct = m_amplitude->value();
  f.cutoffHz = m_cutoff->value();
  f.noiseMv = m_noise->value();
  return f;
}

std::unique_ptr<SimGenNode> SimGenNodeEditor::node() const
{
  auto node = std::make_unique<SimGenNode>();
  node->fromJson(m_node->toJson());
  node->setFaults(faults());
  m_channelRows->apply(node->functionGenerator());
  for (int ch = 0; ch < int(m_given.size()) && ch < node->channels(); ++ch)
  {
    SimGenNode::Given g {};
    for (int q = 0; q < SimGenNode::kQuantities; ++q)
      g[q] = m_given[ch][q]->isChecked();
    node->setGiven(ch, g);
  }
  return node;
}

std::unique_ptr<LogicNode> SimGenNodeEditor::logicNode() const
{
  return node();
}

SimGenNodeDlg::SimGenNodeDlg(QWidget *parent) : NodeDlg(new SimGenNodeEditor, tr("Function generator"), parent)
{
  setObjectName("ui_simGenNodeDlg");
  resize(820, 640);
}
