//======================================================================
// File:		mainwin.cpp
// Author:	Matthias Toussaint
// Created:	Sun Sep  2 12:15:28 CEST 2001
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

#include <QtGui>
#include <QtWidgets>
#include <QTimer>
#include <QMenu>

#include "ui/mainwindow.h"
#include "ui/dialogs/helpdlg.h"
#include "ui/instancewidget.h"
#include "ui/views/graphwidget.h"
#include "ui/views/lcdwidget.h"
#include "ui/views/analogmeter.h"
#include "ui/views/readingswidget.h"
#include "core/settings.h"
#include "ui/alarmbar.h"
#include "ui/mdiarranger.h"
#include "service/metercontroller.h"
#include "ui/designs.h"
#include "ui/controlbar.h"
#include <QMdiArea>
#include <QMdiSubWindow>
#include <QLoggingCategory>

MainWindow::MainWindow(QCommandLineParser &parser, QWidget *parent)
  : QMainWindow(parent)
  , m_running(false)
  , m_menu(Q_NULLPTR)
  , m_helpDlg(Q_NULLPTR)
  , m_localRecord(true)
{
  setupUi(this);
  setupIcons();
  m_config_id = parser.value("config-id");

  m_stateMgr = new SharedStateManager(m_config_id.isEmpty()?"default":m_config_id,this);

  QWidget* spacer = new QWidget();
  spacer->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
  this->toolBarMenu->addWidget(spacer);
  this->toolBarMenu->addAction(this->action_Menu);
  m_wid = new InstanceWidget(m_config_id, parser.value("config-dir"), this);
  m_wid->setFrameShape(QFrame::NoFrame);
  setConsoleLogging(parser.isSet("debug"));

  createActions();

  // the window: alarm banner on top, the MDI area with the four views below
  auto *central = new QWidget(this);
  auto *centralLayout = new QVBoxLayout(central);
  centralLayout->setContentsMargins(0, 0, 0, 0);
  centralLayout->setSpacing(0);
  m_alarmBar = new AlarmBar(central);
  centralLayout->addWidget(m_alarmBar);
  m_mdi = new QMdiArea(central);
  m_mdi->setHorizontalScrollBarPolicy(Qt::ScrollBarAsNeeded);
  m_mdi->setVerticalScrollBarPolicy(Qt::ScrollBarAsNeeded);
  centralLayout->addWidget(m_mdi, 1);
  setCentralWidget(central);
  m_arranger = new MdiArranger(m_mdi, this);

  MeterController *ctl = m_wid->controller();
  connect(ctl, &MeterController::alarmBannerChanged, m_alarmBar, &AlarmBar::setAlarms);
  connect(m_alarmBar, &AlarmBar::acknowledged, ctl, &MeterController::acknowledgeAlarms);

  // the four views; none of them knows another, they all hang off the
  // controller (see InstanceWidget)
  m_display = new LcdWidget(this);
  m_wid->setDisplay(m_display);
  // the display with the meter's keys below it (only for meters that have
  // them) and the fold button in its corner
  auto *displayBox = new QWidget(this);
  auto *displayLayout = new QVBoxLayout(displayBox);
  displayLayout->setContentsMargins(0, 0, 0, 0);
  displayLayout->setSpacing(4);
  displayLayout->addWidget(m_display, 1);
  m_controls = new ControlBar(displayBox);
  displayLayout->addWidget(m_controls);
  m_fold = new FoldButton(m_display);
  m_fold->setFolded(m_wid->settings()->getBool("Display/controls-hidden", false));
  m_display->installEventFilter(this);   // keeps the fold button in the corner
  connect(m_fold, &FoldButton::toggled, this, [this](bool folded) { setControlsFolded(folded); });
  connect(m_wid->controller(), &MeterController::reading, m_controls, &ControlBar::showReading);
  connect(m_controls, &ControlBar::keyPressed, this, [this](const QString &key)
  {
    statusBar()->showMessage(tr("The meter's %1 key: remote control is not built in yet.").arg(key.toUpper()), 4000);
  });
  connect(m_wid, &InstanceWidget::remoteControl, this, [this](bool supported)
  {
    m_controlsSupported = supported;
    updateControls();
  });
  m_displayWin = addView(displayBox, tr("Display"), MdiArranger::Instrument, "display");

  m_meter = new AnalogMeter(this);
  m_wid->setMeter(m_meter);
  m_meterWin = addView(m_meter, tr("Analog meter"), MdiArranger::Instrument, "meter");
  // no cell bigger than the instruments can fill
  m_arranger->setContentAspect(m_displayWin, LcdWidget::kMinAspect, LcdWidget::kMaxAspect);
  m_arranger->setContentAspect(m_meterWin, AnalogMeter::kMinAspect, AnalogMeter::kMaxAspect);
  m_wid->setStateManager(m_stateMgr);

  m_graphWin = addView(m_wid, tr("Graph"), MdiArranger::Graph, "graph");

  m_readings = new ReadingsWidget(this);
  m_wid->setReadingLog(m_readings->log());   // the row limit is the store's: set it after
  m_readings->setMaxRows(m_wid->settings()->getInt("ReadingLog/max-rows", 10000));
  m_readingsWin = addView(m_readings, tr("Readings"), MdiArranger::Table, "readings");

  // right-click on display or meter: the window's menu (also without title bar)
  for (auto [view, win] : { std::pair<QWidget *, QMdiSubWindow *>{m_display, m_displayWin},
                            std::pair<QWidget *, QMdiSubWindow *>{m_meter, m_meterWin} })
  {
    view->setContextMenuPolicy(Qt::CustomContextMenu);
    connect(view, &QWidget::customContextMenuRequested, this, [this, view = view, w = win](const QPoint &pos)
    {
      windowMenu(w, view->mapToGlobal(pos));
    });
  }

  // one checkable action per window: toolbar buttons, menu entries, shortcuts
  m_displayAction = windowAction(m_displayWin, tr("&Display"), "Ctrl+1", "qtdmm-display",
    tr("<html><head/><body><p><span style=\" font-weight:600;\">Display</span></p>"
       "<p>Show the reading on the LCD-style digital display.</p></body></html>"));
  m_meterAction = windowAction(m_meterWin, tr("Analog &meter"), "Ctrl+2", "qtdmm-meter",
    tr("<html><head/><body><p><span style=\" font-weight:600;\">Analog meter</span></p>"
       "<p>Show the reading on a moving-coil style instrument.</p></body></html>"));
  m_readingsAction = windowAction(m_readingsWin, tr("&Readings table"), "Ctrl+4", "table",
    tr("<html><head/><body><p><span style=\" font-weight:600;\">Readings table</span></p>"
       "<p>Every reading the meter sent, one row each, with time, mode and range - "
       "the raw protocol of the session next to the recorder's graph. Copy rows to a "
       "spreadsheet or export them as CSV.</p></body></html>"));
  // the graph keeps its action from the .ui (toolbar button with Ctrl+G)
  action_Graph->setShortcuts({QKeySequence("Ctrl+G"), QKeySequence("Ctrl+3")});
  bindWindowAction(action_Graph, m_graphWin);

  toolBarDMM->addSeparator();
  toolBarDMM->addAction(m_displayAction);
  toolBarDMM->addAction(m_meterAction);
  toolBarDMM->addAction(m_readingsAction);
  connect(m_displayAction, &QAction::toggled, this, &MainWindow::setToolbarVisibilitySLOT);

  // arrangement: automatic (displays on top or on the left), fixed or free,
  // title bars on/off
  auto *arrangeGroup = new QActionGroup(this);
  auto arrangeAction = [this, arrangeGroup](const QString &text, MdiArranger::Mode mode, const QString &whatsThis)
  {
    auto *a = new QAction(text, arrangeGroup);
    a->setCheckable(true);
    a->setWhatsThis(whatsThis);
    connect(a, &QAction::triggered, this, [this, mode] { m_arranger->setMode(mode); });
    return a;
  };
  const QString arrangeHint = tr("<p>Drag a divider between two windows to share the space differently. "
                                 "Ctrl+drag a window (or drag its title bar) onto another one to swap them.</p>");
  m_arrangeTop = arrangeAction(tr("Displays on &top"), MdiArranger::DisplaysOnTop,
                               tr("<html><head/><body><p><span style=\" font-weight:600;\">Displays on top</span></p>"
                                  "<p>The displays share a strip at the top, the graph takes the rest and the "
                                  "readings table a column on the right; everything follows the window size."
                                  "</p>%1</body></html>").arg(arrangeHint));
  m_arrangeLeft = arrangeAction(tr("Displays on the &left"), MdiArranger::DisplaysOnLeft,
                                tr("<html><head/><body><p><span style=\" font-weight:600;\">Displays on the left</span></p>"
                                   "<p>The displays share a column on the left, the graph and the readings table "
                                   "take the rest; everything follows the window size.</p>%1</body></html>")
                                  .arg(arrangeHint));
  m_arrangeFixed = arrangeAction(tr("F&ixed"), MdiArranger::Fixed,
                                 tr("<html><head/><body><p><span style=\" font-weight:600;\">Fixed</span></p>"
                                    "<p>Keeps the layout as it is: from Displays on top or on the left as they "
                                    "are, from Free the windows snap into a grid made from their positions. "
                                    "A window shown later gets a place at the edge.</p>%1</body></html>")
                                   .arg(arrangeHint));
  m_arrangeFree = arrangeAction(tr("&Free"), MdiArranger::Free,
                                tr("<html><head/><body><p><span style=\" font-weight:600;\">Free</span></p>"
                                   "<p>Place and size the windows as you like; Ctrl+drag moves a window, "
                                   "also one without title bar.</p></body></html>"));

  m_titleBars = new QAction(tr("&Hide title bars"), this);
  m_titleBars->setCheckable(true);
  m_titleBars->setShortcut(QKeySequence("Ctrl+L"));
  m_titleBars->setWhatsThis(tr("<html><head/><body><p><span style=\" font-weight:600;\">Hide title bars</span></p>"
                               "<p>Windows without title bar sit flush next to each other. Right-click the display "
                               "or the meter for the window's menu; Ctrl+drag moves a window, in the arranged modes onto "
                               "another one to swap them."
                               "</p></body></html>"));
  connect(m_titleBars, &QAction::triggered, m_arranger, &MdiArranger::setTitleBarsHidden);
  connect(m_arranger, &MdiArranger::changed, this, &MainWindow::syncArrangeActions);

  m_arrangeMenu = new QMenu(tr("&Arrange"), this);
  m_arrangeMenu->addAction(m_arrangeTop);
  m_arrangeMenu->addAction(m_arrangeLeft);
  m_arrangeMenu->addAction(m_arrangeFixed);
  m_arrangeMenu->addAction(m_arrangeFree);
  m_arrangeMenu->addSeparator();
  m_arrangeMenu->addAction(m_titleBars);
  m_arrangeMenu->addSeparator();
  QAction *loadWs = m_arrangeMenu->addAction(QIcon::fromTheme("document-open"), tr("L&oad workspace..."));
  loadWs->setWhatsThis(tr("<html><head/><body><p><span style=\" font-weight:600;\">Load workspace</span></p>"
                          "<p>Takes a window layout saved with <i>Save workspace</i>: which windows are shown, "
                          "the arrangement, title bars, design and the size of the main window.</p>"
                          "</body></html>"));
  connect(loadWs, &QAction::triggered, this, &MainWindow::loadWorkspace);
  QAction *saveWs = m_arrangeMenu->addAction(QIcon::fromTheme("document-save-as"), tr("&Save workspace..."));
  saveWs->setWhatsThis(tr("<html><head/><body><p><span style=\" font-weight:600;\">Save workspace</span></p>"
                          "<p>Writes the window layout to a file: which windows are shown, the arrangement, "
                          "title bars, design and the size of the main window.</p></body></html>"));
  connect(saveWs, &QAction::triggered, this, &MainWindow::saveWorkspace);
  m_autoSaveLayout = m_arrangeMenu->addAction(tr("Save layout on e&xit"));
  m_autoSaveLayout->setCheckable(true);
  m_autoSaveLayout->setChecked(true);
  m_autoSaveLayout->setWhatsThis(tr("<html><head/><body><p><span style=\" font-weight:600;\">Save layout on exit</span></p>"
                                    "<p>On: QtDMM starts with the window layout it had when it was closed. Off: it "
                                    "starts with the layout last saved while this was on, or the workspace last "
                                    "loaded.</p></body></html>"));
  QAction *arrangeButton = m_arrangeMenu->menuAction();
  arrangeButton->setIcon(QIcon::fromTheme("qtdmm-arrange"));
  arrangeButton->setToolTip(tr("Arrange the windows"));
  // on the right, next to the menu button: it arranges the whole window,
  // not the meter
  toolBarMenu->insertAction(action_Menu, arrangeButton);
  if (auto *button = qobject_cast<QToolButton *>(toolBarMenu->widgetForAction(arrangeButton)))
    button->setPopupMode(QToolButton::InstantPopup);

  // colour designs of the window (the LCD tint and the meter style are
  // settings of their own)
  m_designMenu = new QMenu(tr("D&esign"), this);
  auto *designGroup = new QActionGroup(this);
  for (auto [d, text] : { std::pair{Designs::System, tr("&System")}, std::pair{Designs::Silver, tr("S&ilver")},
                          std::pair{Designs::Dark, tr("&Dark")} })
  {
    QAction *a = m_designMenu->addAction(text);
    a->setCheckable(true);
    a->setData(int(d));
    designGroup->addAction(a);
    connect(a, &QAction::triggered, this, [this, d = d] { setDesign(d); });
  }

  updateWindowTitle();
  connect(m_wid, &InstanceWidget::configChanged, this, &MainWindow::updateWindowTitle);
  connect(m_wid, &InstanceWidget::configChanged, this, &MainWindow::updateLed);
  // the design can be chosen in the settings (Appearance), too
  connect(m_wid, &InstanceWidget::configChanged, this, [this]
  {
    const Designs::Design d = Designs::fromName(m_wid->settings()->getString("Windows/design", "dark"));
    if (d != Designs::current())
      setDesign(d);
  });

  createExtraActions();
  addShortcutsToToolTips();

  connect(m_wid, SIGNAL(running(bool)), this, SLOT(runningSLOT(bool)));

  connectSLOT(false);

  // status bar
  // a dot that blinks green while readings come in and turns grey when they
  // stop; the connection text follows it
  m_led = new QLabel(statusBar());
  statusBar()->addWidget(m_led);
  m_ledIdle = new QTimer(this);
  m_ledIdle->setSingleShot(true);
  m_ledIdle->setInterval(3000);
  connect(m_ledIdle, &QTimer::timeout, this, [this]
  {
    m_ledActive = false;
    updateLed();
  });
  connect(m_wid->controller(), &MeterController::reading, this, [this](const Reading &r)
  {
    if (r.id != 0)
      return;
    m_ledActive = true;
    m_ledBlink = !m_ledBlink;
    m_ledIdle->start();
    updateLed();
  });
  updateLed();

  m_error = new QLabel(statusBar());
  m_error->setFrameStyle(QFrame::Panel | QFrame::Sunken);
  statusBar()->addWidget(m_error, 20);
  m_error->setLineWidth(1);

  m_info = new QLabel(statusBar());
  m_info->setFrameStyle(QFrame::Panel | QFrame::Sunken);
  statusBar()->addWidget(m_info, 10);
  m_info->setLineWidth(1);

  m_scpi = new QLabel(statusBar());
  m_scpi->setFrameStyle(QFrame::Panel | QFrame::Sunken);
  m_scpi->setLineWidth(1);
  m_scpi->setToolTip(tr("The SCPI server: other programs can read the meter here (Settings, SCPI server)."));
  statusBar()->addPermanentWidget(m_scpi);
  m_scpi->hide();
  connect(m_wid, &InstanceWidget::scpiStatus, this, [this](const QString &text)
  {
    m_scpi->setText(text);
    m_scpi->setVisible(!text.isEmpty());
  });

  // messages such as the permission hint span several lines; the status bar
  // shows the first one and keeps the rest in the tooltip
  connect(m_wid, &InstanceWidget::error, this, [this](const QString &text)
  {
    const QString firstLine = text.section('\n', 0, 0);
    m_error->setText(firstLine);
    m_error->setToolTip(text.contains('\n') ? text : QString());
  });
  connect(m_wid, SIGNAL(info(const QString &)), m_info, SLOT(setText(const QString &)));
  connect(m_wid, SIGNAL(useTextLabel(bool)), this, SLOT(setUseTextLabel(bool)));
  connect(m_wid, &InstanceWidget::iconSet, this, [](const QString &set)
          { Designs::setIconSet(Designs::iconSetFromName(set)); });
  connect(m_wid, SIGNAL(setConnect(bool)), this, SLOT(setConnectSLOT(bool)));
  connect(m_wid, SIGNAL(connectDMM(bool)), action_Connect, SLOT(setChecked(bool)));
  connect(m_wid, SIGNAL(toolbarVisibility(bool, bool, bool, bool)),
          this, SLOT(toolbarVisibilitySLOT(bool, bool, bool, bool)));

  QRect winRect = m_wid->winRect();

  m_wid->applySLOT();
  // the layout of the windows; the dock state of versions before the MDI
  // window (MainWindow/state) is not taken over
  restoreWindows();

  setMinimumSize(19 * em(), 13 * em());
  // winRect() falls back to 500 x 350: only a stored width means a stored
  // geometry; a first start lets the window manager place the window
  const bool stored = m_wid->settings()->getInt("Position/width", 0) > 0;
  if (stored && m_wid->saveWindowPosition())
    move(winRect.x(), winRect.y());
  // the views shown at the start count as grown for; the table column gets
  // the width the window has for it
  for (QMdiSubWindow *w : { m_displayWin, m_meterWin, m_graphWin, m_readingsWin })
    if (!w->isHidden())
      m_grown.insert(w);
  if (!m_controls->isHidden())
    m_grown.insert(m_controls);
  if (m_grown.contains(m_readingsWin))
    m_arranger->setTableWidth(growthFor(m_readingsWin).width());
  if (stored && m_wid->saveWindowSize())
  {
    resize(winRect.width(), winRect.height());
    m_userSized = m_wid->settings()->getBool("Windows/user-sized", false);
  }
  else
  {
    // from the content: an LCD, plus what each shown view adds
    QSize start(28 * em(), 22 * em());
    if (m_grown.contains(m_displayWin) && m_grown.contains(m_meterWin))
      start += growthFor(m_meterWin);
    for (QMdiSubWindow *w : { m_graphWin, m_readingsWin })
      if (m_grown.contains(w))
        start += growthFor(w);
    if (!m_controls->isHidden())
      start += QSize(0, 2 * em());
    const QRect av = screen()->availableGeometry();
    resize(qMin(start.width(), int(av.width() * 0.84)), qMin(start.height(), int(av.height() * 0.84)));
  }
  m_expectSize = size();

  connect(m_stateMgr, &SharedStateManager::stateChanged, this, [=](const QString& state){
    if (state == "RECORD")
    {
      QMetaObject::invokeMethod(m_wid, "startSLOT", Qt::DirectConnection);
      m_localRecord = false;
    }
    else if (state == "STOP")
    {
      if (!m_localRecord)
        action_Stop->trigger();
    }
    else if (state == "RAISE_"+(m_config_id.isEmpty()?"default":m_config_id))
    {
      bringMainWindowToFront();
      m_stateMgr->writeState("IDLE");
    }
  });

  connect(m_stateMgr, &SharedStateManager::instanceIdAlreadyInUse, this, [=](){
    QMessageBox::critical(this, APP_NAME,tr("Another instance is running."));
    qApp->quit();
  });

  // auto-connect at start, but not before a meter was ever chosen: a fresh
  // instance would otherwise try the first serial port it finds
  if (m_stateMgr->registerInstance() && m_wid->dmmConfigured())
    QTimer::singleShot(1000, action_Connect, &QAction::trigger);
}

