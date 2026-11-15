// Copyright (c) 2026 The QtDMM developers
// SPDX-License-Identifier: GPL-3.0-or-later
#include "ui/views/graphcommon.h"

GraphCommon::GraphCommon(QWidget *parent) : QWidget(parent)
{
}

void GraphCommon::setThemeColors(const QBrush &background, const QColor &grid, const QColor &labels,
                                 const QColor &data)
{
  setTheme(PlotTheme{ background, grid, labels, data });
}

void GraphCommon::setTheme(const PlotTheme &theme)
{
  m_theme = theme;
}

void GraphCommon::setTitle(const QString &title)
{
  m_title = title;
}

void GraphCommon::setHold(bool hold)
{
  m_hold = hold;
}
