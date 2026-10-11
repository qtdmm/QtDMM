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
#include "ui/dialogs/devicesettingsdlg.h"
#include "ui/devicesettings.h"
#include "ui/devicesidebar.h"
#include "ui/tilebutton.h"
#include "ui/dialogs/adddevicedlg.h"
#include "device/transport.h"
#include "device/transports/serial.h"
#include "core/devicelibrary.h"
#include "core/instances.h"
#include "core/sampletypes.h"
#include "core/siprefix.h"
#include "ui/instancewidget.h"
#include "ui/views/graphwidget.h"
#include "ui/views/lcdwidget.h"
#include "ui/views/poincareplot.h"
#include "ui/views/analogmeter.h"
#include "ui/views/readingswidget.h"
#include "core/settings.h"
#include "ui/alarmbar.h"
#include "ui/mdiarranger.h"
#include "service/metercontroller.h"
#include "device/meterconnection.h"
#include "ui/designs.h"
#include "ui/controlbar.h"
#include <QMdiArea>
#include <QMdiSubWindow>
#include <QLoggingCategory>
#include <QIconEngine>

namespace
{
// The Record button's symbol, drawn at any size: a grey dot to start, a red
// square while it records
class RecordIconEngine : public QIconEngine
{
public:
  explicit RecordIconEngine(bool stop) : m_stop(stop) {}
  void paint(QPainter *painter, const QRect &rect, QIcon::Mode mode, QIcon::State) override
  {
    painter->save();
    painter->setRenderHint(QPainter::Antialiasing);
    const qreal side = qMin(rect.width(), rect.height()) * (m_stop ? 0.56 : 0.62);
    const QRectF r(QPointF(rect.center()) + QPointF(0.5, 0.5) - QPointF(side / 2, side / 2), QSizeF(side, side));
    QColor fill = m_stop ? QColor(0xd3, 0x2f, 0x2f) : QColor(0x8c, 0x8c, 0x8c);
    if (mode == QIcon::Disabled)
      fill.setAlpha(80);
    QColor edge = fill.darker(140);
    edge.setAlpha(fill.alpha());
    painter->setPen(QPen(edge, qMax<qreal>(1.0, side / 12)));
    painter->setBrush(fill);
    if (m_stop)
      painter->drawRoundedRect(r, side * 0.12, side * 0.12);
    else
      painter->drawEllipse(r);
    painter->restore();
  }
  QPixmap pixmap(const QSize &size, QIcon::Mode mode, QIcon::State state) override
  {
    QPixmap pm(size);
    pm.fill(Qt::transparent);
    QPainter painter(&pm);
    paint(&painter, QRect(QPoint(0, 0), size), mode, state);
    return pm;
  }
  QIconEngine *clone() const override { return new RecordIconEngine(m_stop); }

private:
  bool m_stop;
};

/// A theme symbol with a "+" in its lower right corner (Add device: the
/// meter and "add"), looked up when painted, so it follows the symbol set.
class PlusIconEngine : public QIconEngine
{
public:
  explicit PlusIconEngine(const QString &name) : m_name(name) {}
  void paint(QPainter *painter, const QRect &rect, QIcon::Mode mode, QIcon::State state) override
  {
    QIcon::fromTheme(m_name).paint(painter, rect, Qt::AlignCenter, mode, state);
    const int side = qMax(6, int(qMin(rect.width(), rect.height()) * 0.55));
    const QRect corner(rect.right() + 1 - side, rect.bottom() + 1 - side, side, side);
    QIcon::fromTheme("list-add").paint(painter, corner, Qt::AlignCenter, mode, state);
  }
  QPixmap pixmap(const QSize &size, QIcon::Mode mode, QIcon::State state) override
  {
    QPixmap pm(size);
    pm.fill(Qt::transparent);
    QPainter painter(&pm);
    paint(&painter, QRect(QPoint(0, 0), size), mode, state);
    return pm;
  }
  QIconEngine *clone() const override { return new PlusIconEngine(m_name); }

private:
  QString m_name;
};
}

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
  // the empty start: one big button on the area's background, over the
  // views, which stay arranged below it
  m_emptyStart = new QWidget(m_mdi);
  m_emptyStart->setAutoFillBackground(true);
  auto *emptyLayout = new QGridLayout(m_emptyStart);
  auto *addButton = new TileButton(m_emptyStart);
  addButton->setObjectName("ui_emptyStart");
  addButton->setIcon(QIcon::fromTheme("list-add"));
  addButton->setIconSize(QSize(64, 64));
  addButton->setText(tr("&Add device"));
  addButton->setToolTip(tr("Add device: a meter, a sensor or a calculated value"));
  QFont big = addButton->font();
  big.setPointSizeF(big.pointSizeF() * 1.4);
  addButton->setFont(big);
  addButton->setMinimumSize(260, 160);
  emptyLayout->addWidget(addButton, 0, 0, Qt::AlignCenter);
  m_emptyStart->hide();
  m_mdi->installEventFilter(this);
  connect(addButton, &QToolButton::clicked, this, &MainWindow::addDevice);

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
    // a Bluetooth link takes a few seconds before it takes keys
    if (!m_wid->controller()->pressKey(key))
      statusBar()->showMessage(tr("The key did not reach the meter: it is not connected yet."), 4000);
  });
  connect(m_wid, &InstanceWidget::remoteControl, this, [this](bool supported)
  {
    m_controlsSupported = supported;
    // readConfig() has set the protocol: the keys this meter has
    m_controls->setProtocol(m_wid->controller()->dmm()->format());
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

  // the scatter of the main value: every reading, also while hidden
  m_poincare = new PoincarePlot(this);
  m_poincare->setSettings(m_wid->settings());
  connect(m_wid->controller(), &MeterController::reading, m_poincare, &PoincarePlot::addReading);
  connect(m_wid->controller(), &MeterController::staleChanged, m_poincare, &PoincarePlot::setStale);
  m_poincareWin = addView(m_poincare, tr("Poincaré plot"), MdiArranger::Table, "poincare");

  // right-click on display or meter: the window's menu (also without title bar)
  for (auto [view, win] : { std::pair<QWidget *, QMdiSubWindow *>{m_display, m_displayWin},
                            std::pair<QWidget *, QMdiSubWindow *>{m_meter, m_meterWin},
                            std::pair<QWidget *, QMdiSubWindow *>{m_poincare, m_poincareWin} })
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
       "the raw protocol of the session next to the graph. Copy rows to a "
       "spreadsheet or export them as CSV.</p></body></html>"));
  m_poincareAction = windowAction(m_poincareWin, tr("Poi&ncaré plot"), "Ctrl+5", "qtdmm-poincare",
    tr("<html><head/><body><p><span style=\" font-weight:600;\">Poincaré plot</span></p>"
       "<p>Each reading against the next one: noise widens the cloud across the diagonal, "
       "drift stretches it along it. SD1 and SD2 put both in numbers.</p></body></html>"));
  // the graph keeps its action from the .ui (toolbar button with Ctrl+3)
  bindWindowAction(action_Graph, m_graphWin);

  // the sidebar "Devices" at the left: My devices, a click switches
  m_sidebar = new DeviceSidebar(m_wid->devices(), this);
  m_sidebarDock = new QDockWidget(tr("Devices"), this);
  m_sidebarDock->setObjectName("ui_devicesDock");
  m_sidebarDock->setFeatures(QDockWidget::NoDockWidgetFeatures);   // the toolbar button shows and hides it
  m_sidebarDock->setWidget(m_sidebar);
  addDockWidget(Qt::LeftDockWidgetArea, m_sidebarDock);
  m_sidebarDock->hide();
  // an own action: without DockWidgetClosable Qt disables the toggleViewAction()
  m_sidebarAction = new QAction(QIcon::fromTheme("view-sidetree"), tr("De&vices"), this);
  m_sidebarAction->setObjectName("ui_sidebarAction");
  m_sidebarAction->setCheckable(true);
  m_sidebarAction->setShortcut(QKeySequence("F9"));   // as the sidebars of KDE's file managers
  QAction *sidebar = m_sidebarAction;
  sidebar->setToolTip(tr("Devices: show or hide the sidebar with My devices and the instances"));
  addAction(sidebar);   // F9 also with the toolbar hidden
  sidebar->setWhatsThis(tr("<html><head/><body><p><span style=\" font-weight:600;\">Devices</span></p>"
                           "<p>The sidebar with My devices: click one to switch to it, its context menu has its "
                           "settings, rename, a new window and remove. Drag an entry to change the order."
                           "</p></body></html>"));
  connect(sidebar, &QAction::triggered, this, [this](bool on)
  {
    showSidebar(on);
    m_wid->settings()->setBool("Windows/sidebar", on);
  });
  connect(m_sidebar, &DeviceSidebar::switchRequested, m_wid, [this](const QString &id)
  {
    // the selection is the device in use: the new one, or after Cancel at
    // the question about unsaved data the one before
    m_wid->switchDevice(id);
    m_sidebar->setCurrentItem(m_sidebar->deviceItem(m_wid->currentDevice()));
  });
  connect(m_sidebar, &DeviceSidebar::settingsRequested, this, &MainWindow::deviceSettings);
  connect(m_sidebar, &DeviceSidebar::newWindowRequested, this, &MainWindow::openInNewWindow);
  connect(m_sidebar, &DeviceSidebar::instanceRequested, this, &MainWindow::openInstance);
  connect(m_sidebar, &DeviceSidebar::renameInstanceRequested, this, &MainWindow::renameInstance);
  connect(m_sidebar, &DeviceSidebar::deleteInstanceRequested, this, &MainWindow::deleteInstance);
  connect(m_sidebar, &DeviceSidebar::addDeviceRequested, this, &MainWindow::addDevice);
  // the readings of the others change all the time
  m_instancesTimer = new QTimer(this);
  m_instancesTimer->setInterval(1000);
  connect(m_instancesTimer, &QTimer::timeout, this, &MainWindow::updateInstances);
  m_instancesTimer->start();
  toolBarDMM->insertAction(toolBarDMM->actions().value(0), sidebar);
  m_addDeviceAction = new QAction(QIcon(new PlusIconEngine("qtdmm-dmm")), tr("&Add device..."), this);
  m_addDeviceAction->setToolTip(tr("Add device: a meter, a sensor or a calculated value"));
  m_addDeviceAction->setWhatsThis(tr("<html><head/><body><p><span style=\" font-weight:600;\">Add device</span></p>"
                                     "<p>Step by step: how the meter is connected, which one it is, and whether it "
                                     "goes into this window or a new one. It is kept in My devices.</p></body></html>"));
  connect(m_addDeviceAction, &QAction::triggered, this, &MainWindow::addDevice);
  toolBarDMM->insertAction(sidebar, m_addDeviceAction);

  toolBarDMM->addSeparator();
  toolBarDMM->addAction(m_displayAction);
  toolBarDMM->addAction(m_meterAction);
  toolBarDMM->addAction(m_readingsAction);
  toolBarDMM->addAction(m_poincareAction);
  // the graph is a window like the others (it sat with the recording)
  toolBarDMM->addAction(action_Graph);
  connect(m_displayAction, &QAction::toggled, this, &MainWindow::storeDisplaySLOT);

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
  m_titleBars->setShortcut(QKeySequence("F10"));
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
  // the design can be chosen in the settings (General), too
  connect(m_wid, &InstanceWidget::configChanged, this, [this]
  {
    const Designs::Design d = Designs::fromName(m_wid->settings()->getString("Windows/design", "dark"));
    if (d != Designs::current())
      setDesign(d);
  });

  createExtraActions();
  addShortcutsToToolTips();

  connect(m_wid, SIGNAL(running(bool)), this, SLOT(runningSLOT(bool)));
  connect(m_wid, &InstanceWidget::recorderState, this, &MainWindow::updateRecorderActions);

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
  connect(m_wid, &InstanceWidget::showDisplay, this, &MainWindow::showDisplaySLOT);

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
  for (QMdiSubWindow *w : { m_displayWin, m_meterWin, m_graphWin, m_readingsWin, m_poincareWin })
    if (!w->isHidden())
      m_grown.insert(w);
  if (!m_controls->isHidden())
    m_grown.insert(m_controls);
  if (m_grown.contains(m_readingsWin) || m_grown.contains(m_poincareWin))
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
    for (QMdiSubWindow *w : { m_graphWin, m_readingsWin, m_poincareWin })
      if (m_grown.contains(w))
        start += growthFor(w);
    if (!m_controls->isHidden())
      start += QSize(0, 2 * em());
    const QRect av = screen()->availableGeometry();
    resize(qMin(start.width(), int(av.width() * 0.84)), qMin(start.height(), int(av.height() * 0.84)));
  }
  m_expectSize = size();

  connect(m_stateMgr, &SharedStateManager::stateChanged, this, [=](const QString& state){
    // RECORD_<ms> / STOP_<ms>: a command is new each time - a plain "RECORD"
    // after a recording that ended without Stop (its length, another
    // device) was the same state again, and no instance started
    if (state.startsWith("RECORD_"))
    {
      // another instance's start: nobody here to ask (review R9-01) - a
      // recording running goes on, one not saved stays
      QString why;
      if (state != m_ownRecord && !m_wid->controller()->mayStartRecording(&why))
      {
        m_info->setText(tr("Another instance: %1").arg(why));
        return;
      }
      QMetaObject::invokeMethod(m_wid, "startSLOT", Qt::DirectConnection);
      m_localRecord = false;
    }
    else if (state.startsWith("STOP_"))
    {
      // the others stop too, without sending a STOP of their own
      if (!m_localRecord)
      {
        m_remoteStop = true;
        action_Stop->trigger();
        m_remoteStop = false;
      }
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
  // instance would otherwise try the first serial port it finds; without
  // one the window starts empty
  if (m_stateMgr->registerInstance() && m_wid->dmmConfigured())
    QTimer::singleShot(1000, action_Connect, &QAction::trigger);
  updateEmptyStart();
  connect(m_wid, &InstanceWidget::configChanged, this, &MainWindow::updateEmptyStart);
  // the sidebar as left, with two devices or more; a window without a meter
  // shows it when there are devices to choose from
  const bool sidebarSet = !m_wid->settings()->getString("Windows/sidebar").isEmpty();
  showSidebar(DeviceSidebar::shownAtStart(sidebarSet ? QVariant(m_wid->settings()->getBool("Windows/sidebar"))
                                                     : QVariant(),
                                          int(m_wid->devices()->list().size()), m_wid->dmmConfigured()));
}

void MainWindow::updateEmptyStart()
{
  const bool empty = !m_wid->dmmConfigured();
  if (empty)
  {
    m_emptyStart->setGeometry(m_mdi->rect());
    m_emptyStart->raise();
  }
  m_emptyStart->setVisible(empty);
}

void MainWindow::addDevice()
{
  AddDeviceDlg dlg(m_wid->devices(), this);
  dlg.setPorts(Transport::availablePorts() + m_wid->settings()->customPorts());
  dlg.setSigrokExe(m_wid->settings()->getString("Port settings/sigrok_exe", "sigrok-cli"));
  dlg.setStateManager(m_stateMgr);
  dlg.setPlacesInUse(placesInUse());
  if (m_wid->dmmConfigured())
    dlg.setCurrentDevice(m_wid->dmmTitle());
  else
    dlg.setTargetChoice(false);   // an empty window: the device goes into it
  if (dlg.exec() != QDialog::Accepted)
    return;
  QVariantMap keys = dlg.keys();
  keys.insert("Port settings/device", SerialDevice::stableDevice(keys.value("Port settings/device").toString()));
  const int before = int(m_wid->devices()->list().size());
  // a find that is one of My devices changes that entry: no second one
  QString id = dlg.knownDevice();
  if (id.isEmpty())
    id = m_wid->devices()->add(dlg.name(), keys);
  else
  {
    m_wid->devices()->update(id, keys);
    m_wid->devices()->rename(id, dlg.name());
  }
  if (dlg.target() == AddDeviceDlg::NewWindow)
    openInNewWindow(id);
  else
    m_wid->switchDevice(id);
  // the second device: from now on there is a choice
  if (DeviceSidebar::opensAfterAdd(before, int(m_wid->devices()->list().size())))
  {
    showSidebar(true);
    m_wid->settings()->setBool("Windows/sidebar", true);
  }
}

QMap<QString, QString> MainWindow::placeOwners() const
{
  QMap<QString, QString> owners;
  for (const QString &instance : m_stateMgr->instances())
  {
    const bool here = instance == m_stateMgr->id();
    const Settings other(instance == QLatin1String("default") ? QString() : instance, m_wid->settings()->configDir());
    const QString place = DeviceLibrary::place(here ? m_wid->settings()->meterKeys() : other.meterKeys());
    // a formula is no port: any number of windows may compute it
    if (!place.isEmpty() && !place.startsWith(QLatin1String("calc ")))
      owners.insert(place, instance);
  }
  return owners;
}

QMap<QString, QString> MainWindow::placesInUse() const
{
  QMap<QString, QString> inUse;
  const QMap<QString, QString> owners = placeOwners();
  for (auto it = owners.constBegin(); it != owners.constEnd(); ++it)
    inUse.insert(it.key(), it.value() == m_stateMgr->id() ? tr("In use here")
                                                          : tr("In use by the instance %1").arg(it.value()));
  return inUse;
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
  // toggled, not triggered: a connection made by the program (Add device, the
  // sidebar, the start, SCPI) sets the check mark only - Record stayed off
  connect(action_Connect, &QAction::toggled, this, &MainWindow::connectSLOT);
  connect(action_Reset, SIGNAL(triggered()), m_wid, SLOT(resetSLOT()));
  connect(action_Start, SIGNAL(triggered()), this, SLOT(startSLOT()));
  // one button for both: a grey dot starts, the red square stops
  connect(action_Record, &QAction::triggered, this, &MainWindow::toggleRecordingSLOT);
  connect(action_Live, &QAction::triggered, this, [this]
  {
    m_wid->liveSLOT();
    updateRecorderActions();   // pressed again when the question was cancelled
  });
  // the graph's context menu starts and stops like the button
  connect(m_wid->graph(), &GraphWidget::recordRequested, this, [this](bool start)
  {
    if (!start)
      action_Stop->trigger();
    else if (action_Start->isEnabled())
      action_Start->trigger();
  });
  connect(action_Stop, SIGNAL(triggered()), m_wid, SLOT(stopSLOT()));
  connect(action_Stop, SIGNAL(triggered()), this, SLOT(stopSLOT()));
  connect(action_Clear, SIGNAL(triggered()), m_wid, SLOT(clearSLOT()));
  connect(action_Print, SIGNAL(triggered()), m_wid, SLOT(printSLOT()));
  connect(action_Import, SIGNAL(triggered()), m_wid, SLOT(importSLOT()));
  connect(action_Export, SIGNAL(triggered()), m_wid, SLOT(exportSLOT()));
  connect(action_Configure, SIGNAL(triggered()), m_wid, SLOT(configSLOT()));
  // Shift+F8: the settings of the device in use, as its context menu in the sidebar
  connect(action_ConfigureDMM, &QAction::triggered, this, [this]
  {
    if (m_wid->devices()->find(m_wid->currentDevice()))
      deviceSettings(m_wid->currentDevice());
    else
      addDevice();
  });
  connect(actionConfigureRecorder, SIGNAL(triggered()), m_wid, SLOT(configRecorderSLOT()));
  connect(action_Quit, SIGNAL(triggered()), this, SLOT(storeDisplaySLOT()));
  connect(action_Quit, SIGNAL(triggered()), m_wid, SLOT(quitSLOT()));
  connect(action_Direct_help, SIGNAL(triggered()), m_wid, SLOT(helpSLOT()));


  connect(m_stateMgr, &SharedStateManager::instancesChanged, this, &MainWindow::updateInstances);

}

// Actions that live only in the popup menu are not attached to any widget,
// so their shortcuts would be dead - adding them to the window fixes that.
void MainWindow::createExtraActions()
{
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
                               "<p>Puts a picture of the graph on the clipboard, ready to paste into a "
                               "report or a chat.</p></body></html>"));
  connect(m_copyImage, &QAction::triggered, m_wid->graph(), &GraphWidget::copyImageSLOT);

  // Space (Record) toggles the recorder; Start and Stop have neither a
  // button nor a key of their own any more
  addActions({action_Configure, action_ConfigureDMM, action_Direct_help, action_Help, action_Quit,
              m_displayAction, m_meterAction, m_readingsAction, m_poincareAction, m_titleBars,
              m_fullScreen, m_zoomIn, m_zoomOut, m_zoomFit, m_copyImage});
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
  // a recording viewed and not saved yet: the new one clears it
  if (!m_wid->confirmRecording())
    return;
  if (!m_wid->askRecordingLength())
    return;
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
        m_ownRecord = "RECORD_" + QString::number(QDateTime::currentMSecsSinceEpoch());
        m_stateMgr->writeState(m_ownRecord);
        m_localRecord = false;
        return;
    }
  }
}

