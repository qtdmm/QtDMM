// Copyright (c) 2026 The QtDMM developers
// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <memory>

#include "logic/simgennode.h"
#include "ui/dialogs/nodeeditor.h"

class GenChannelsEditor;
class QCheckBox;
class QDoubleSpinBox;
class QGridLayout;
class QLineEdit;
class QSpinBox;

/// The settings of a simulated function generator (SimGenNode), "Simulated
/// → Function generator" in the assistant and its gear: its name, how many
/// channels (1 or 2), the channels' settings (GenChannelsEditor: shown,
/// colour, highest amplitude and offset, knob steps), which values each
/// channel gives (at most four in all) and what makes it a
/// worse generator than an ideal one - frequency and amplitude error, a
/// cutoff, noise; all 0 at first. What the channels are set to stays.
class SimGenNodeEditor : public NodeEditor
{
  Q_OBJECT
public:
  explicit SimGenNodeEditor(QWidget *parent = nullptr);
  ~SimGenNodeEditor() override;

  void        load(const QString &name, const SimGenNode &node);
  QString     name() const override;
  /// The generator as set, its channels' settings and outputs as loaded.
  std::unique_ptr<SimGenNode> node() const;
  std::unique_ptr<LogicNode> logicNode() const override;
  QString     model() const override { return QStringLiteral("QtDMM Function generator"); }

private:
  void        setChannels(int count);
  SimGenNode::Faults faults() const;
  /// The rows of what each channel gives anew, from m_node.
  void        loadGiven();
  /// At most kMaxValues ticked: the others locked while that many are.
  void        limitGiven();

  QLineEdit   *m_name;
  QSpinBox    *m_channels;
  GenChannelsEditor *m_channelRows;
  QGridLayout *m_givenGrid;
  QList<std::array<QCheckBox *, SimGenNode::kQuantities>> m_given;
  QDoubleSpinBox *m_ppm, *m_amplitude, *m_cutoff, *m_noise;
  std::unique_ptr<SimGenNode> m_node;   ///< as loaded, with the channels as set
};

/// SimGenNodeEditor in a dialog of its own.
class SimGenNodeDlg : public NodeDlg
{
  Q_OBJECT
public:
  explicit SimGenNodeDlg(QWidget *parent = nullptr);
  SimGenNodeEditor *generatorEditor() const { return static_cast<SimGenNodeEditor *>(editor()); }
  void        load(const QString &name, const SimGenNode &node) { generatorEditor()->load(name, node); }
};
