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
class QCPTextElement;
class QCustomPlot;

/// A side plot drawn with QCustomPlot (XY diagram, histogram, heatmap): the
/// plot with its title row, the "no data" and HOLD notes, the colours, the
/// value crosshair under the mouse, OpenGL, and the drawing pace - new
/// samples are drawn at most about 30 times a second (scheduleRefresh()).
/// A plot keeps its samples, draws them in refreshPlot() and freezes a copy
/// in holdChanged().
class QcpGraphCommon : public GraphCommon
{
  Q_OBJECT
public:
  /// @p noData: the note while there is nothing to draw.
  explicit QcpGraphCommon(QWidget *parent, const QString &noData);

  /// Whether new plots draw with OpenGL (Graph/openGl, read at start, off by
  /// default: QCustomPlot's OpenGL shows artefacts and costs more with several
  /// plots open; NeoQCP is to do it better).
  static void setOpenGlEnabledDefault(bool enabled);

  void setNoDataText(const QString &text);
  void setTitle(const QString &title) override;
  void setTheme(const PlotTheme &theme) override;
  void setHold(bool hold) override;
  void clear() final;

protected:
  /// Draws the samples (all, or the copy hold froze); the plot calls it at
  /// the end of its constructor too.
  virtual void refreshPlot() = 0;
  /// Hold was switched: keep a copy of the samples, or drop it.
  virtual void holdChanged(bool hold) = 0;
  /// Forgets the samples (clear()).
  virtual void clearSamples() = 0;
  /// The crosshair's text for the axis coordinates @p x, @p y (base: both
  /// values with the axes' units).
  virtual QString crosshairText(double x, double y) const;

  /// refreshPlot() soon, at most every kRefreshMs.
  void scheduleRefresh();
  void setNoDataVisible(bool visible);
  /// @p range on @p axis: its limits in the range's unit, its title; the
  /// crosshair takes the unit.
  void setAxisRange(QCPAxis *axis, const PlotAxis::Range &range);
  QString axisValueText(QCPAxis *axis, double value) const;

  QCustomPlot *m_plot = nullptr;

private:
  static constexpr int kRefreshMs = 33;
  bool eventFilter(QObject *watched, QEvent *event) override;
  void moveCrosshair(QMouseEvent *event);
  void hideCrosshair();

  QTimer               m_refreshTimer;
  QCPTextElement      *m_titleElement = nullptr;
  QCPItemText         *m_noData = nullptr;
  QCPItemText         *m_holdItem = nullptr;
  QCPLayer            *m_crossLayer = nullptr;   ///< replotted on its own when the mouse moves
  QCPItemStraightLine *m_crossX = nullptr;
  QCPItemStraightLine *m_crossY = nullptr;
  QCPItemText         *m_crossText = nullptr;
  QHash<QCPAxis *, QString> m_axisUnits;
  static bool          s_openGlDefault;
};