void MainWindow::stopSLOT()
{
  qInfo() << "stop" << m_localRecord;
  if (!m_localRecord && !m_remoteStop)
    m_stateMgr->writeState("STOP_" + QString::number(QDateTime::currentMSecsSinceEpoch()));
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
  updateRecorderActions();
}

void MainWindow::connectSLOT(bool on)
{
  action_Start->setEnabled(on);
  action_Stop->setEnabled(on && m_running);
  // whether it records is runningSLOT()'s: an alarm's program has the port
  // a while and the recording runs on
  updateRecorderActions();
}

void MainWindow::updateRecorderActions()
{
  const RecordingStore::State state = m_wid->controller()->recorder()->state();
  const bool recording = state == RecordingStore::Record;
  action_Record->setEnabled(recording ? action_Stop->isEnabled() : action_Start->isEnabled());
  action_Record->setIcon(QIcon(new RecordIconEngine(recording)));
  action_Record->setText(recording ? tr("S&top") : tr("&Record"));
  const QString key = action_Record->shortcut().toString(QKeySequence::NativeText);
  action_Record->setToolTip(QString("%1 (%2)").arg(recording ? tr("Stop recording") : tr("Start recording"), key));
  action_Live->setEnabled(!recording);
  action_Live->setChecked(state == RecordingStore::Live);
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
                   "Symbols from the <b>Oxygen</b> and <b>Breeze</b> icon themes of the KDE community (LGPL 3); "
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
    m_menu->addAction(m_sidebarAction);
    m_menu->addAction(action_Graph);
    m_menu->addAction(m_displayAction);
    m_menu->addAction(m_meterAction);
    m_menu->addAction(m_readingsAction);
    m_menu->addAction(m_poincareAction);
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
  storeDisplaySLOT();
  // the settings dialog saves "Show display" as well: without auto-save it
  // keeps the start layout's
  if (!m_autoSaveLayout->isChecked())
    m_wid->setShowDisplay(m_startDisplay);
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
  if (watched == m_mdi && event->type() == QEvent::Resize)
    m_emptyStart->setGeometry(m_mdi->rect());
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
    for (QAction *a : { m_displayAction, m_meterAction, m_readingsAction, m_poincareAction, action_Graph })
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
            storeDisplaySLOT();
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
  if (win == m_poincareWin)
  {
    menu.addSeparator();
    m_poincare->addMenuActions(&menu);
  }
  QAction *chosen = menu.exec(globalPos);
  if (chosen == hide)
    win->hide();
  else if (chosen == title)
    m_arranger->setTitleBarHidden(win, !title->isChecked());
}

