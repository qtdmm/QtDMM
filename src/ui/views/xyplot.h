// Copyright (c) 2026 The QtDMM developers
// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <QPointF>
#include <QBrush>
#include <QColor>
#include <QVector>
#include <QWidget>

/// A live scatter plot of one value against another.
class XYPlot : public QWidget
{
  Q_OBJECT
public:
  explicit XYPlot(QWidget *parent = nullptr);
  void        setThemeColors(const QBrush &background, const QColor &grid, const QColor &labels, const QColor &data);
  void        setAxisUnits(const QString &x, const QString &y);
  void        setLegend(const QString &legend);
  void        addPoint(double x, double y);
  void        clear();
  void        setHold(bool hold);
  bool        hold() const { return m_hold; }

protected:
  void        paintEvent(QPaintEvent *) override;

private:
  static constexpr int kMaxPoints = 2000;

  QVector<QPointF> m_points;
  QVector<QPointF> m_shown;
  int         m_pointHead = 0;
  bool        m_hold = false;
  QString     m_xUnit;
  QString     m_yUnit;
  QString     m_legend;
  QBrush      m_background;
  QColor      m_grid;
  QColor      m_labels;
  QColor      m_data;
};
