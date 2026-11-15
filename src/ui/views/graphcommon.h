// Copyright (c) 2026 The QtDMM developers
// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "ui/views/plottheme.h"

#include <QWidget>

/// What every side plot has, whatever draws it: the design's colours, a
/// title and hold. The plots keep and draw their samples themselves; the
/// ones on QCustomPlot build on QcpGraphCommon, the Poincaré plot paints
/// with QPainter (QCustomPlot does not suit it).
class GraphCommon : public QWidget
{
  Q_OBJECT
public:
  explicit GraphCommon(QWidget *parent = nullptr);

  /// The window design's colours; @p data may be invalid (a plot with colours of its own).
  void             setThemeColors(const QBrush &background, const QColor &grid, const QColor &labels,
                                  const QColor &data = QColor());
  virtual void     setTheme(const PlotTheme &theme);
  const PlotTheme &theme() const { return m_theme; }
  virtual void     setTitle(const QString &title);
  const QString   &title() const { return m_title; }
  /// Freezes the picture; readings still come in and show once it is released.
  virtual void     setHold(bool hold);
  bool             hold() const { return m_hold; }
  virtual void     clear() = 0;

private:
  PlotTheme m_theme;
  QString   m_title;
  bool      m_hold = false;
};