// "QtDMM: UNI-T UT61E", with the instance id for non-default instances
void MainWindow::updateWindowTitle()
{
  QString title = APP_NAME;
  if (!m_config_id.isEmpty())
    title += QString(" [%1]").arg(m_config_id);
  setWindowTitle(QString("%1: %2").arg(title, m_wid->dmmTitle()));
}

void MainWindow::sendStateSLOT(const QString & state)
{
  m_stateMgr->writeState(state);
}


void MainWindow::setConsoleLogging(bool on)
{
  if (on)
    QLoggingCategory::setFilterRules("qtdmm.hid.debug=true\nqtdmm.ble.debug=true\nqtdmm.mdns.debug=true\nqtdmm.scpi.debug=true");
  m_wid->setConsoleLogging(on);
}

void MainWindow::setUseTextLabel(bool on)
{
  Qt::ToolButtonStyle Style = Qt::ToolButtonTextUnderIcon;
  if (!on)
    Style = Qt::ToolButtonIconOnly;
  toolBarDMM->setToolButtonStyle(Style);
  toolBarRecorder->setToolButtonStyle(Style);
  toolBarFile->setToolButtonStyle(Style);
  toolBarMenu->setToolButtonStyle(Style);
}

void MainWindow::createActions()
{
  connect(action_Connect, SIGNAL(triggered(bool)), m_wid, SLOT(connectSLOT(bool)));
  connect(action_Connect, SIGNAL(triggered(bool)), this, SLOT(connectSLOT(bool)));
  connect(action_Reset, SIGNAL(triggered()), m_wid, SLOT(resetSLOT()));
  connect(action_Start, SIGNAL(triggered()), this, SLOT(startSLOT()));
  connect(action_Stop, SIGNAL(triggered()), m_wid, SLOT(stopSLOT()));
  connect(action_Stop, SIGNAL(triggered()), this, SLOT(stopSLOT()));
  connect(action_Clear, SIGNAL(triggered()), m_wid, SLOT(clearSLOT()));
  connect(action_Print, SIGNAL(triggered()), m_wid, SLOT(printSLOT()));
  connect(action_Import, SIGNAL(triggered()), m_wid, SLOT(importSLOT()));
  connect(action_Export, SIGNAL(triggered()), m_wid, SLOT(exportSLOT()));
  connect(action_Configure, SIGNAL(triggered()), m_wid, SLOT(configSLOT()));
  connect(action_ConfigureDMM, SIGNAL(triggered()), m_wid, SLOT(configDmmSLOT()));
  connect(actionConfigureRecorder, SIGNAL(triggered()), m_wid, SLOT(configRecorderSLOT()));
  connect(action_Quit, SIGNAL(triggered()), this, SLOT(setToolbarVisibilitySLOT()));
  connect(action_Quit, SIGNAL(triggered()), m_wid, SLOT(quitSLOT()));
  connect(action_Direct_help, SIGNAL(triggered()), m_wid, SLOT(helpSLOT()));
  connect(action_Instances, SIGNAL(triggered()), m_wid, SLOT(instancesSLOT()));

  connect(toolBarMenu, SIGNAL(visibilityChanged(bool)),  this, SLOT(setToolbarVisibilitySLOT()));
  connect(toolBarFile, SIGNAL(visibilityChanged(bool)), this, SLOT(setToolbarVisibilitySLOT()));
  connect(toolBarRecorder, SIGNAL(visibilityChanged(bool)), this, SLOT(setToolbarVisibilitySLOT()));
  connect(toolBarDMM, SIGNAL(visibilityChanged(bool)), this, SLOT(setToolbarVisibilitySLOT()));

  connect(m_stateMgr, SIGNAL(instancesChanged(QStringList&)), m_wid, SLOT(instancesChangedSlot(QStringList&)));

}

