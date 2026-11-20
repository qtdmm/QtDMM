// Copyright (c) 2026 The QtDMM developers
// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <QTimer>
#include <functional>
#include <memory>
#include <optional>

#include "core/reading.h"
#include "core/valueref.h"
#include "logic/integralnode.h"
#include "ui/dialogs/nodeeditor.h"
#include "ui/sourcelist.h"

class QCheckBox;
class QComboBox;
class QDoubleSpinBox;
class QLabel;
class QLineEdit;
class QPushButton;

/// The settings of an integral (IntegralNode): its name, the value it
/// integrates - any value of My devices -, its unit (derived from the
/// value's: W Wh, A Ah; a prefix may be chosen), the maximum age of a
/// reading, and when it starts at 0 again: at the start of a recording,
/// when the value rises above a threshold. An integral open shows where it
/// is now, with a button to set it to 0.
class IntegralNodeEditor : public NodeEditor
{
  Q_OBJECT
public:
  explicit IntegralNodeEditor(QWidget *parent = nullptr);

  /// The entries of My devices whose values may be integrated.
  void        setSources(const QList<SourceList::Source> &sources);
  /// @p makesLoop says whether taking these inputs closes a loop through
  /// this one: such devices are greyed out, and choosing one is an error.
  void        setLoopCheck(std::function<bool(const QList<ValueRef> &inputs)> makesLoop);
  /// The integral's own reading now (an integral open), and what sets it
  /// to 0; none: no "where it is now".
  void        setState(std::function<std::optional<Reading>()> current, std::function<void()> reset);

  /// Fills in @p name and @p node; a new one: @p node without input takes
  /// the first choice.
  void        load(const QString &name, const IntegralNode &node);
  QString     name() const override;
  std::unique_ptr<IntegralNode> node() const;
  std::unique_ptr<LogicNode> logicNode() const override;
  QString     model() const override { return QStringLiteral("QtDMM Integral"); }

private:
  /// The unit and name proposed from the value chosen, unless chosen.
  void        propose();
  void        validate();
  void        updateState();

  QLineEdit      *m_name;
  SourceList     *m_input;
  QComboBox      *m_unit;
  QDoubleSpinBox *m_maxAge;
  QCheckBox      *m_onRecording;
  QCheckBox      *m_thresholdOn;
  QLineEdit      *m_threshold;
  QLabel         *m_thresholdUnit;   ///< the value's base unit: the threshold is in it
  QLabel         *m_now;
  QPushButton    *m_zero;
  QLabel         *m_status;
  bool            m_unitChosen = false;
  bool            m_nameChosen = false;
  bool            m_proposing = false;
  bool            m_on = true;
  std::function<bool(const QList<ValueRef> &)> m_makesLoop;
  QTimer          m_live;
  std::function<std::optional<Reading>()> m_current;
  std::function<void()> m_reset;
};

/// IntegralNodeEditor in a dialog of its own.
class IntegralNodeDlg : public NodeDlg
{
  Q_OBJECT
public:
  explicit IntegralNodeDlg(QWidget *parent = nullptr);
  IntegralNodeEditor *integralEditor() const { return static_cast<IntegralNodeEditor *>(editor()); }
  void        setSources(const QList<SourceList::Source> &sources) { integralEditor()->setSources(sources); }
  void        setState(std::function<std::optional<Reading>()> current, std::function<void()> reset)
  { integralEditor()->setState(std::move(current), std::move(reset)); }
  void        load(const QString &name, const IntegralNode &node) { integralEditor()->load(name, node); }
  QString     name() const { return integralEditor()->name(); }
  std::unique_ptr<IntegralNode> node() const { return integralEditor()->node(); }
};
