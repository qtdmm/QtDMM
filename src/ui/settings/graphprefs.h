//======================================================================
// File:		graphprefs.h
// Author:	Matthias Toussaint
// Created:	Sat Oct 19 15:28:30 CEST 2002
//----------------------------------------------------------------------
// This file is part of QtDMM.
//
// QtDMM is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
//
// QtDMM is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with QtDMM.  If not, see <http://www.gnu.org/licenses/>.
//----------------------------------------------------------------------
// Copyright (c) 2002 Matthias Toussaint
//======================================================================

#pragma once

#include "ui/settings/settingspage.h"

class ColorButton;
class QCheckBox;
class QComboBox;
class QFormLayout;
class QSpinBox;

/// Settings page "Graph": colours, line and point style of the curves, the
/// window a new graph shows, the crosshair and the statistics' side.
class GraphPrefs : public SettingsPage
{
  Q_OBJECT
public:
  GraphPrefs(QWidget *parent = nullptr);
  ~GraphPrefs();
  /// The graphs' default colours, a GraphLook::name().
  QString       variant() const;
  QColor        bgColor() const;
  QColor        gridColor() const;
  QColor        dataColor() const;
  QColor        startColor() const;
  QColor        cursorColor() const;
  int           lineWidth() const;
  int           lineMode() const;
  int           pointMode() const;
  bool          crosshair() const;
  /// The statistics left of the graph (else right), for every graph.
  bool          statsLeft() const;

  /// The time window a new graph shows, seconds.
  int           windowSeconds() const;

public Q_SLOTS:
  virtual void  defaultsSLOT();
  virtual void  factoryDefaultsSLOT();
  virtual void  applySLOT();

private:
  /// Shows the own colours with Custom.
  void          updateRows();

  QFormLayout  *m_form;
  QComboBox    *m_variant;
  ColorButton  *ui_bgColor, *ui_gridColor, *ui_dataColor, *ui_cursorColor, *ui_startColor;
  QWidget      *m_ownColours;   ///< data, background, grid: only with Custom
  QComboBox    *ui_lineMode, *ui_pointMode;
  QSpinBox     *ui_lineWidth;
  QCheckBox    *ui_crosshair;
  QCheckBox    *ui_openGl;   ///< Graph/openGl, read at start (QcpGraphCommon)
  QSpinBox     *ui_winSize;
  QComboBox    *sizeUnit;
  QComboBox    *ui_statsSide;
};
