//======================================================================
// File:		mainwid.h
// Author:	Matthias Toussaint
// Created:	Tue Apr 10 17:25:07 CEST 2001
//----------------------------------------------------------------------
// This file is part of QtDMM.
//
// QtDMM is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License version 3
// as published by the Free Software Foundation.
//
// QtDMM is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with Foobar.  If not, see <http://www.gnu.org/licenses/>.
//----------------------------------------------------------------------
// Copyright (c) 2001 Matthias Toussaint
//======================================================================

#pragma once


#include <QtGui>
#include <QtPrintSupport>

#include "ui_uiinstancewidget.h"

#include "ui/dialogs/printdlg.h"

class MeterController;
class SettingsDialog;
class LcdWidget;
class Settings;
class AnalogMeter;
class ReadingsModel;
struct Alarm;
class SharedStateManager;
class DeviceLibrary;

/// The recorder graph (shown in its own MDI window), plus the settings and
/// the other dialogs.
///
/// The meter session itself - connection, min/max memory, alarms, SCPI
/// server, the alarms' programs - is a MeterController, which InstanceWidget creates.
/// The views (display, analog meter, readings table, graph) are connected to
/// its signals here and know nothing of each other. MainWindow provides the
/// frame (menus, toolbars, docks, status bar) and hooks its actions up to
/// the *SLOT members here.
class InstanceWidget : public QFrame, private Ui::UIInstanceWidget
{
  Q_OBJECT
public:
  /// @param instance_id  name of this instance for multi-instance setups (--config-id)
  /// @param config_path  directory of the settings file (--config-dir), or empty
  /// @param parent       the MainWindow
  InstanceWidget(QString instance_id, QString config_path, QWidget *parent = Q_NULLPTR);
  /// Disconnects, saves the settings and asks about unsaved data. Returns
  /// false when the user cancels; MainWindow then ignores the close event.
  bool        closeWin();
  /// Window geometry stored in the settings.
  QRect       winRect() const;
  bool        saveWindowPosition() const;
  bool        saveWindowSize() const;
  /// The LCD panel to feed; created and docked by MainWindow.
  void        setDisplay(LcdWidget *);
  /// The LCD's colours (LcdWidget::LcdVariant), stored as Display/lcd.
  void        setLcdVariant(int variant);
  /// The analog meter's style from its context menu (0 dark, 1 ivory):
  /// shown at once, saved, and the Appearance page follows.
  void        setMeterStyle(int style);
  int         meterStyle() const;
  /// The analog meter to feed; created and docked by MainWindow.
  void        setMeter(AnalogMeter *);
  /// Table model that gets every reading (MainWindow owns it).
  void        setReadingLog(ReadingsModel *);
  /// The instance coordinator; readings are published through it.
  void        setStateManager(SharedStateManager *);
  /// --debug: pass on to MeterConnection::setConsoleLogging().
  void        setConsoleLogging(bool);
  /// Stores whether the display is shown in the settings.
  void        setShowDisplay(bool show);
  /// The recorder graph (for the zoom/pan shortcuts in MainWindow).
  GraphWidget   *graph() const { return ui_graph; }
  /// False until a device has been chosen once (sidebar, Add device); a fresh
  /// instance does not try to connect to a guessed port on its own.
  bool        dmmConfigured() const;
  /// What the window title shows: the model, else the port, else a hint.
  QString     dmmTitle() const;
  /// The configured port for display ("/dev/ttyUSB0", "BLE AA:BB:...";
  /// never a Bluetooth key).
  QString     portName() const;
  /// The meter session (connection, alarms, SCPI); views connect to it.
  MeterController *controller() const { return m_ctl; }
  Settings   *settings() const { return m_settings; }
  /// "My devices", shared by all instances.
  DeviceLibrary *devices() const { return m_devices; }
  /// The entry of "My devices" this instance uses now (its key
  /// DMM/my-device); empty when none.
  QString     currentDevice() const;
  /// Switches to the device @p id of "My devices": disconnects, takes over
  /// its meter keys and connects again. A running recording stops with a
  /// message - another meter is another function; unsaved readings are
  /// offered for export first unless @p ask is false (confirmSwitch()).
  /// False when there is no such entry or the user cancels.
  bool        switchDevice(const QString &id, bool ask = true);
  /// Before a switch to the device @p name: unsaved readings are exported
  /// or dropped, or the user cancels (false).
  bool        confirmSwitch(const QString &name);
  /// Starts a new instance with this one's settings and the device @p id of
  /// "My devices", named after the device (DeviceLibrary::instanceId());
  /// returns the instance name, empty when there is no such entry.
  QString     openInNewWindow(const QString &id);
  /// Before a new recording: unsaved readings viewed are exported or
  /// dropped, or the user cancels (false). True at once in Live.
  bool        confirmRecording();
  /// Before a recording started by hand: asks for its length, the last
  /// one ready (RecordLengthDlg); false when the user cancels.
  bool        askRecordingLength();
  /// --config-dir as given (empty: the default), for the instances started from here.
  QString     configPath() const { return m_configPath; }

Q_SIGNALS:
  /// Recording started/stopped (graph state).
  void        running(bool);
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
  /// The configured meter has keys QtDMM can press (ControlBar).
  void        remoteControl(bool supported);
  /// Asks MainWindow to connect/disconnect (drives the Connect action).
  void        setConnect(bool);
  /// Whether the display is shown, read from the settings, for MainWindow.
  void        showDisplay(bool show);
  /// The connection state changed; MainWindow checks the Connect action.
  void        connectDMM(bool);
  /// The settings were applied; the meter shown in the title may have changed.
  void        configChanged();
  /// A state string for the other instances (SharedStateManager).
  void        sendState(const QString&);
  /// The SCPI server's state for the status bar ("SCPI 5025", empty = off).
  void        scpiStatus(const QString&);

public Q_SLOTS:
  /// Clears min/max memory and the meter's peak/auto-bipolar latch.
  void        resetSLOT();
  /// Connect (true) or disconnect (false) the meter.
  void        connectSLOT(bool);
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
  void        printSLOT();
  void        exportSLOT();
  void        importSLOT();
  /// Graph started/stopped recording.
  void        runningSLOT(bool);
  /// Settings dialog OK/Apply: re-reads the configuration (readConfig());
  /// the connection stays as it is.
  void        applySLOT();

protected:
  /// Shows the settings dialog, on @p page (a SettingsDialog::PageType) unless -1.
  void        openConfig(int page = -1);
  void        applyMeterStyle();   ///< style, ballistics, red zone from the settings
  MeterController *m_ctl;
  SettingsDialog  *m_configDlg;
  qtdmm::PrintDlg *m_printDlg;
  QPrinter    m_printer;
  LcdWidget *m_display;
  QColor      m_lcdTint;   ///< the Appearance page's tint last applied
  AnalogMeter   *m_meter;
  /// The desktop part of an alarm: beep, raise the window, popup.
  void        alarmRaised(const Alarm &alarm, const QString &shown, const QString &text);
  Settings    *m_settings;
  /// Disconnects, takes over @p keys of the entry @p id, clears the graph,
  /// connects; @p name for the messages.
  void        takeOver(const QVariantMap &keys, const QString &name, const QString &id);
  /// After the settings were applied: the entry in use follows them (same
  /// model: the same meter, its entry changes along); another model is
  /// another meter - the entry at its place, or none.
  void        syncDevice();
  /// Opens or closes the port without touching the recorder (connectSLOT()
  /// stops it on closing; an alarm's program does not).
  void        connectPort(bool on);
  /// Asks about unsaved readings before they are lost (@p text says how,
  /// @p discard is the button that drops them): export first, drop or
  /// cancel. True when the caller may go on - nothing unsaved, exported or
  /// dropped; false for Cancel or a failed export.
  bool        keepUnsavedData(const QString &text, const QString &discard);
  DeviceLibrary *m_devices;
  QString     m_instanceId;
  QString     m_configPath;   ///< --config-dir, passed on to new instances

  /// Applies the settings to the MeterConnection, graph, display and meter.
  void        readConfig();
  /// The meter's keys (and sigrok-cli) the port was set up with last:
  /// readConfig() opens the port again only when they changed.
  QVariantMap m_appliedMeter;
  QRect       parentRect() const;

protected Q_SLOTS:
  /// Graph zoom changed; re-applies the window/total size.
  void        zoomedSLOT();
};