// Actions that live only in the popup menu are not attached to any widget,
// so their shortcuts would be dead - adding them to the window fixes that.
void MainWindow::createExtraActions()
{
  // Ctrl+C is the historical Connect key; Ctrl+D is the one that does not
  // fight the copy reflex.
  action_Connect->setShortcuts({QKeySequence("Ctrl+C"), QKeySequence("Ctrl+D")});

  m_fullScreen = new QAction(tr("&Full screen"), this);
  m_fullScreen->setCheckable(true);
  m_fullScreen->setShortcut(QKeySequence("F11"));
  m_fullScreen->setWhatsThis(tr("<html><head/><body><p><span style=\" font-weight:600;\">Full screen</span></p>"
                                "<p>Use the whole screen for the instruments, e.g. on a lab monitor. F11 again "
                                "returns to the normal window.</p></body></html>"));
  connect(m_fullScreen, &QAction::toggled, this, &MainWindow::setFullScreen);

  m_zoomIn = new QAction(tr("Zoom &in"), this);
  m_zoomIn->setShortcuts({QKeySequence::ZoomIn, QKeySequence("Ctrl+=")});
  connect(m_zoomIn, &QAction::triggered, m_wid->graph(), &GraphWidget::zoomInSLOT);
  m_zoomOut = new QAction(tr("Zoom &out"), this);
  m_zoomOut->setShortcut(QKeySequence::ZoomOut);
  connect(m_zoomOut, &QAction::triggered, m_wid->graph(), &GraphWidget::zoomOutSLOT);
  m_zoomFit = new QAction(tr("Show &whole recording"), this);
  m_zoomFit->setShortcut(QKeySequence("Ctrl+0"));
  connect(m_zoomFit, &QAction::triggered, m_wid->graph(), &GraphWidget::zoomFitSLOT);
  m_copyImage = new QAction(tr("Cop&y graph image"), this);
  m_copyImage->setShortcut(QKeySequence("Ctrl+Shift+C"));
  m_copyImage->setWhatsThis(tr("<html><head/><body><p><span style=\" font-weight:600;\">Copy graph image</span></p>"
                               "<p>Puts a picture of the recorder graph on the clipboard, ready to paste into a "
                               "report or a chat.</p></body></html>"));
  connect(m_copyImage, &QAction::triggered, m_wid->graph(), &GraphWidget::copyImageSLOT);

  // Space toggles the recorder; a bare key, so only while this window is active
  QAction *toggleRecord = new QAction(this);
  toggleRecord->setShortcut(QKeySequence(Qt::Key_Space));
  connect(toggleRecord, &QAction::triggered, this, &MainWindow::toggleRecordingSLOT);

  addActions({action_Configure, action_Direct_help, action_Help, action_Quit,
              m_displayAction, m_meterAction, m_readingsAction, m_titleBars,
              m_fullScreen, m_zoomIn, m_zoomOut, m_zoomFit, m_copyImage, toggleRecord});
}

