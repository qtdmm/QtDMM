//======================================================================
// File:		mainwid.h
// Author:	Matthias Toussaint
// Created:	Tue Apr 10 17:25:07 CEST 2001
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
// Copyright (c) 2001 Matthias Toussaint
//======================================================================

#pragma once

#include <QtGui>
#include <QtPrintSupport>

#include "ui_uiinstancewidget.h"

#include "core/limittest.h"
#include "core/reading.h"
#include "core/valueref.h"
#include "ui/dialogs/printdlg.h"
#include "ui/settings/recorderprefs.h"

class MeterController;
class RecordingStore;
class DeviceSession;
class ValueFeed;
class SettingsDialog;
class Settings;
class AnalogMeter;
class PoincarePlot;
class ReadingsModel;
struct Alarm;
class DeviceLibrary;

/// The recorder graph (shown in its own MDI window), plus the settings and
/// the other dialogs.
///
/// The window has no meter of its own: every meter connected is one of the
/// DeviceSession (MeterController: connection, min/max memory, alarms, SCPI,
/// the alarms' programs), each with its FrontPanel, which MainWindow keeps.
/// The views (analog meter, readings table, graph, Poincaré plot) show the
/// values they are given (ValueRef with the meter's source) and know nothing
/// of each other. MainWindow provides the frame (menus, toolbars, docks,
/// status bar) and hooks its actions up to the *SLOT members here.
class InstanceWidget : public QFrame, private Ui::UIInstanceWidget
{
  Q_OBJECT
public:
  /// @param instanceId  name of this instance for multi-instance setups (--config-id)
  /// @param configPath  directory of the settings file (--config-dir), or empty
  /// @param session     the meters of the process; its network services are
  ///                    set up from this window's settings
  /// @param parent      the MainWindow
  InstanceWidget(QString instanceId, QString configPath, DeviceSession *session, QWidget *parent = nullptr);
  /// Saves the settings and asks about unsaved data. Returns false when the
  /// user cancels; MainWindow then ignores the close event.
  bool        closeWin();
  /// The settings dialog: its pages hold what MainWindow reads (the window
  /// at exit, the analog meter's style, the shown display, the keys).
  SettingsDialog *settingsDialog() const { return m_configDlg; }
  /// The analog meter's style from its context menu (0 dark, 1 ivory):
  /// shown at once, saved, and the Appearance page follows.
  void        setMeterStyle(int style);
  /// The active device (MainWindow), the one the settings' Alarms page
  /// shows first: its source.
  void        setActiveSource(const QString &source) { m_activeSource = source; }
  /// The settings' Alarms page with the alarms of the meter @p source
  /// (MeterController::source()).
  void        openAlarms(const QString &source);
  /// The analog meter to feed; created and docked by MainWindow.
  void        setMeter(AnalogMeter *);
  /// The Poincaré plot (MainWindow owns and feeds it), for its value.
  void        setPoincare(PoincarePlot *plot);
  /// Table model that gets every reading (MainWindow owns it).
  void        setReadingLog(ReadingsModel *);
  /// @name Which value a view shows
  /// The sidebar's value menu (ValueMenu), kept with the window layout:
  /// a meter's value (ValueFeed), "no value" while the meter is not connected.
  /// @{
  void        setMeterValue(const ValueRef &value);
  void        setPoincareValue(const ValueRef &value);
  ValueRef    meterValue() const { return m_meterValue; }
  ValueRef    poincareValue() const { return m_poincareValue; }
  /// @}
  /// The limit test's band to the graph and the analog meter (they draw it
  /// for the value they show).
  void        applyLimitBands();
  /// --debug: pass on to MeterConnection::setConsoleLogging() of every meter.
  void        setConsoleLogging(bool);
  /// The recorder graph (for the zoom/pan shortcuts in MainWindow).
  GraphWidget   *graph() const { return ui_graph; }
  Settings   *settings() const { return m_settings; }
  /// "My devices", shared by all instances.
  DeviceLibrary *devices() const { return m_devices; }
  /// The entry of My devices a window from before 26.4 had as its own meter
  /// (DMM/my-device), once: its rules from the window go into the entry and
  /// the key goes. A meter set up in the window without an entry (Manual)
  /// gets one, or takes the entry at its place with its model. Empty when
  /// the window had none.
  QString     takeOldDevice();
  /// Before a new recording: unsaved readings viewed are exported or
  /// dropped, or the user cancels (false). True at once in Live.
  bool        confirmRecording();
  /// Before a recording started by hand: asks for its length, the last
  /// one ready (RecordLengthDlg); false when the user cancels.
  bool        askRecordingLength();
  /// The Recording page's settings to the session (DeviceSession::setRecordingSetup()).
  void        applyRecordingSetup();
  /// The values the recording's threshold can watch: of every meter connected.
  QList<RecorderPrefs::TriggerChoice> triggerChoices() const;
  /// --config-dir as given (empty: the default), for the instances started from here.
  QString     configPath() const { return m_configPath; }

Q_SIGNALS:
  /// The recorder is in View, Live or Recording now (RecordingStore::State).
  void        recorderState(int state);
  /// Message for the status bar's info field.
  void        info(const QString &);
  /// Message for the status bar's connection field (from MeterConnection::error()).
  void        error(const QString &);
  /// The "icons with text" preference changed.
  void        useTextLabel(bool);
  /// The symbols chosen: "colored", "plain" or "system" (Designs::IconSet).
  void        iconSet(const QString &set);
  /// Whether the front panels are shown, read from the settings, for MainWindow.
  void        showDisplay(bool show);
  /// The settings were applied (the LCDs' look, the services).
  void        configChanged();
  /// The SCPI server or HiSLIP started, stopped or got a client
  /// (ServiceHost::services()).
  void        servicesChanged();

public Q_SLOTS:
  void        quitSLOT();
  void        helpSLOT();
  /// Clears the graph.
  void        clearSLOT();
  /// Starts recording (also triggered remotely via the shared state).
  void        startSLOT();
  void        stopSLOT();
  /// Back to Live from View: unsaved readings are offered for export first.
  void        liveSLOT();
  /// Opens the settings dialog on General (already open: where it is).
  void        configSLOT();
  /// Opens the settings dialog on the recording page.
  void        configRecorderSLOT();
  /// Opens the settings dialog on the SCPI server page.
  void        configScpiSLOT();
  void        printSLOT();
  void        exportSLOT();
  void        importSLOT();
  /// Settings dialog OK/Apply: re-reads the configuration (readConfig()).
  void        applySLOT();

protected:
  /// Shows the settings dialog, on @p page (a SettingsDialog::PageType) unless -1.
  void        openConfig(int page = -1);
  void        applyMeterStyle();   ///< style, ballistics, red zone from the settings
  DeviceSession   *m_session;
  ValueFeed       *m_meterFeed = nullptr;     ///< the analog meter's
  ValueFeed       *m_poincareFeed = nullptr;  ///< the Poincaré plot's
  SettingsDialog  *m_configDlg;
  qtdmm::PrintDlg *m_printDlg;
  QPrinter    m_printer;
  QColor      m_alertColor;   ///< the raised alarm's colour for the analog meter, invalid when none
  QMetaObject::Connection m_alertLink;   ///< the alarm colour of the analog meter's meter
  QMetaObject::Connection m_meterLimitLink;   ///< the limit test of the analog meter's meter
  /// The graph's store while no meter has it: empty, Live (time axis only).
  RecordingStore *m_emptyStore = nullptr;
  QPointer<MeterController> m_graphBase;   ///< the meter of the graph's store, nullptr: m_emptyStore
  QMetaObject::Connection m_graphUnit;      ///< its unit to the graph
  MeterController *m_closing = nullptr;   ///< the meter closing now (DeviceSession::closing())
  QMetaObject::Connection m_graphAlarms;    ///< its alarms: the reference line's thresholds
  /// The graph shows none of its store's meter's values: the meter of its first.
  void        followGraphBase();
  /// The graph's store: of @p ctl, or the empty one for nullptr.
  void        setGraphBase(MeterController *ctl);
  /// The alarms' thresholds of the graph's meter, for its reference line to snap to.
  void        applyAlarmThresholds();
  QMetaObject::Connection m_graphLimitLink;   ///< of the meter of the graph's left axis
  bool        m_alertBlink = false;   ///< Alarms/blink
  bool        m_alertPhase = true;    ///< blinking: the colour shown now
  QTimer     *m_alertTimer = nullptr;
  QString     m_activeSource;   ///< setActiveSource()
  bool        m_consoleLogging = false;   ///< setConsoleLogging()
  /// The analog meter in the alarm's colour (or its own in the dark phase
  /// of the blinking).
  void        showAlertColor();
  AnalogMeter   *m_meter;
  PoincarePlot  *m_poincare = nullptr;
  QPointer<ReadingsModel> m_readingLog;
  ValueRef    m_meterValue;
  ValueRef    m_poincareValue;
  /// The values chosen to the views.
  void        applyValueChoice();
  /// The meters' values by name: the recording's choice of trigger value.
  void        updateValueNames();
  Settings    *m_settings;
  /// Asks about unsaved readings before they are lost (@p text says how,
  /// @p discard is the button that drops them): export first, drop or
  /// cancel. True when the caller may go on - nothing unsaved, exported or
  /// dropped; false for Cancel or a failed export.
  bool        keepUnsavedData(QString text, const QString &discard);
  /// The session's recording state (RecordingStore::State): the leading meter's, else the graph store's.
  int         recordingState() const;
  DeviceLibrary *m_devices;
  QString     m_instanceId;
  QString     m_configPath;   ///< --config-dir, passed on to new instances

  /// Applies the settings to the services, graph and analog meter.
  void        readConfig();
  /// The SCPI page's settings to the session's services (ServiceHost::apply()).
  void        applyScpi();
  QRect       parentRect() const;

protected Q_SLOTS:
  /// Graph zoom changed; re-applies the window/total size.
  void        zoomedSLOT();
};

