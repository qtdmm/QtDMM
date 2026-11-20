// Copyright (c) 2026 The QtDMM developers
// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <QDialog>
#include <memory>

class LogicNode;
class QPushButton;

/// The settings of one kind of logic node as a widget (FormulaNodeEditor,
/// GeneratorNodeEditor, IntegralNodeEditor): a page of the assistant "Add
/// device" or the content of its dialog (NodeDlg). It says whether what is
/// set is complete (valid()); the node it makes is the one set.
class NodeEditor : public QWidget
{
  Q_OBJECT
public:
  using QWidget::QWidget;

  /// The name for My devices.
  virtual QString name() const = 0;
  /// The node as set.
  virtual std::unique_ptr<LogicNode> logicNode() const = 0;
  /// Its entry's model in My devices ("QtDMM Formula").
  virtual QString model() const = 0;
  /// Everything set is right: OK, Next.
  bool        valid() const { return m_valid; }

Q_SIGNALS:
  void        validChanged(bool valid);

protected:
  void        setValid(bool valid);

private:
  bool        m_valid = false;
};

/// A dialog around a NodeEditor: OK while it is valid.
class NodeDlg : public QDialog
{
  Q_OBJECT
public:
  NodeDlg(NodeEditor *editor, const QString &title, QWidget *parent = nullptr);
  NodeEditor *editor() const { return m_editor; }

private:
  NodeEditor  *m_editor;
  QPushButton *m_ok;
};
