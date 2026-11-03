// Copyright (c) 2026 The QtDMM developers
// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <QBrush>
#include <QColor>
#include <QTimer>
#include <QWidget>

#include "core/poincare.h"

class QMenu;
class Settings;

/// The Poincaré plot: each reading against the one k readings later
/// (PoincareSeries), so the scatter of a value shows at a glance - noise
/// across the dashed diagonal, drift along it. The cloud fades from the
/// newest point to the oldest; the ellipse has the semi-axes SD1 (across)
/// and SD2 (along), their values in the corner.
///
/// Both axes have the same scale, the plot is square. It takes every main
/// reading while it exists, shown or not, and repaints at most 20 times a
/// second. Hold freezes the picture, the series goes on underneath.
/// Settings: Poincare/capacity, Poincare/lag, Poincare/ellipse.
class PoincarePlot : public QWidget
{
  Q_OBJECT
public:
  explicit PoincarePlot(QWidget *parent = nullptr);

  /// Reads the plot's settings from @p cfg and writes changes back there.
  void        setSettings(Settings *cfg);
  /// Colours of the window design (Designs::graphColors()); an invalid
  /// @p data takes the highlight colour.
  void        setThemeColors(const QBrush &background, const QColor &grid, const QColor &labels, const QColor &data);
  /// The plot's own entries for the window's context menu.
  void        addMenuActions(QMenu *menu);

  const PoincareSeries &series() const { return m_series; }
  void        setLag(int k);
  void        setCapacity(int values);
  void        setEllipse(bool on);
  bool        ellipse() const { return m_ellipse; }
  void        setHold(bool on);
  bool        hold() const { return m_hold; }
  /// Which value of the meter it plots (Reading::id; the context menu
  /// "Value"); another one starts afresh.
  void        setValueId(int id);
  int         valueId() const { return m_valueId; }
  /// Writes the plot as PNG, JPEG or SVG (by suffix).
  bool        exportImageFile(const QString &fileName, QSize size = QSize());

public Q_SLOTS:
  /// A reading from the MeterController; only the value valueId() counts.
  void        addReading(const Reading &reading);
  /// The main value went stale (true) or is current again.
  void        setStale(bool stale);
  void        clear();
  /// Where the value comes from (ValueFeed::caption()), above the plot.
  void        setCaption(const QString &caption);
  /// Why there are no points (ValueFeed::notice()), in place of them.
  void        setNotice(const QString &notice);

protected:
  void        paintEvent(QPaintEvent *) override;
  void        mouseMoveEvent(QMouseEvent *) override;
  void        leaveEvent(QEvent *) override;

private:
  void        scheduleUpdate();
  void        render(QPainter &p, const QRect &area);
  /// The square plot area inside @p area, room for the labels around it.
  QRect       plotRect(const QRect &area) const;
  bool        exportImageSLOT();

  PoincareSeries m_series;
  QVector<PoincareSeries::Pair> m_shown;   ///< the pairs drawn (frozen while held)
  PoincareSeries::Stats m_stats;
  QString     m_unit;
  double      m_resolution = 0;
  bool        m_hold = false;
  int         m_valueId = 0;
  bool        m_ellipse = true;
  Settings   *m_cfg = nullptr;
  QTimer      m_repaint;   ///< with the FrameClock off
  /// The readings collected since into the picture (unless held), drawn.
  void        refresh();

  // the axes of the last paint, for the crosshair
  double      m_lo = 0;
  double      m_hi = 1;
  QRect       m_plot;
  QPoint      m_mouse;
  bool        m_mouseIn = false;

  QBrush      m_background;
  QString     m_caption;   ///< setCaption()
  QString     m_notice;    ///< setNotice()
  QColor      m_grid;
  QColor      m_labels;
  QColor      m_data;
};
