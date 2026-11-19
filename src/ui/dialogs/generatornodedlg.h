// Copyright (c) 2026 The QtDMM developers
// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <QDialog>
#include <QElapsedTimer>
#include <QList>
#include <QTimer>
#include <memory>

#include "logic/generatornode.h"

class QComboBox;
class QGridLayout;
class QLabel;
class QLineEdit;
class QPushButton;
class QSpinBox;
class QStackedWidget;
class QToolButton;

/// The settings of a generator (GeneratorNode), "Simulated" in the
/// assistant: its name, its clock and up to kMaxValues values, each with
/// name, unit, AC/DC and a shape - min, max, period and noise - or a
/// formula over t and the values above it (y1 ...). Each value shows
/// where it is now; a formula that does not parse keeps OK off.
class GeneratorNodeDlg : public QDialog
{
  Q_OBJECT
public:
  explicit GeneratorNodeDlg(QWidget *parent = nullptr);

  /// Fills in @p name and @p node; one without values gets a sine.
  void        load(const QString &name, const GeneratorNode &node);
  QString     name() const;
  /// The generator as set, its on/off as loaded.
  std::unique_ptr<GeneratorNode> node() const;

private:
  struct Row
  {
    QLabel         *label;
    QLineEdit      *name;
    QComboBox      *unit;
    QComboBox      *coupling;
    QComboBox      *shape;
    QStackedWidget *how;       ///< the shape's numbers, or the formula
    QLineEdit      *min;
    QLineEdit      *max;
    QLineEdit      *period;
    QLineEdit      *noise;
    QLineEdit      *formula;
    QLabel         *result;
    QToolButton    *remove;
  };

  void        addRow(const GeneratorNode::Wave &wave);
  void        removeRow(int index);
  void        relayout();
  QList<GeneratorNode::Wave> waves() const;
  void        validate();
  void        updateResults();

  QLineEdit   *m_name;
  QSpinBox    *m_clock;
  QGridLayout *m_grid;
  QPushButton *m_add;
  QLabel      *m_status;
  QPushButton *m_ok = nullptr;
  QList<Row>   m_rows;
  bool         m_on = true;
  int          m_maxAgeMs = 3000;
  QTimer       m_live;
  QElapsedTimer m_since;   ///< t of the results shown
  std::unique_ptr<GeneratorNode> m_preview;   ///< computes the results shown
};
