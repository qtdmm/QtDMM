// Copyright (c) 2026 The QtDMM developers
// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <QList>
#include <QPen>
#include <QWidget>

/// The legend top left in the plot: a piece of each curve's line - its
/// colour and its style, so the values tell apart also without colour -
/// and its name, in a row. Each name comes in shorter forms; the legend
/// takes the longest form of all names that fits its room.
class GraphLegend : public QWidget
{
public:
  struct Entry
  {
    QPen        pen;
    QStringList names;   ///< from the full one to the shortest (the unit alone); at least one
  };

  explicit GraphLegend(QWidget *parent);

  /// The curves, the colour of the names, and the width there is for the
  /// row (0: any): the first form of the names that fits, else the shortest.
  void setEntries(const QList<Entry> &entries, const QColor &ink, int room = 0);
  /// The names as shown, two spaces between them (for the tests).
  QString text() const;

  QSize sizeHint() const override;

protected:
  void paintEvent(QPaintEvent *) override;

private:
  static constexpr int kSample = 24;   ///< the piece of line
  static constexpr int kGap = 5;       ///< between it and the name
  static constexpr int kSpace = 14;    ///< between two entries

  QList<Entry> m_entries;
  int          m_form = 0;   ///< the form of the names shown
  /// The name of @p e in form @p form, its shortest when it has fewer.
  static QString name(const Entry &e, int form) { return e.names.value(qMin(form, int(e.names.size()) - 1)); }
  int          widthOf(int form) const;
  QColor       m_ink;
};
