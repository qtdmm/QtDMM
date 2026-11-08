// Copyright (c) 2026 The QtDMM developers
// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "ui/settings/settingspage.h"

class QCheckBox;
class QLineEdit;

/// Settings page "General": what QtDMM does at program exit (warn about
/// unsaved data, keep the window's position and size) and the programs it
/// runs (sigrok-cli, LabPlot), the folder for new project baskets. Keys
/// Alert/unsaved-file, Save/window-pos, Save/window-size,
/// Port settings/sigrok_exe, Programs/labplot, Basket/base-dir.
class GeneralPrefs : public SettingsPage
{
  Q_OBJECT
public:
  explicit GeneralPrefs(QWidget *parent = nullptr);

  bool alertUnsavedData() const;
  bool saveWindowPosition() const;
  bool saveWindowSize() const;
  /// The program for sigrok meters (Port settings/sigrok_exe, the same for all instances).
  QString sigrokExecutable() const;
  /// LabPlot as set (Programs/labplot); empty: searched (LabPlot::command()).
  QString labPlotExecutable() const;
  /// Where New project basket starts (Basket/base-dir; empty: Documents/QtDMM).
  QString basketBase() const;
  static QString defaultBasketBase();

public Q_SLOTS:
  void defaultsSLOT() override;
  void factoryDefaultsSLOT() override;
  void applySLOT() override;

private:
  QCheckBox *ui_alertUnsavedData, *ui_saveWindowPos, *ui_saveWindowSize;
  QLineEdit *ui_sigrokExe;
  QLineEdit *ui_labPlotExe;
  QLineEdit *ui_basketBase;
};
