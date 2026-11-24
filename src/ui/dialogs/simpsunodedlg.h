// Copyright (c) 2026 The QtDMM developers
// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <memory>

#include "logic/simpsunode.h"
#include "ui/dialogs/nodeeditor.h"

class PsuChannelsEditor;
class QDoubleSpinBox;
class QLineEdit;
class QSpinBox;

/// The settings of a simulated power supply (SimPsuNode), "Simulated →
/// Power supply" in the assistant and its gear: its name, how many
/// channels (1 to 4, dual: 2), the channels' settings (PsuChannelsEditor:
/// shown, colour, limits, knob steps) and what makes it a worse supply than
/// an ideal one - rise and fall time, overshoot, ripple, noise, drift; all
/// 0 at first. The setpoints stay as they were.
class SimPsuNodeEditor : public NodeEditor
{
  Q_OBJECT
public:
  explicit SimPsuNodeEditor(QWidget *parent = nullptr);
  ~SimPsuNodeEditor() override;

  void        load(const QString &name, const SimPsuNode &node);
  QString     name() const override;
  /// The supply as set, its setpoints and on/off as loaded.
  std::unique_ptr<SimPsuNode> node() const;
  std::unique_ptr<LogicNode> logicNode() const override;
  QString     model() const override { return QStringLiteral("QtDMM Power supply"); }

private:
  /// The channel count changed: the channels' rows follow, those kept as edited.
  void        setChannels(int count);
  SimPsuNode::Faults faults() const;

  QLineEdit   *m_name;
  QSpinBox    *m_channels;
  PsuChannelsEditor *m_channelRows;
  QDoubleSpinBox *m_rise, *m_fall, *m_overshoot, *m_ripple, *m_noise, *m_drift;
  std::unique_ptr<SimPsuNode> m_node;   ///< as loaded, with the channels as set
};

/// SimPsuNodeEditor in a dialog of its own.
class SimPsuNodeDlg : public NodeDlg
{
  Q_OBJECT
public:
  explicit SimPsuNodeDlg(QWidget *parent = nullptr);
  SimPsuNodeEditor *supplyEditor() const { return static_cast<SimPsuNodeEditor *>(editor()); }
  void        load(const QString &name, const SimPsuNode &node) { supplyEditor()->load(name, node); }
};
