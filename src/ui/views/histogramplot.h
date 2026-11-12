// Copyright (c) 2026 The QtDMM developers
// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <QBrush>
#include <QColor>
#include <QVector>
#include <QWidget>

#include "core/reading.h"

class QCPBars;
class QCPItemText;
class QCPTextElement;
class QCustomPlot;

/// A live histogram of the selected value's recent readings.
class HistogramPlot : public QWidget
{
  Q_OBJECT
public:
  explicit HistogramPlot(QWidget *parent = nullptr);
  void        setThemeColors(const QBrush &background, const QColor &grid, const QColor &labels, const QColor &data);
  void        setValueId(int id);

public Q_SLOTS:
  void        addReading(const Reading &reading);
  void        clear();
  void        setHold(bool hold);
  bool        hold() const { return m_hold; }
  void        setCaption(const QString &caption);
  void        setNotice(const QString &notice);

private:
  static constexpr int kMaxValues = 1000;
  void refreshPlot();

  QVector<double> m_values;
  QVector<double> m_shown;
  int         m_valueHead = 0;
  bool        m_hold = false;
  int         m_valueId = 0;
  QString     m_caption;
  QString     m_notice;
  QString     m_unit;
  QCustomPlot *m_plot = nullptr;
  QCPBars    *m_bars = nullptr;
  QCPTextElement *m_title = nullptr;
  QCPItemText *m_noDataText = nullptr;
  QCPItemText *m_holdText = nullptr;
  QBrush      m_background;
  QColor      m_grid;
  QColor      m_labels;
  QColor      m_data;
};