void MainWindow::showSidebar(bool show)
{
  m_sidebarDock->setVisible(show);
  m_sidebarAction->setChecked(show);
  updateInstances();
}

void MainWindow::updateInstances()
{
  if (!m_sidebarDock->isVisible())
    return;
  const QString own = m_stateMgr->id();
  const QStringList running = m_stateMgr->instances();
  QStringList ids = m_wid->settings()->getConfigInstances();
  for (const QString &id : running)
    if (!ids.contains(id))
      ids << id;
  const auto readings = m_stateMgr->readings();
  const qint64 now = QDateTime::currentMSecsSinceEpoch();
  QList<DeviceSidebar::Instance> rows;
  for (const QString &id : ids)
  {
    DeviceSidebar::Instance row;
    row.id = id;
    row.running = id == own || running.contains(id);
    // its device: the entry of My devices, else the model or the formula
    const Settings other(id == QLatin1String("default") ? QString() : id, m_wid->settings()->configDir());
    const QVariantMap keys = id == own ? m_wid->settings()->meterKeys() : other.meterKeys();
    const std::optional<MyDevice> device = m_wid->devices()->find(keys.value("DMM/my-device").toString());
    const QString model = keys.value("DMM/model").toString();
    if (device)
      row.device = device->name;
    else if (!keys.value("DMM/calc-expression").toString().isEmpty())
      row.device = "= " + keys.value("DMM/calc-expression").toString();
    else if (model != QLatin1String("Manual"))
      row.device = model;
    // its reading as the old instances dialog showed it
    if (readings.contains(id) && row.running)
    {
      const SharedStateManager::Reading &r = readings[id];
      if (r.valid)
      {
        QString prefix;
        const QString value = SiPrefix::format(r.value, &prefix);
        row.value = QString("%1 %2%3 %4").arg(value.left(8), prefix, r.unit,
                                              couplingText(PortKey::fromString(r.port).defining)).trimmed();
      }
      else
        row.value = QStringLiteral("OL");
      row.active = now - r.msecs < 3000;
    }
    else if (!row.running)
      row.value = tr("stopped");
    rows << row;
  }
  m_sidebar->setInstances(rows, own);
}