void MainWindow::addShortcutsToToolTips()
{
  for (QAction *a : findChildren<QAction *>())
  {
    if (a->shortcut().isEmpty() || a->isSeparator())
      continue;
    QString tip = a->toolTip();
    if (tip.isEmpty())
      tip = a->text().remove('&');
    a->setToolTip(QString("%1 (%2)").arg(tip, a->shortcut().toString(QKeySequence::NativeText)));
  }
}

void MainWindow::toggleRecordingSLOT()
{
  if (m_running)
    action_Stop->trigger();
  else if (action_Start->isEnabled())
    action_Start->trigger();
}

void MainWindow::setFullScreen(bool on)
{
  if (on)
    showFullScreen();
  else
    showNormal();
}

void MainWindow::startSLOT()
{
  if (m_stateMgr->instances().count()<=1)
  {
    QMetaObject::invokeMethod(m_wid, "startSLOT", Qt::DirectConnection);
    m_localRecord = true;
  }
  else
  {
    QMessageBox question(
      QMessageBox::Question,
      tr("Record DMM data"),
      tr("Multiple instances of QtDMM have been detected.\n"
         "Please choose which instance should record."),
      QMessageBox::Yes | QMessageBox::No | QMessageBox::Cancel);
    question.button(QMessageBox::Yes)->setText(tr("This instance"));
    question.button(QMessageBox::No)->setText(tr("All instances"));
    question.setEscapeButton(QMessageBox::Cancel);

    switch (question.exec())
    {
      case QMessageBox::Yes:
        QMetaObject::invokeMethod(m_wid, "startSLOT", Qt::DirectConnection);
        m_localRecord = true;
        return;
      case QMessageBox::No:
        m_stateMgr->writeState("RECORD");
        m_localRecord = false;
        return;
    }
  }
}

