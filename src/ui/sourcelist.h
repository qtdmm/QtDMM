// Copyright (c) 2026 The QtDMM developers
// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <QStringList>
#include <QWidget>
#include <functional>

#include "core/valueref.h"

class QListWidget;
class QPushButton;
class QTreeWidget;
class QTreeWidgetItem;

/// The sources a logic node takes, as a list to tick (prototype round 3,
/// scenario 6): every entry of My devices - devices and logic nodes alike,
/// each with its picture -, opened to its values. One not connected is
/// grey, "(not connected)", and may be chosen; one that would close a loop
/// is greyed out, why in its tooltip. Shared by the node dialogs (and
/// later the assistant's page).
///
/// - Devices (a formula): a tick on a device makes it an input; its values
///   below are shown for information with their names in the formula
///   ("v1 Voltage", "v1.2 Current"). The inputs, in their order v1, v2 ...,
///   are listed beside the tree: dragged or moved with the arrows into
///   another order, one of them marked as leading.
/// - Value (an integral): one value of a device is chosen.
class SourceList : public QWidget
{
  Q_OBJECT
public:
  enum Mode { Devices, Value };

  /// An entry of My devices with its values.
  struct Source
  {
    QString     source;   ///< "device:<id>"
    QString     name;
    QStringList values;   ///< what each value is ("Voltage DC (V)")
    QStringList units;    ///< their base units when known
    bool        connected = true;
    bool        node = false;   ///< a logic node (QtDMM's picture)
  };

  explicit SourceList(Mode mode, QWidget *parent = nullptr);

  /// The entries offered, in their order.
  void        setSources(const QList<Source> &sources);
  QList<Source> sources() const { return m_sources; }
  /// Why @p source may not be taken (a loop); empty: it may.
  void        setBlocked(std::function<QString(const QString &source)> blocked);

  /// @name Devices
  /// @{
  QStringList inputs() const { return m_inputs; }
  int         leading() const { return m_leading; }
  void        setInputs(const QStringList &sources, int leading);
  /// Moves input @p from to place @p to (what the arrows and a drop do).
  void        moveInput(int from, int to);
  /// Ticks or unticks @p source.
  void        setTicked(const QString &source, bool on);
  /// @}

  /// @name Value
  /// @{
  ValueRef    value() const { return m_value; }
  void        setValue(const ValueRef &value);
  /// @}

  /// The base unit of @p ref's value, when known.
  QString     unitOf(const ValueRef &ref) const;

Q_SIGNALS:
  /// The inputs changed: @p from gives for each input now the place it had
  /// before, -1 for a new one (the formulas follow: v3 is v2 now);
  /// @p former the number of inputs before.
  void        inputsChanged(const QList<int> &from, int former);
  void        valueChanged(const ValueRef &value);

private:
  void        fill();
  /// The tree's texts after the inputs changed: "v2 Voltage" under input 2.
  void        updateNames();
  void        updateList();
  void        changeInputs(const QStringList &inputs, int leading);

  Mode          m_mode;
  QTreeWidget  *m_tree;
  QListWidget  *m_list = nullptr;
  QPushButton  *m_up = nullptr;
  QPushButton  *m_down = nullptr;
  QPushButton  *m_lead = nullptr;
  QList<Source> m_sources;
  QStringList   m_inputs;
  int           m_leading = 0;
  ValueRef      m_value = ValueRef(0);
  bool          m_filling = false;
  bool          m_updating = false;   ///< updateList() fills the list: no drop
  std::function<QString(const QString &)> m_blocked;
};