void MainWindow::openInstance(const QString &id)
{
  if (m_stateMgr->instances().contains(id))
  {
    m_stateMgr->writeState("RAISE_" + id);
    return;
  }
  QStringList args;
  if (id != QLatin1String("default"))
    args << "--config-id" << id;
  if (!m_wid->configPath().isEmpty())
    args << "--config-dir" << m_wid->configPath();
  QProcess::startDetached(QCoreApplication::applicationFilePath(), args);
  statusBar()->showMessage(tr("The instance %1 starts").arg(id), 4000);
}

void MainWindow::renameInstance(const QString &from, const QString &to)
{
  QString error;
  const QStringList changed = Instances::rename(*m_wid->settings(), m_wid->devices(), from, to, &error);
  if (!error.isEmpty())
  {
    QMessageBox::warning(this, tr("Rename instance"), error);
    return;
  }
  m_stateMgr->writeState("UPDATE_INSTANCES_" + QString::number(QDateTime::currentMSecsSinceEpoch()));
  updateInstances();
  // the running ones still compute with the formula they started with
  QStringList runningChanged;
  for (const QString &id : changed)
    if (m_stateMgr->instances().contains(id))
      runningChanged << id;
  if (!runningChanged.isEmpty())
    QMessageBox::information(this, tr("Rename instance"),
                             tr("The formulas now use \"%1\". The running instances %2 use it after a restart.")
                               .arg(to, runningChanged.join(", ")));
}

