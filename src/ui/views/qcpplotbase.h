// Copyright (c) 2026 The QtDMM developers
// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "ui/views/graphcommon.h"
#include "ui/views/plotaxis.h"

#include <QHash>
#include <QTimer>

class QCPAxis;
class QCPItemStraightLine;
class QCPItemText;
class QCPLayer;
class QCustomPlot;

/// A view drawn with QCustomPlot: the plot filling the widget, the design's
/// colours, the right click handed to the widget, the value crosshair under
/// the mouse on a layer of its own, OpenGL, and the drawing pace - new
/// samples are drawn at most about 30 times a second (scheduleRefresh()).
/// Where the samples come from is the subclass's: the side plots keep them
/// (QcpGraphCommon), the main graph reads the recording.
class QcpPlotBase : public GraphCommon
{
  Q_OBJECT
public:
  explicit QcpPlotBase(QWidget *parent);

  /// Whether new plots draw with OpenGL (Graph/openGl, read at start, off by
  /// default: QCustomPlot's OpenGL shows artefacts and costs more with several
  /// plots open; NeoQCP is to do it better).
  static void setOpenGlEnabledDefault(bool enabled);

  void setTheme(const PlotTheme &theme) override;

protected:
  /// Draws the samples; called by the drawing pace.
  virtual void refreshPlot() = 0;
  /// The crosshair's text for the axis coordinates @p x, @p y (base: both
  /// values with the axes' units).
  virtual QString crosshairText(double x, double y) const;
  /// Where the crosshair goes for the mouse at the axis coordinates @p x,
  /// @p y, and what it says. The base: at the mouse, crosshairText(). A plot
  /// may put it onto a reading instead, or leave out the horizontal line
  /// where there is no value.
  struct CrosshairAt
  {
    double  x = 0;
    double  y = 0;
    bool    horizontal = true;
    QString text;
  };
  virtual CrosshairAt crosshairAt(double x, double y);
  /// The crosshair's lines in @p pen instead of the lettering's dotted one.
  void setCrosshairPen(const QPen &pen);

  /// refreshPlot() soon, at most every kRefreshMs; not while hold is on.
  void scheduleRefresh();
  /// Drops a refreshPlot() that is still to come.
  void cancelRefresh();
  /// Off: no crosshair, and the one shown goes (a plot with nothing to point at).
  void setCrosshairEnabled(bool enabled);
  /// @p range on @p axis: its limits in the range's unit, its title; the
  /// crosshair takes the unit.
  void setAxisRange(QCPAxis *axis, const PlotAxis::Range &range);
  QString axisValueText(QCPAxis *axis, double value) const;

  /// The crosshair gone until the mouse moves again (a picture of the plot).
  void hideCrosshair();

  QCustomPlot *m_plot = nullptr;
  QCPItemText *m_crossText = nullptr;

private:
  static constexpr int kRefreshMs = 33;
  bool eventFilter(QObject *watched, QEvent *event) override;
  void moveCrosshair(QMouseEvent *event);

  QTimer               m_refreshTimer;
  QCPLayer            *m_crossLayer = nullptr;   ///< replotted on its own when the mouse moves
  QCPItemStraightLine *m_crossX = nullptr;
  QCPItemStraightLine *m_crossY = nullptr;
  bool                 m_crosshairEnabled = true;
  QHash<QCPAxis *, QString> m_axisUnits;
  static bool          s_openGlDefault;
};