void MainWindow::stopSLOT()
{
  qInfo() << "stop" << m_localRecord;
  if (! m_localRecord)
    m_stateMgr->writeState("STOP");
  m_localRecord = false;

}

void MainWindow::runningSLOT(bool on)
{
  m_running = on;
  if (on)
    action_Graph->setChecked(true);   // a recording wants to be seen

  action_Start->setEnabled(!on);
  action_Stop->setEnabled(on);
  action_Print->setEnabled(!on);
  action_Export->setEnabled(!on);
  action_Import->setEnabled(!on);
}

void MainWindow::connectSLOT(bool on)
{
  action_Start->setEnabled(on);
  action_Stop->setEnabled(on && m_running);

  if (!on)
    m_running = false;
}

void MainWindow::on_action_Help_triggered()
{
  if (!m_helpDlg)
    m_helpDlg = new HelpDlg(m_wid->settings(), this);
  m_helpDlg->show();
  m_helpDlg->raise();
  m_helpDlg->activateWindow();
}

void MainWindow::on_action_About_triggered()
{
  QMessageBox about(this);
  about.setWindowTitle(tr("About QtDMM"));
  about.setIconPixmap(QPixmap(":/Symbols/qtdmm_64.png"));
  about.setTextFormat(Qt::RichText);
  about.setText(tr("<h2>QtDMM %1</h2>"
                   "<p>A readout and transient recorder for digital multimeters.</p>"
                   "<p>Built with <b>Qt</b> %2. Licensed under the <b>GNU GPL 3</b> "
                   "(versions before 0.9.0 under GPL 2).</p>"
                   "<p>0.9.5 onwards: tuxmaster and contributors, see the AUTHORS file.<br>"
                   "0.9.3 and before: &copy; 2001-2016 M. Toussaint "
                   "&lt;<a href='mailto:qtdmm@mtoussaint.de'>qtdmm@mtoussaint.de</a>&gt;</p>"
                   "<p>Website: <a href='https://qtdmm.de'>qtdmm.de</a> &middot; "
                   "Contact: <a href='mailto:hello@qtdmm.de'>hello@qtdmm.de</a><br>"
                   "Source and bug reports: <a href='https://github.com/qtdmm/QtDMM'>github.com/qtdmm/QtDMM</a><br>"
                   "Symbols from the <b>Breeze</b> icon theme of the KDE community (LGPL 3); "
                   "QtDMM's own symbols are drawn in its style.</p>")
                .arg(APP_VERSION).arg(qVersion()));

  // The device list used to be pasted in here as a table; it lives in the
  // handbook now, where it is readable, searchable on the web and generated
  // from the decoders instead of maintained by hand.
  QPushButton *devices = about.addButton(tr("Supported devices..."), QMessageBox::ActionRole);
  about.addButton(QMessageBox::Close);
  about.setDefaultButton(QMessageBox::Close);
  about.exec();

  if (about.clickedButton() == devices)
  {
    on_action_Help_triggered();
    m_helpDlg->showPage("supported-devices.md");
  }
}

void MainWindow::on_action_Menu_triggered()
{
  if (!m_menu)
  {
    m_menu = new QMenu(this);
    m_menu->addAction(action_Configure);
    m_menu->addAction(action_Graph);
    m_menu->addAction(m_displayAction);
    m_menu->addAction(m_meterAction);
    m_menu->addAction(m_readingsAction);
    m_menu->addMenu(m_arrangeMenu);
    m_menu->addMenu(m_designMenu);
    m_menu->addAction(m_fullScreen);
    m_menu->addSeparator();
    m_menu->addAction(m_zoomIn);
    m_menu->addAction(m_zoomOut);
    m_menu->addAction(m_zoomFit);
    m_menu->addAction(m_copyImage);
    m_menu->addSeparator();
    m_menu->addAction(action_Help);
    m_menu->addAction(action_Direct_help);
    m_menu->addAction(action_About);
    m_menu->addSeparator();
    m_menu->addAction(action_Quit);
  }

  QWidget* widget = this->toolBarMenu->widgetForAction(this->action_Menu);
  if (widget)
  {
    m_menu->popup(widget->mapToGlobal(
      QPoint( widget->width() - m_menu->sizeHint().width(), widget->height())
    ));
  }
}


void MainWindow::closeEvent(QCloseEvent *ev)
{
  setToolbarVisibilitySLOT();
  // the settings dialog saves "Show display" as well: without auto-save it
  // keeps the start layout's
  if (!m_autoSaveLayout->isChecked())
    m_wid->setToolbarVisibility(m_startDisplay, toolBarDMM->isVisible(), toolBarRecorder->isVisible(),
                                toolBarFile->isVisible());
  // dock layout (meter position, floating state, size) and toolbar layout
  saveWindows();
  m_wid->settings()->setInt("ReadingLog/max-rows", m_readings->maxRows());

  if (m_wid->closeWin())
    ev->accept();
  else
    ev->ignore();
}

// ---------------------------------------------------------------- MDI windows

QMdiSubWindow *MainWindow::addView(QWidget *view, const QString &title, int role, const QString &name)
{
  QMdiSubWindow *win = m_mdi->addSubWindow(view);
  // closing a window hides it; its action brings it back
  win->setAttribute(Qt::WA_DeleteOnClose, false);
  win->setWindowTitle(title);
  win->setWindowIcon(windowIcon());
  win->setObjectName(name);
  win->installEventFilter(this);
  m_arranger->addWindow(win, static_cast<MdiArranger::Role>(role));
  return win;
}

QAction *MainWindow::windowAction(QMdiSubWindow *win, const QString &text, const char *shortcut,
                               const char *icon, const QString &whatsThis)
{
  auto *action = new QAction(QIcon::fromTheme(icon), text, this);
  action->setShortcut(QKeySequence(shortcut));
  action->setWhatsThis(whatsThis);
  bindWindowAction(action, win);
  return action;
}

