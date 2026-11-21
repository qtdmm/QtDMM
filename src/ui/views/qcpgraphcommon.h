// Copyright (c) 2026 The QtDMM developers
// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "ui/views/qcpplotbase.h"

class QCPTextElement;

/// A side plot drawn with QCustomPlot (XY diagram, histogram, heatmap): the
/// plot of QcpPlotBase with a title row and the "no data" and HOLD notes.
/// A plot keeps its samples, draws them in refreshPlot() and freezes a copy
/// in holdChanged().
class QcpGraphCommon : public QcpPlotBase
{
  Q_OBJECT
public:
  /// @p noData: the note while there is nothing to draw.
  explicit QcpGraphCommon(QWidget *parent, const QString &noData);

  void setNoDataText(const QString &text);
  void setTitle(const QString &title) override;
  /// The title, and a shorter one for a narrow plot (the value names
  /// without their devices): the plot shows the longest that fits, else the
  /// short one cut with "…", and the full one as the title's tooltip.
  void setTitles(const QString &title, const QString &shortTitle);
  void setTheme(const PlotTheme &theme) override;
  void setHold(bool hold) override;
  void clear() final;

protected:
  /// Draws the samples (all, or the copy hold froze); the plot calls it at
  /// the end of its constructor too.
  void refreshPlot() override = 0;
  /// Hold was switched: keep a copy of the samples, or drop it.
  virtual void holdChanged(bool hold) = 0;
  /// Forgets the samples (clear()).
  virtual void clearSamples() = 0;

  void setNoDataVisible(bool visible);
  void resizeEvent(QResizeEvent *event) override;
  bool eventFilter(QObject *watched, QEvent *event) override;

private:
  /// The title that fits the plot's width now.
  void fitTitle();

  QString         m_shortTitle;
  QCPTextElement *m_titleElement = nullptr;
  QCPItemText    *m_noData = nullptr;
  QCPItemText    *m_holdItem = nullptr;
};
