// Copyright (c) 2026 The QtDMM developers
// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <QList>
#include <QString>
#include <QWidget>

#include <functional>

/// The states the meter reported with its readings, as a strip at the
/// bottom of the plot: for each kind a lane with a bar over the time it
/// held - HOLD (the display is frozen), MIN / MAX / PEAK (a stored extreme,
/// not the value now), REL (a difference), OL (overload). A lane has its
/// colour and names its state in the bar, so it reads without colour too.
/// The gaps have a lane too, each cause with a hatching of its own: the
/// port released on purpose, the connection lost, a silent meter, no
/// number, a fault.
/// Only the lanes of states in the window are drawn; without any, nothing.
class GraphStateStrip : public QWidget
{
public:
  enum Kind { Gap, Overload, Hold, Extreme, Relative, KindCount };
  /// A state over a stretch of the window, in fractions of its width (0..1).
  struct Span
  {
    Kind    kind = Hold;
    QString label;   ///< "HOLD", "MIN", "MAX", "PEAK", "REL", "OL"
    double  from = 0;
    double  to = 0;
    Qt::BrushStyle pattern = Qt::SolidPattern;   ///< a gap's cause
  };

  explicit GraphStateStrip(QWidget *parent);

  /// Where the spans come from, asked when the strip paints.
  void setProvider(std::function<QList<Span>()> provider) { m_provider = std::move(provider); }
  /// The height the strip takes for all lanes.
  static int fullHeight() { return KindCount * kLane; }
  static QColor colorOf(Kind kind);

protected:
  void paintEvent(QPaintEvent *) override;

private:
  static constexpr int kLane = 12;   ///< a lane's height
  std::function<QList<Span>()> m_provider;
};