void MainWindow::bindWindowAction(QAction *action, QMdiSubWindow *win)
{
  action->setCheckable(true);
  action->setProperty("window", QVariant::fromValue<QObject *>(win));
  connect(action, &QAction::toggled, win, [this, win](bool on)
  {
    // the view inside may have been closed with the window before (Qt
    // closes it along): show it again, or the window stays empty
    if (on && win->widget())
      win->widget()->show();
    win->setVisible(on);
    if (on)
    {
      m_mdi->setActiveSubWindow(win);
      if (!m_restoring)
        growFor(win);
    }
  });
}

// A window was closed with its title bar button, or shown: its action follows.
bool MainWindow::eventFilter(QObject *watched, QEvent *event)
{
  if (watched == m_display && event->type() == QEvent::Resize)
    m_fold->move(m_display->width() - m_fold->width() - 6, 6);
  // closing a window (its title bar button, Ctrl+F4) only hides it: a real
  // close would close the view inside as well, and it would not come back
  if (event->type() == QEvent::Close)
    if (auto *win = qobject_cast<QMdiSubWindow *>(watched))
    {
      win->hide();
      event->ignore();
      return true;
    }
  // minimizing the main window hides its children spontaneously; they are
  // not closed, so only explicit show/hide counts (isHidden() stays false)
  if ((event->type() == QEvent::Show || event->type() == QEvent::Hide) && !event->spontaneous())
    for (QAction *a : { m_displayAction, m_meterAction, m_readingsAction, action_Graph })
      if (a && a->property("window").value<QObject *>() == watched)
      {
        const bool visible = !static_cast<QWidget *>(watched)->isHidden();
        if (a->isChecked() != visible && !m_restoring)
        {
          QSignalBlocker block(a);
          a->setChecked(visible);
          // the settings dialog keeps its own "Show display": update it, or
          // the next OK there would bring the display back
          if (a == m_displayAction)
            setToolbarVisibilitySLOT();
        }
      }
  return QMainWindow::eventFilter(watched, event);
}

void MainWindow::windowMenu(QMdiSubWindow *win, const QPoint &globalPos)
{
  QMenu menu(this);
  QAction *hide = menu.addAction(tr("&Hide window"));
  QAction *title = menu.addAction(tr("&Title bar"));
  title->setCheckable(true);
  title->setChecked(!MdiArranger::titleBarHidden(win));
  if (win == m_displayWin && m_controlsSupported)
  {
    QAction *hideControls = menu.addAction(tr("Hide &controls"));
    hideControls->setCheckable(true);
    hideControls->setChecked(m_fold->isFolded());
    connect(hideControls, &QAction::toggled, this, [this](bool on) { setControlsFolded(on); });
  }
  if (win == m_displayWin)
  {
    QMenu *lcd = menu.addMenu(tr("&LCD colours"));
    const std::pair<LcdWidget::LcdVariant, QString> variants[] = {
      { LcdWidget::Classic, tr("&Classic") }, { LcdWidget::BacklightBlue, tr("&Backlight blue") },
      { LcdWidget::Amber, tr("&Amber") }, { LcdWidget::HighContrast, tr("&High contrast") },
      { LcdWidget::Custom, tr("C&ustom (from the settings)") } };
    for (const auto &[v, text] : variants)
    {
      if (v == LcdWidget::Custom)
        lcd->addSeparator();
      QAction *a = lcd->addAction(text);
      a->setCheckable(true);
      a->setChecked(m_display->lcdVariant() == v);
      connect(a, &QAction::triggered, this, [this, v = v] { m_wid->setLcdVariant(v); });
    }
  }
  if (win == m_meterWin)
  {
    QMenu *style = menu.addMenu(tr("Meter &style"));
    const std::pair<int, QString> styles[] = { { 0, tr("&Dark studio") }, { 1, tr("Classic &ivory") } };
    for (const auto &[s, text] : styles)
    {
      QAction *a = style->addAction(text);
      a->setCheckable(true);
      a->setChecked(m_wid->meterStyle() == s);
      connect(a, &QAction::triggered, this, [this, s = s] { m_wid->setMeterStyle(s); });
    }
  }
  menu.addSeparator();
  menu.addMenu(m_arrangeMenu);
  QAction *chosen = menu.exec(globalPos);
  if (chosen == hide)
    win->hide();
  else if (chosen == title)
    m_arranger->setTitleBarHidden(win, !title->isChecked());
}

void MainWindow::syncArrangeActions()
{
  m_arrangeTop->setChecked(m_arranger->mode() == MdiArranger::DisplaysOnTop);
  m_arrangeLeft->setChecked(m_arranger->mode() == MdiArranger::DisplaysOnLeft);
  m_arrangeFixed->setChecked(m_arranger->mode() == MdiArranger::Fixed);
  m_arrangeFree->setChecked(m_arranger->mode() == MdiArranger::Free);
  m_titleBars->setChecked(m_arranger->titleBarsHidden());
}

void MainWindow::updateLed()
{
  const QColor c = !m_ledActive ? QApplication::palette().color(QPalette::Disabled, QPalette::WindowText)
                                : m_ledBlink ? QColor(0x22, 0xaa, 0x22) : QColor(0x66, 0xcc, 0x66);
  m_led->setText(QString("<span style='color:%1'>&#9679;</span>").arg(c.name()));
  const QString meter = m_wid->dmmConfigured() ? QString("%1 · %2").arg(m_wid->dmmTitle(), m_wid->portName())
                                               : m_wid->dmmTitle();
  m_led->setToolTip(m_ledActive ? tr("%1: readings are coming in").arg(meter)
                                : tr("%1: no reading for 3 s").arg(meter));
}

// Which windows are shown, the mode and - in Free mode - where
// they are. The display's visibility is the old Display/show key.
void MainWindow::restoreWindows()
{
  Settings *cfg = m_wid->settings();
  applyWorkspace([cfg](const QString &key, const QVariant &def) -> QVariant
  {
    if (def.typeId() == QMetaType::Bool)
      return cfg->getBool(key, def.toBool());
    return cfg->getString(key, def.toString());
  });
  m_autoSaveLayout->setChecked(cfg->getBool("Windows/auto-save", true));
  m_startDisplay = m_displayAction->isChecked();
}

void MainWindow::saveWindows()
{
  Settings *cfg = m_wid->settings();
  cfg->setBool("Windows/auto-save", m_autoSaveLayout->isChecked());
  // without auto-save the layout stays as last saved (or loaded)
  if (!m_autoSaveLayout->isChecked())
    return;
  storeWorkspace([cfg](const QString &key, const QVariant &value)
  {
    if (value.typeId() == QMetaType::Bool)
      cfg->setBool(key, value.toBool());
    else
      cfg->setString(key, value.toString());
  });
  cfg->setBool("Windows/user-sized", m_userSized);
}

