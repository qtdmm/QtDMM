//======================================================================
// File:		configdlg.h
// Author:	Matthias Toussaint
// Created:	Sat Oct 19 14:53:06 CEST 2002
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
// SPDX-License-Identifier: GPL-3.0-or-later
//======================================================================

#pragma once

#include <QtSerialPort>

#include "ui_uisettingsdialog.h"
#include "ui/views/graphwidget.h"
#include "device/frameformat.h"
#include "device/dmmdecoder.h"
#include "device/metersetup.h"

class Settings;
class DeviceLibrary;
class QPrinter;
class RecorderPrefs;
class GeneralPrefs;
class GuiPrefs;
class GraphPrefs;
class AlarmPrefs;
class ScpiPrefs;
class KeyboardPrefs;
class ShortcutRegistry;
#include "core/alarm.h"
class SettingsPage;

/// The settings dialog: a category list beside a stack of SettingsPage pages.
///
/// Besides hosting the pages it is the read-only facade the rest of the
/// application uses for configuration values - InstanceWidget, the graph and the
/// meter ask the accessors here rather than reading Settings keys
/// themselves. OK/Apply run applySLOT() on every page, save the Settings and
/// emit applied()/accepted(); Cancel restores the pages from the stored
/// values.
class SettingsDialog : public QDialog, private Ui::UISettingsDialog
{
  Q_OBJECT
public:
  /// The ids double as the page index in ui_stack, so this is also the
  /// order of the category list.
  enum PageType
  {
    General = 0,
    GUI,   ///< "Appearance"
    Graph,      ///< with the Y axis, the time window and the integration curve
    Recorder,
    Alarms,
    Scpi,
    Keyboard,   ///< the keys of the main window, for all instances
    NumItems,
  };

  SettingsDialog(Settings* settings, QWidget *parent = nullptr);

  /// @name The pages
  /// What the instance reads from the settings comes from the pages
  /// directly; the dialog adds only what is not a page's.
  /// @{
  GeneralPrefs         *generalPage() const { return m_general; }
  GuiPrefs             *guiPage() const { return m_gui; }   ///< "Appearance"
  GraphPrefs           *graphPage() const { return m_graph; }
  RecorderPrefs        *recorderPage() const { return m_recorder; }
  AlarmPrefs           *alarmPage() const { return m_alarms; }
  ScpiPrefs            *scpiPage() const { return m_scpi; }
  KeyboardPrefs        *keyboardPage() const { return m_keyboard; }
  /// @}

  /// The recording length as asked when Record is pressed (RecordLengthDlg):
  /// a value, 0 = until stopped, and its unit (0 s, 1 min, 2 h, 3 d). Stored
  /// at once.
  void                  setRecordingLength(int value, int unit);

  /// The window at program exit (General page).
  void                  setWinRect(const QRect &);
  QRect                 winRect() const;

  /// Printer settings are kept in the Settings too.
  void                  writePrinter(QPrinter *);
  void                  readPrinter(QPrinter *);
  /// Raises the dialog on the given page.
  void                  showPage(PageType);

public Q_SLOTS:
  /// OK or Apply: applies all pages and saves.
  void                  on_ui_buttonBox_accepted();
  void                  on_ui_buttonBox_rejected();
  /// The graph changed the sample time (recorder page follows).
  void                  setSampleTimeSLOT(int);
  /// A time button of the graph: the visible window, applied like a zoom.
  void                  setWindowSecondsSLOT(int seconds);
  void                  zoomInSLOT(double);
  void                  zoomOutSLOT(double);
  /// A threshold cursor was dragged in the graph; updates the spin box.
  void                  thresholdChangedSLOT(GraphWidget::CursorMode, double);

Q_SIGNALS:
  /// Apply pressed: settings saved, dialog stays open.
  void applied();
  /// OK pressed: settings saved, dialog closes.
  void                  accepted();
  void                  rejected();
  /// The visible window changed on the graph page.
  void                  zoomed();

protected:
  QPrinter             *m_printer;
  QRect                 m_winRect;
  RecorderPrefs        *m_recorder;
  GeneralPrefs         *m_general;
  GuiPrefs             *m_gui;
  GraphPrefs           *m_graph;
  AlarmPrefs           *m_alarms;
  ScpiPrefs            *m_scpi;
  KeyboardPrefs        *m_keyboard;
  bool                  m_buttonBox_OK;

  void                  reloadSettings();
  /// Puts @p page into the stack, inside a scroll area: the tallest page
  /// would otherwise make the dialog taller than a 1080p screen.
  void                  addPage(SettingsPage *page);
  /// The settings page at stack index @p index (unwrapped from its scroll area).
  SettingsPage           *page(int index) const;
  /// Keeps the dialog within the available area of its screen.
  void                  showEvent(QShowEvent *event) override;
  /// The page's name and its line in the header.
  void                  showHeader(SettingsPage *page);
  /// The list's symbols after the design changed (light or dark).
  void                  updateIcons();

protected Q_SLOTS:
  void                  on_ui_list_currentItemChanged(QListWidgetItem *current, QListWidgetItem *);
  void                  on_ui_factoryDefaults_clicked();
  void                  on_ui_buttonBox_clicked(QAbstractButton *button);

private:
  Settings             *m_settings;

};

