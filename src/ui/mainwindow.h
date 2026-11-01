//======================================================================
// File:		mainwin.h
// Author:	Matthias Toussaint
// Created:	Sun Sep  2 12:14:07 CEST 2001
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

#include <functional>
#include <optional>

#include <QtGui>
#include <QtWidgets>
#include <QMenu>
#include <QCommandLineParser>

#include "ui_uimainwindow.h"
#include "core/limittest.h"
#include "core/recentfiles.h"
#include "core/valueref.h"
#include "ui/devicesidebar.h"
#include "ui/valuemenu.h"

class InstanceWidget;
class DeviceSession;
class WorkspaceFile;
class MeterController;
class LcdWidget;
class HelpDlg;
class AnalogMeter;
class ReadingsWidget;
class PoincarePlot;
class AlarmBar;
class ShortcutRegistry;
class FrontPanel;
class MdiArranger;
class QMdiArea;
class QToolButton;
class QPushButton;
class QDockWidget;
class DeviceSidebar;
class QMdiSubWindow;

/// The application window: toolbars, status bar, the alarm banner and an
/// MDI area with four windows - LCD display, analog meter, graph (InstanceWidget)
/// and readings table - placed by an MdiArranger.
///
/// The meters of the process are in a DeviceSession: the window's and those
/// opened beside it (sidebar "Open devices", --also, SCPI); they record
/// together.
class MainWindow : public QMainWindow, private Ui::UIMainWindow
{
  Q_OBJECT
public:
  /// @param parser the processed command line (--debug, --config-dir, --config-id)
  /// @param parent parent widget, normally none
  MainWindow(QCommandLineParser &parser, QWidget *parent = nullptr);
  /// --debug: frame dump and the qtdmm.hid logging category.
  void      setConsoleLogging(bool);

protected Q_SLOTS:
  /// Recording state changed; enables/disables Start/Stop.
  void      runningSLOT(bool);
  /// Record and Start are there while a meter is connected (@p on).
  void      connectSLOT(bool);
  /// Start action: asks about a recording not saved and its length, then records.
  void      startSLOT();
  void      on_action_About_triggered();
  void      on_action_Help_triggered();
  /// Shows the popup menu (the window has no menu bar).
  void      on_action_Menu_triggered();
  /// Shows the front panels as the settings say.
  void      showDisplaySLOT(bool show);
  /// The front panels were shown/hidden: the settings keep it.
  void      storeDisplaySLOT();
  /// Toolbar button style: icons only or icons with text.
  void      setUseTextLabel(bool on);
  /// Title = app name, instance id and the configured meter.
  void      updateWindowTitle();
  /// Record (Space): starts the recorder, or stops it when it is running.
  void      toggleRecordingSLOT();
  /// Record and Live follow the recorder: grey dot or red square, Live
  /// pressed while live and locked while recording.
  void      updateRecorderActions();
  /// F11
  void      setFullScreen(bool on);
  /// Ctrl+M: all toolbars at once
  void      setToolbarsHidden(bool hide);
  void      toolbarHiddenHint();   ///< after hiding the toolbar by hand: how it comes back (until switched off)
  /// Arrange actions and the title-bar action follow the arranger.
  void      syncArrangeActions();
  /// The arrow of the toolbar button at the end of the status line.
  void      updateToolbarButton();

protected:
  /// @name Parts of the constructor, in this order
  /// @{
  void      createViews();
  void      createSidebar();
  void      fillViewToolbar();
  QList<QAction *> createArrangeMenu();
  void      createDesignMenu();
  void      createStatusBar();
  void      placeWindow();
  /// @}