void MainWindow::applyWorkspace(const WorkspaceGet &get)
{
  setDesign(Designs::fromName(get("Windows/design", QString("dark")).toString()));
  m_restoring = true;
  const QString arrange = get("Windows/arrange", QString("left")).toString();
  const bool free = arrange == "free";
  const MdiArranger::Mode mode = free ? MdiArranger::Free : arrange == "left" ? MdiArranger::DisplaysOnLeft
                                 : arrange == "fixed" ? MdiArranger::Fixed : MdiArranger::DisplaysOnTop;
  // Fixed gets its tree first: switching to it from Free would derive one
  // from the positions
  if (mode == MdiArranger::Fixed)
    m_arranger->setMode(MdiArranger::DisplaysOnTop);
  m_arranger->setMode(mode);
  // the layout the user made (dividers, swaps); none or a broken one: the rule
  if (!m_arranger->setLayoutText(get("Windows/layout", QString()).toString()))
    m_arranger->setLayoutText(QString());   // a broken one: the rule
  m_arranger->setTitleBarsHidden(get("Windows/title-bars-hidden", !free).toBool());
  if (free)
  {
    QMap<QString, QString> geometry;
    for (QMdiSubWindow *w : { m_displayWin, m_meterWin, m_graphWin, m_readingsWin })
      geometry[w->objectName()] = get("Windows/geometry-" + w->objectName(), QString()).toString();
    // once the window is shown and the area has its size: the automatic
    // layout for a start, then the stored positions on top of it
    QTimer::singleShot(0, this, [this, geometry]
    {
      m_arranger->arrangeNow();
      for (QMdiSubWindow *w : { m_displayWin, m_meterWin, m_graphWin, m_readingsWin })
      {
        const QStringList g = geometry.value(w->objectName()).split(' ');
        if (g.size() == 4)
          w->setGeometry(g[0].toInt(), g[1].toInt(), g[2].toInt(), g[3].toInt());
      }
    });
  }
  m_displayAction->setChecked(get("Display/show", true).toBool());
  m_meterAction->setChecked(get("Windows/meter", true).toBool());
  action_Graph->setChecked(get("MainWindow/show-graph", false).toBool());   // a fresh start is a compact instrument
  m_readingsAction->setChecked(get("Windows/readings", false).toBool());
  // an unchecked action did not toggle: hide its window explicitly
  for (QAction *a : { m_displayAction, m_meterAction, action_Graph, m_readingsAction })
    qobject_cast<QWidget *>(a->property("window").value<QObject *>())->setVisible(a->isChecked());
  m_restoring = false;
  syncArrangeActions();
  m_arranger->arrange();
}

void MainWindow::storeWorkspace(const WorkspaceSet &set)
{
  static const char *const modes[] = { "top", "left", "fixed", "free" };
  set("Windows/arrange", QString(modes[m_arranger->mode()]));
  set("Windows/layout", m_arranger->layoutText());
  set("Windows/title-bars-hidden", m_arranger->titleBarsHidden());
  set("Windows/design", Designs::name(Designs::current()));
  if (m_arranger->mode() == MdiArranger::Free)
    for (QMdiSubWindow *w : { m_displayWin, m_meterWin, m_graphWin, m_readingsWin })
    {
      const QRect g = w->geometry();
      set("Windows/geometry-" + w->objectName(),
          QString("%1 %2 %3 %4").arg(g.x()).arg(g.y()).arg(g.width()).arg(g.height()));
    }
  set("Display/show", m_displayAction->isChecked());
  set("Windows/meter", m_meterAction->isChecked());
  set("Windows/readings", m_readingsAction->isChecked());
  set("MainWindow/show-graph", action_Graph->isChecked());
}

// A workspace file is an ini file with the same keys as the settings, plus
// the size of the main window.
static const char *kWorkspaceSuffix = "qtdmm-workspace";

void MainWindow::saveWorkspace()
{
  Settings *cfg = m_wid->settings();
  const QString dir = cfg->getString("Windows/workspace-dir",
                                     QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation));
  QString file = QFileDialog::getSaveFileName(this, tr("Save workspace"), dir,
                                              tr("QtDMM workspace (*.%1)").arg(kWorkspaceSuffix));
  if (file.isEmpty())
    return;
  if (QFileInfo(file).suffix().isEmpty())
    file += QString(".") + kWorkspaceSuffix;
  QFile::remove(file);   // only our keys in it
  QSettings ws(file, QSettings::IniFormat);
  ws.setValue("Workspace/version", 1);
  ws.setValue("Workspace/size", QString("%1 %2").arg(width()).arg(height()));
  storeWorkspace([&ws](const QString &key, const QVariant &value) { ws.setValue(key, value); });
  ws.sync();
  if (ws.status() != QSettings::NoError)
  {
    QMessageBox::warning(this, tr("QtDMM: Save workspace"), tr("Could not write %1.").arg(QDir::toNativeSeparators(file)));
    return;
  }
  cfg->setString("Windows/workspace-dir", QFileInfo(file).absolutePath());
  Q_EMIT m_wid->error(tr("Workspace saved to %1").arg(QDir::toNativeSeparators(file)));
}

void MainWindow::loadWorkspace()
{
  Settings *cfg = m_wid->settings();
  const QString dir = cfg->getString("Windows/workspace-dir",
                                     QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation));
  const QString file = QFileDialog::getOpenFileName(this, tr("Load workspace"), dir,
                                                    tr("QtDMM workspace (*.%1)").arg(kWorkspaceSuffix));
  if (file.isEmpty())
    return;
  QSettings ws(file, QSettings::IniFormat);
  if (ws.status() != QSettings::NoError || ws.value("Workspace/version").toInt() < 1)
  {
    QMessageBox::warning(this, tr("QtDMM: Load workspace"),
                         tr("%1 is no QtDMM workspace.").arg(QDir::toNativeSeparators(file)));
    return;
  }
  cfg->setString("Windows/workspace-dir", QFileInfo(file).absolutePath());
  // the size first: the arrangement fills the area it gets
  const QStringList size = ws.value("Workspace/size").toString().split(' ');
  if (size.size() == 2 && !isMaximized() && !isFullScreen())
  {
    resize(size[0].toInt(), size[1].toInt());
    m_userSized = true;   // a size chosen on purpose: no growing
  }
  applyWorkspace([&ws](const QString &key, const QVariant &def) -> QVariant
  {
    const QVariant v = ws.value(key, def);
    return def.typeId() == QMetaType::Bool ? QVariant(v.toBool()) : QVariant(v.toString());
  });
  // without auto-save the loaded workspace is the one to start with next time
  if (!m_autoSaveLayout->isChecked())
  {
    m_autoSaveLayout->setChecked(true);
    saveWindows();
    m_autoSaveLayout->setChecked(false);
    m_startDisplay = m_displayAction->isChecked();
  }
  Q_EMIT m_wid->error(tr("Workspace loaded from %1").arg(QDir::toNativeSeparators(file)));
}

// ---------------------------------------------------------------- window size