void MainWindow::deleteInstance(const QString &id)
{
  if (m_stateMgr->instances().contains(id) || id == QLatin1String("default"))
    return;
  m_wid->settings()->deleteConfig(id);
  m_stateMgr->writeState("UPDATE_INSTANCES_" + QString::number(QDateTime::currentMSecsSinceEpoch()));
  updateInstances();
}

void MainWindow::deviceSettings(const QString &id)
{
  const std::optional<MyDevice> device = m_wid->devices()->find(id);
  if (!device)
    return;
  DeviceSettingsDlg dlg(device->name, this);
  dlg.settings()->setPorts(Transport::availablePorts() + m_wid->settings()->customPorts());
  dlg.settings()->setSigrokExe(m_wid->settings()->getString("Port settings/sigrok_exe", "sigrok-cli"));
  dlg.settings()->setStateManager(m_stateMgr);
  dlg.load(device->keys);
  if (dlg.exec() != QDialog::Accepted)
    return;
  QVariantMap keys = dlg.keys();
  keys.insert("Port settings/device", SerialDevice::stableDevice(keys.value("Port settings/device").toString()));
  if (DeviceLibrary::entryKeys(keys) == device->keys)
    return;   // nothing changed: the meter stays connected, the recording too
  // the device in use takes them at once; its unsaved readings first
  const bool inUse = id == m_wid->currentDevice();
  if (inUse && !m_wid->confirmSwitch(device->name))
    return;
  m_wid->devices()->update(id, keys);
  if (inUse)
    m_wid->switchDevice(id, false);
}