  DeviceSession     *m_session;   ///< the meters of this process and their network services
  InstanceWidget    *m_wid;
  /// @name The front panels of the meters connected
  /// One MDI window each (role Instrument), by MeterController::source().
  /// Its X hides it, the meter stays connected. Display (Ctrl+1) shows and
  /// hides them all.
  /// @{
  QMap<QString, QMdiSubWindow *> m_panelWins;
  /// What a workspace said of a panel, by the entry of My devices, until
  /// its meter is open.
  struct PanelState
  {
    ValueRef value;
    bool     folded = false;
    std::optional<QList<int>> secondaries;   ///< the LCD's small line, unset: not chosen
    bool     shown = true;
    QString  geometry;   ///< "x y w h" in Free mode
    std::optional<int> lcdVariant;   ///< its own LCD colours, unset: the settings'
  };
  QMap<QString, PanelState> m_panelStates;
  /// The entries a workspace had connected, to open (openPending()).
  QStringList m_pendingOpen;
  /// The entry of My devices that was the window's meter before 26.4
  /// (InstanceWidget::takeOldDevice(), WorkspaceFile::windowDevice): the
  /// next applyWorkspace() connects it first, values without a source are its.
  QString     m_oldDevice;
  /// The layout tree of the workspace, set again once its panels are there.
  QString     m_pendingLayout;
  /// A front panel for @p ctl, in its state of the workspace.
  void        addPanel(MeterController *ctl);
  void        removePanel(const QString &source);
  /// The workspace's @p state of a panel on its window @p win.
  void        applyPanelState(QMdiSubWindow *win, const PanelState &state);
  /// The LCD's look on @p panel: its colours (its own or the Appearance
  /// page's), the tint, its meter's display.
  void        applyLcdLook(FrontPanel *panel);
  /// The LCD colours of the panel of @p source, unset: the settings' again.
  void        setPanelLcdVariant(const QString &source, std::optional<int> variant);
  /// The LCD colours chosen in a panel's context menu (LcdWidget::LcdVariant),
  /// by source; gone with its meter closed.
  QHash<QString, int> m_panelLcd;
  /// Opens the entries of m_pendingOpen.
  void        openPending();
  /// Before device @p id is closed: true when its recording is saved, or
  /// the user discards it after @p text.
  bool        keepRecordingOf(const QString &id, const QString &text);
  /// The front panel windows.
  QList<QMdiSubWindow *> panelWindows() const { return m_panelWins.values(); }
  /// Whether @p win shows an instrument: a front panel or the analog meter.
  bool        isInstrument(const QMdiSubWindow *win) const;
  /// @name The active device: the meter of the front panel used last
  /// Connect, Reset min/max and Device settings act on it; the first meter
  /// connected while no panel was used, none without a meter.
  /// @{
  QPointer<MeterController> m_active;   ///< null: the first meter
  QLabel     *m_activeName = nullptr;   ///< "Active: <name>" in the status bar, with more than one meter
  QMetaObject::Connection m_limitCountLink;   ///< the active device's limit test count
  std::function<void(const LimitTest::Count &)> m_showLimitCount;   ///< into the status bar
  MeterController *activeController() const;
  /// Makes @p ctl the active device; Connect shows its state.
  void        setActive(MeterController *ctl);
  /// @}
  /// The panel in @p win, nullptr for another view.
  static FrontPanel *panelOf(const QMdiSubWindow *win);
  /// The window's background behind @p view: the design's frame (metal) or the palette's.
  void        applyFrame(QWidget *view);
  /// @}
  AnalogMeter   *m_meter;
  ReadingsWidget *m_readings;
  PoincarePlot *m_poincare;
  AlarmBar   *m_alarmBar;
  class AlarmService *m_alarmService;
  QMdiArea   *m_mdi;
  MdiArranger *m_arranger;
  QToolButton *m_toolbarButton = nullptr;   ///< toolbar on/off, at the end of the status line
  QMdiSubWindow *m_meterWin;
  QMdiSubWindow *m_graphWin;
  QMdiSubWindow *m_readingsWin;
  QMdiSubWindow *m_poincareWin;
  QAction    *m_displayAction = nullptr;
  QAction    *m_meterAction = nullptr;
  QAction    *m_readingsAction = nullptr;
  QAction    *m_poincareAction = nullptr;
  QAction    *m_addDeviceAction = nullptr;
  QDockWidget   *m_sidebarDock = nullptr;
  QElapsedTimer  m_designApplied;          ///< since setDesign(): its own palette change is no switch
  bool           m_designPending = false;  ///< the desktop switched: setDesign() on its way
  QAction       *m_sidebarAction = nullptr; ///< shows and hides the sidebar
  DeviceSidebar *m_sidebar = nullptr;     ///< "My devices" at the left
  /// "Settings..." of an entry: its meter's settings; a device connected
  /// takes them at once (connected anew), unchanged ones change nothing.
  /// Info and settings of the entry @p id, on its Info tab or the Settings tab.
  void        deviceSettings(const QString &id, bool settingsTab = false);
  /// The Info tab's state of the entry @p id: in use where, connected,
  /// the last reading, the values; rich text.
  QString     deviceStatus(const QString &id) const;
  /// Connects the entry @p id (DeviceSession::open()), unless its port is in use.
  void        openBeside(const QString &id);
  void        showSidebar(bool show);
  /// The node "Open devices" anew: the meters of the session with their
  /// readings (each second while the sidebar is shown).
  void        updateOpenDevices();
  /// The node "Services": the SCPI server and HiSLIP of this window.
  void        updateServices();
  QTimer     *m_openTimer = nullptr;
  /// The places (DeviceLibrary::place()) the open devices use, with the
  /// meter that uses each.
  QMap<QString, const MeterController *> placeOwners() const;
  /// The places (DeviceLibrary::place()) the open devices use, with
  /// "Connected: x" for the assistant's cards.
  QMap<QString, QString> placesInUse() const;
  /// The assistant "Add device": the device into My devices, connected
  /// unless "Save only".
  void        addDevice();
  /// The empty start: without a meter connected (or about to be) the window
  /// shows the big button "Add device", and "Connect from My devices" when
  /// it has entries, over the area instead of the views.
  void        updateEmptyStart();
  QWidget    *m_emptyStart = nullptr;   ///< covers the area, with the buttons
  QPushButton *m_emptyConnect = nullptr;   ///< "Connect a device from My devices"
  QAction    *m_arrangeTop;
  QAction    *m_arrangeLeft;
  QAction    *m_arrangeFixed;
  QAction    *m_arrangeFree;
  QAction    *m_titleBars;
  QAction    *m_autoSaveLayout;   ///< Windows/auto-save: the layout is saved on exit
  bool        m_startDisplay = true;   ///< Display/show of the start layout (auto-save off)
  QMenu      *m_arrangeMenu;
  QMenu      *m_recentMenu = nullptr;   ///< Arrange > Recent workspaces
  QMenu      *m_designMenu;
  bool        m_restoring = false;   ///< restoreWindows() is setting the actions
  QStringList m_openingPending;   ///< openPending(): the meters still to come, a view's among them waits
  /// The panel windows make room for the keys (FrontPanel::keysHeight()).
  void        updateControls();
  // Window size (package 26.2, 4a): the window starts at the size its
  // content needs and grows when a view is shown for the first time, up to
  // 84 % of the screen per side. It never shrinks by itself and stops
  // growing for good once the user sized it. Views shown while it is
  // maximized or full screen get their room once when it comes back.
  bool        m_userSized = false;   ///< persisted as Windows/user-sized
  bool        m_growEnabled = false; ///< off until the window is on screen
  QSize       m_expectSize;          ///< the size we asked for last
  QSize       m_pendingGrow{0, 0};   ///< growth asked for while maximized
  bool        m_wasFull = false;     ///< maximized or full screen before the last state change
  bool        m_unmaximizing = false;   ///< back from maximized, the size settling
  QSet<QObject *> m_grown;           ///< views the window already grew for
  int         em() const;            ///< a font height, the unit of all sizes
  /// Width and height a view adds to the window (0 = none in that direction).
  QSize       growthFor(QMdiSubWindow *win) const;
  /// Grows the window by @p delta within the limits, moving it up/left when
  /// the frame would stick out of the screen.
  void        autoGrow(const QSize &delta);
  /// A view was shown: grow for it once per session.
  void        growFor(QMdiSubWindow *win);
  QAction    *m_fullScreen;
  QAction    *m_toolbars;
  QAction    *m_zoomIn;
  QAction    *m_zoomOut;
  QAction    *m_zoomFit;
  QAction    *m_copyImage;
  ShortcutRegistry *m_shortcuts = nullptr;
  bool        m_running;
  QLabel     *m_error;
  QLabel     *m_info;
  QLabel     *m_limitCount;   ///< PASS/FAIL count, while the limit test is on
  QMenu      *m_menu;
  HelpDlg    *m_helpDlg;
  QString     m_configId;