int MainWindow::em() const
{
  return QFontMetrics(font()).height();
}

QSize MainWindow::growthFor(QMdiSubWindow *win) const
{
  if (win == m_graphWin)
    return QSize(0, 18 * em());    // below the instruments
  if (win == m_readingsWin)
    return QSize(19 * em(), 0);    // a column on the right
  if (win == m_meterWin || win == m_displayWin)
    return QSize(20 * em(), 0);    // next to the other instrument
  return QSize();
}

void MainWindow::growFor(QMdiSubWindow *win)
{
  if (m_grown.contains(win))
    return;
  m_grown.insert(win);
  // the display and the analog meter share the top row: the first of them
  // needs no room of its own
  if ((win == m_displayWin || win == m_meterWin)
      && !m_grown.contains(win == m_displayWin ? m_meterWin : m_displayWin))
    return;
  const QSize before = size();
  autoGrow(growthFor(win));
  if (win == m_readingsWin)
    m_arranger->setTableWidth(size().width() - before.width());
}

void MainWindow::autoGrow(const QSize &delta)
{
  if (m_userSized || !m_growEnabled || isMaximized() || isFullScreen())
    return;
  const QRect av = screen()->availableGeometry();
  // Wayland neither tells a window where it is nor lets it move, so a
  // grown frame cannot be pulled back onto the screen: grow less there.
  // Compositors place new windows in the upper left part (Miriway at a
  // quarter of the width, KWin further left), which leaves room for 70 %.
  const double limit = QGuiApplication::platformName().startsWith("wayland") ? 0.70 : 0.84;
  const int maxW = int(av.width() * limit), maxH = int(av.height() * limit);
  const QSize s = size();
  const QSize n(qMax(s.width(), qMin(s.width() + delta.width(), maxW)),
                qMax(s.height(), qMin(s.height() + delta.height(), maxH)));
  if (n == s)
    return;
  const QSize frameExtra = frameGeometry().size() - s;
  m_expectSize = n;
  resize(n);
  // stay on screen: move up/left when the grown frame would stick out
  QPoint p = pos();
  p.setX(qMax(av.left(), qMin(p.x(), av.right() + 1 - (n.width() + frameExtra.width()))));
  p.setY(qMax(av.top(), qMin(p.y(), av.bottom() + 1 - (n.height() + frameExtra.height()))));
  if (p != pos())
    move(p);
}

void MainWindow::resizeEvent(QResizeEvent *event)
{
  QMainWindow::resizeEvent(event);
  if (m_growEnabled && event->size() != m_expectSize && event->oldSize().isValid()
      && !isMaximized() && !isFullScreen())
    m_userSized = true;
}

void MainWindow::changeEvent(QEvent *event)
{
  QMainWindow::changeEvent(event);
  if (event->type() == QEvent::WindowStateChange && (isMaximized() || isFullScreen()))
    m_userSized = true;
}

void MainWindow::showEvent(QShowEvent *event)
{
  QMainWindow::showEvent(event);
  // the window manager may still adjust the first size; what it settles on
  // is the start, not a size the user chose
  if (!m_growEnabled)
    QTimer::singleShot(300, this, [this]
    {
      m_expectSize = size();
      m_growEnabled = true;
    });
}

// ---------------------------------------------------------------- meter keys

void MainWindow::setControlsFolded(bool folded)
{
  m_fold->setFolded(folded);
  m_wid->settings()->setBool("Display/controls-hidden", folded);
  updateControls();
}

void MainWindow::updateControls()
{
  m_fold->setVisible(m_controlsSupported);
  const bool show = m_controlsSupported && !m_fold->isFolded();
  m_controls->setVisible(show);
  // the keys below the LCD are part of its window
  m_arranger->setContentAspect(m_displayWin, LcdWidget::kMinAspect, LcdWidget::kMaxAspect,
                               show ? m_controls->sizeHint().height() + 4 : 0);
  // the first time the keys appear, the window grows for them
  if (show && !m_grown.contains(m_controls))
  {
    m_grown.insert(m_controls);
    autoGrow(QSize(0, 2 * em()));
  }
  m_arranger->arrange();
}

void MainWindow::setDesign(int design)
{
  const auto d = static_cast<Designs::Design>(design);
  Designs::apply(d);
  m_mdi->setBackground(Designs::areaBrush(d));
  // the instruments are drawn on a transparent background: the window's
  // (brushed metal in Silver) shows around them
  const QBrush frame = Designs::frameBrush(d);
  for (QWidget *view : { static_cast<QWidget *>(m_display), static_cast<QWidget *>(m_meter),
                         static_cast<QWidget *>(m_readings) })
  {
    const bool own = frame.style() != Qt::NoBrush;
    QPalette pal = view->palette();
    pal.setBrush(QPalette::Window, own ? frame : QApplication::palette().window());
    view->setPalette(pal);
    view->setAutoFillBackground(own);
    view->update();
  }
  updateLed();
  const Designs::GraphColors g = Designs::graphColors(d);
  m_wid->graph()->setThemeColors(g.background, g.grid, g.labels, g.data);
  for (QAction *a : m_designMenu->actions())
    a->setChecked(a->data().toInt() == design);
  // at once, so the settings dialog shows the design of the menu
  m_wid->settings()->setString("Windows/design", Designs::name(d));
}

void MainWindow::setToolbarVisibilitySLOT()
{
  m_wid->setToolbarVisibility(m_displayAction->isChecked(),
                              toolBarDMM->isVisible(),
                              toolBarRecorder->isVisible(),
                              toolBarFile->isVisible());
}

void MainWindow::setConnectSLOT(bool on)
{
  action_Connect->setChecked(on);
}

void MainWindow::toolbarVisibilitySLOT(bool disp, bool dmm, bool graph, bool file)
{
  toolBarDMM->setVisible(dmm);
  toolBarRecorder->setVisible(graph);
  toolBarFile->setVisible(file);
  m_displayAction->setChecked(disp);
}

void MainWindow::setupIcons()
{
  // theme icons exist on Linux desktops only; Windows and macOS get the
  // bundled ones
  QIcon iconConnectOn = QIcon::fromTheme("network-connect");
  QIcon iconConnectOff = QIcon::fromTheme("network-disconnect");

  this->action_Connect->setIcon(iconConnectOff);
  connect(this->action_Connect, &QAction::toggled, this, [ = ](bool checked)
  {
    m_controls->setConnected(checked);
    this->action_Connect->setIcon(checked ? iconConnectOn : iconConnectOff);
  });
}

void MainWindow::bringMainWindowToFront()
{
  QWidget *mainWin = nullptr;
  const auto topWidgets = QApplication::topLevelWidgets();

  for (QWidget *w : topWidgets)
  {
    if (w->inherits("MainWindow"))
    {
      mainWin = w;
      break;
    }
  }

  if (!mainWin)
    return;

  mainWin->showNormal();
  mainWin->raise();
  mainWin->activateWindow();
}