void MainWindow::openInNewWindow(const QString &id)
{
  const std::optional<MyDevice> device = m_wid->devices()->find(id);
  if (!device)
    return;
  // one port, one reader: the window that has it comes to the front
  const QString owner = placeOwners().value(DeviceLibrary::place(device->keys));
  if (owner == m_stateMgr->id())
  {
    statusBar()->showMessage(tr("%1 is in use in this window").arg(device->name), 4000);
    return;
  }
  if (!owner.isEmpty())
  {
    m_stateMgr->writeState("RAISE_" + owner);
    statusBar()->showMessage(tr("%1 is in use by the instance %2").arg(device->name, owner), 4000);
    return;
  }
  const QString instance = m_wid->openInNewWindow(id);
  m_stateMgr->writeState("UPDATE_INSTANCES_" + QString::number(QDateTime::currentMSecsSinceEpoch()));
  statusBar()->showMessage(tr("%1 opens in the new window %2").arg(device->name, instance), 4000);
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
  m_sidebar->setCurrent(m_wid->currentDevice(), m_ledActive);
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
    for (QMdiSubWindow *w : { m_displayWin, m_meterWin, m_graphWin, m_readingsWin, m_poincareWin })
      geometry[w->objectName()] = get("Windows/geometry-" + w->objectName(), QString()).toString();
    // once the window is shown and the area has its size: the automatic
    // layout for a start, then the stored positions on top of it
    QTimer::singleShot(0, this, [this, geometry]
    {
      m_arranger->arrangeNow();
      for (QMdiSubWindow *w : { m_displayWin, m_meterWin, m_graphWin, m_readingsWin, m_poincareWin })
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
  m_poincareAction->setChecked(get("Windows/poincare", false).toBool());
  // an unchecked action did not toggle: hide its window explicitly
  for (QAction *a : { m_displayAction, m_meterAction, action_Graph, m_readingsAction, m_poincareAction })
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
    for (QMdiSubWindow *w : { m_displayWin, m_meterWin, m_graphWin, m_readingsWin, m_poincareWin })
    {
      const QRect g = w->geometry();
      set("Windows/geometry-" + w->objectName(),
          QString("%1 %2 %3 %4").arg(g.x()).arg(g.y()).arg(g.width()).arg(g.height()));
    }
  set("Display/show", m_displayAction->isChecked());
  set("Windows/meter", m_meterAction->isChecked());
  set("Windows/readings", m_readingsAction->isChecked());
  set("Windows/poincare", m_poincareAction->isChecked());
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
  if (win == m_readingsWin || win == m_poincareWin)
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
  // the same for the readings table and the Poincaré plot in the column
  if ((win == m_readingsWin || win == m_poincareWin)
      && m_grown.contains(win == m_readingsWin ? m_poincareWin : m_readingsWin))
    return;
  const QSize before = size();
  autoGrow(growthFor(win));
  if (win == m_readingsWin || win == m_poincareWin)
    m_arranger->setTableWidth(size().width() - before.width());
}

void MainWindow::autoGrow(const QSize &delta)
{
  if (m_userSized || !m_growEnabled)
    return;
  // maximized: the room is kept for when the window comes back
  if (isMaximized() || isFullScreen())
  {
    m_pendingGrow += delta;
    return;
  }
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
  if (m_growEnabled && !m_unmaximizing && event->size() != m_expectSize && event->oldSize().isValid()
      && !isMaximized() && !isFullScreen())
    m_userSized = true;
}

void MainWindow::changeEvent(QEvent *event)
{
  QMainWindow::changeEvent(event);
  if (event->type() != QEvent::WindowStateChange)
    return;
  // maximized is no size chosen: back from it, the window grows once for
  // the views shown meanwhile
  const bool full = isMaximized() || isFullScreen();
  if (!full && m_wasFull)
  {
    m_unmaximizing = true;
    // the window manager restores the size in steps; what it settles on is
    // the size the window had
    QTimer::singleShot(300, this, [this]
    {
      m_unmaximizing = false;
      m_expectSize = size();
      const QSize pending = m_pendingGrow;
      m_pendingGrow = QSize(0, 0);
      if (!pending.isNull() && !isMaximized() && !isFullScreen())
        autoGrow(pending);
    });
  }
  m_wasFull = full;
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
  QPalette empty = m_emptyStart->palette();
  empty.setBrush(QPalette::Window, Designs::areaBrush(d));
  m_emptyStart->setPalette(empty);
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
  m_poincare->setThemeColors(g.background, g.grid, g.labels, g.data);
  for (QAction *a : m_designMenu->actions())
    a->setChecked(a->data().toInt() == design);
  // at once, so the settings dialog shows the design of the menu
  m_wid->settings()->setString("Windows/design", Designs::name(d));
}

void MainWindow::storeDisplaySLOT()
{
  m_wid->setShowDisplay(m_displayAction->isChecked());
}

void MainWindow::setConnectSLOT(bool on)
{
  action_Connect->setChecked(on);
}

void MainWindow::showDisplaySLOT(bool show)
{
  m_displayAction->setChecked(show);
}

QMenu *MainWindow::createPopupMenu()
{
  // the toolbars are always there: no menu to hide them
  return nullptr;
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