  void        setupIcons();
  void        createActions();
  /// Menu-only actions and their shortcuts (the window has no menu bar).
  void        createExtraActions();
  /// The commands with a key for the page Keyboard (ShortcutRegistry);
  /// the user's keys applied, the tooltips name them: "Record (Space)".
  /// @p workspace: the menu's Load/Save workspace.
  void        createShortcuts(const QList<QAction *> &workspace);
  /// Adds @p view as an MDI window with @p title (@p role: MdiArranger::Role;
  /// @p name identifies it in the settings).
  QMdiSubWindow *addView(QWidget *view, const QString &title, int role, const QString &name);
  /// The context menu of the sidebar's row of @p value (ValueMenu).
  void        valueMenu(const ValueRef &value, const QPoint &globalPos);
  /// @name The readings table's meters
  /// The meters in it, the window's like the others (m_tableMeters; none
  /// chosen: all connected). More than the window's: the table logs the
  /// readings of all of them in its own store, in the order they come, with
  /// the Device column; the window's alone: its recorder's readings.
  /// @{
  QStringList m_tableMeters;   ///< sources ("device:<id>"); empty: all connected
  /// The meters the table shows now: m_tableMeters, or all connected.
  QStringList tableSources() const;
  QList<QMetaObject::Connection> m_tableLinks;
  /// The table anew for tableSources(): its rows from before, merged by time.
  void        applyTableSources();
  /// The readings of the meters in the table to it (the open ones of them).
  void        connectTableSources();
  /// The table shows @p value (its meter is in it, the value not hidden).
  bool        tableShows(const ValueRef &value) const;
  /// Shows or hides @p value in the table; another meter's first value
  /// shown brings it in (its other values hidden), its last one hidden takes it out.
  void        setTableShown(const ValueRef &value, bool shown);
  /// "Sources..." of a view: the values of the open meters it shows.
  void        editSources(ValueMenu::View view);
  QAction    *m_graphSources = nullptr;   ///< in the graph's menu, with more than one meter
  QAction    *m_tableSources = nullptr;   ///< in the table's menu, the same
  /// A meter's name by its source, also after it was closed.
  QString     sourceName(const QString &source) const;
  /// @}
  /// The sidebar's row of value @p id of a meter open beside.
  DeviceSidebar::Value otherValueRow(const MeterController *ctl, int id) const;
  /// Fills @p menu for @p value of an open meter (ValueMenu).
  void        fillValueMenu(QMenu *menu, const ValueRef &value);
  /// A checkable action that shows/hides @p win.
  QAction    *windowAction(QMdiSubWindow *win, const QString &text, const char *shortcut,
                           const char *icon, const QString &whatsThis);
  void        bindWindowAction(QAction *action, QMdiSubWindow *win);
  /// The window's context menu (from its header line).
  void        windowMenu(QMdiSubWindow *win, const QPoint &globalPos);
  /// The limit test's dialog for the meter @p ctl; @p enable ticks "Check
  /// the limits" in it (switched on from the menu without limits yet),
  /// @p id >= 0 sets the value it judges.
  void        editLimits(MeterController *ctl, bool enable, int id = -1);
  /// The rules of the meter @p ctl in @p menu: PASS/FAIL test, limits, alarms.
  void        addRulesMenu(QMenu *menu, MeterController *ctl);
  /// A new alarm on @p value, edited in AlarmDlg and given to its meter.
  void        addAlarm(const ValueRef &value);
  /// Window layout from/to the settings (Windows/... keys).
  void        restoreWindows();
  void        saveWindows();
  /// @name Workspace: the window layout, from the settings or a file
  /// @{
  /// Reads a key; the type of @p def says bool or string.
  using WorkspaceGet = std::function<QVariant(const QString &key, const QVariant &def)>;
  using WorkspaceSet = std::function<void(const QString &key, const QVariant &value)>;
  /// Shows the windows, mode, tree, title bars, design and (Free) positions.
  void        applyWorkspace(const WorkspaceGet &get);
  void        storeWorkspace(const WorkspaceSet &set);
  void        loadWorkspace();
  /// Loads the workspace @p file; false (with a message) when it is none.
  bool        loadWorkspaceFile(const QString &file);
  void        saveWorkspace();
  /// The workspaces used last, shared by all instances (workspaces.conf).
  RecentFiles recentWorkspaces() const;
  /// The sidebar's node and the menu "Recent workspaces" anew.
  void        updateRecentWorkspaces();
  /// The workspace in use: loaded or saved last (Windows/workspace-current).
  void        setCurrentWorkspace(const QString &file);
  /// For a tooltip: name, description, picture and path of @p file.
  static QString workspaceTooltip(const QString &file, const WorkspaceFile &ws);
  QString     m_workspaceFile;          ///< setCurrentWorkspace()
  QString     m_workspaceName;
  QString     m_workspaceDescription;
  /// @}
  /// Applies a colour design (Designs::Design) to the window and the views.
  void        setDesign(int design);
  /// Keeps the window actions checked when a window is closed or shown.
  bool        eventFilter(QObject *watched, QEvent *event) override;
  /// Saves window state; vetoed by InstanceWidget::closeWin() on unsaved data.
  void        closeEvent(QCloseEvent *)Q_DECL_OVERRIDE;
  /// No context menu to hide the toolbars: they are always there.
  QMenu      *createPopupMenu() override;
  /// A size we did not ask for came from the user (or the window manager
  /// on their behalf, like snapping to a screen half): no more growing.
  void        resizeEvent(QResizeEvent *) override;
  void        changeEvent(QEvent *) override;
  void        showEvent(QShowEvent *) override;
  /// Raises this window when another instance asks for it ("RAISE_<id>").
  void        bringMainWindowToFront();
};

