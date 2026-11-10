// Copyright (c) 2026 The QtDMM developers
// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <QDialog>
#include <QList>

#include "core/valueref.h"

class QTreeWidget;
class QTreeWidgetItem;

/// "Sources...": which values of the open meters a view shows. The meters
/// with their values below them; the graph and the table take several
/// (check boxes), the analog meter and the Poincaré plot one.
class SourcesDlg : public QDialog
{
  Q_OBJECT
public:
  /// One open meter: its source (empty for the window's), its name and the
  /// names of its values by id.
  struct Meter
  {
    QString     source;
    QString     name;
    QStringList values;
  };

  /// @p view names the view in the title; @p several: check boxes, else one value.
  SourcesDlg(const QString &view, bool several, const QList<Meter> &meters, QWidget *parent = nullptr);
  void            setChosen(const QList<ValueRef> &values);
  /// The values ticked, meter by meter in their order.
  QList<ValueRef> chosen() const;
  /// The tree, for the tests.
  QTreeWidget    *tree() const { return m_tree; }

private:
  /// One value only: another one ticked unticks the one before.
  void            ticked(QTreeWidgetItem *item);

  QTreeWidget *m_tree;
  bool         m_several;
  bool         m_filling = false;
};
