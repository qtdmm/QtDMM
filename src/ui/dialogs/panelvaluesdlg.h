// Copyright (c) 2026 The QtDMM developers
// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <QDialog>
#include <QList>
#include <QStringList>

class QComboBox;

/// Values... in a front panel's context menu: which of its device's values
/// the LCD shows big and on the small line. A value chosen for a second place
/// swaps with the one there, as LCD in the menu of a value does.
class PanelValuesDlg : public QDialog
{
  Q_OBJECT
public:
  /// @p names the device's values by id; @p main and @p secondaries as the
  /// panel has them.
  PanelValuesDlg(const QStringList &names, int main, const QList<int> &secondaries, QWidget *parent = nullptr);

  /// The big line's value.
  int        main() const;
  /// The small line's, left to right.
  QList<int> secondaries() const;

  QComboBox *place(int i) const { return m_places.value(i); }

private:
  void       swapInto(int place);

  QList<QComboBox *> m_places;   ///< the big line, then the small line's
  QList<int>         m_chosen;   ///< as they were before the last change
};
