//======================================================================
// File:		mainwin.cpp
// Author:	Matthias Toussaint
// Created:	Sun Sep  2 12:15:28 CEST 2001
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

#include <QtGui>
#include <QtWidgets>
#include <QFrame>
#include <QTimer>
#include <QMenu>
#include <cmath>

#include "ui/mainwindow.h"
#include "ui/frameclock.h"
#include "ui/meterview.h"
#include "ui/poincareview.h"
#include "ui/dialogs/helpdlg.h"
#include "ui/dialogs/devicesettingsdlg.h"
#include "ui/devicesettings.h"
#include "ui/devicesidebar.h"
#include "ui/valuemenu.h"
#include "ui/tilebutton.h"
#include "ui/dialogs/adddevicedlg.h"
#include "ui/dialogs/limitdlg.h"
#include "device/transport.h"
#include "device/transports/serial.h"
#include "core/projectfolder.h"
#include "core/devicelibrary.h"
#include "core/sampletypes.h"
#include "core/siprefix.h"
#include "recording/labplotlive.h"
#include "recording/recordingfile.h"
#include "recording/recordingstore.h"
#include "ui/instancewidget.h"
#include "ui/views/graphwidget.h"
#include "ui/views/lcdwidget.h"
#include "ui/views/poincareplot.h"
#include "ui/settings/settingsdialog.h"
#include "ui/settings/alarmprefs.h"
#include "ui/settings/generalprefs.h"
#include "ui/settings/guiprefs.h"
#include "ui/settings/keyboardprefs.h"
#include "ui/views/analogmeter.h"
#include "ui/views/basketwidget.h"

static const char *kWorkspaceSuffix = "qtdmm-workspace";
#include "ui/views/readingswidget.h"
#include "ui/dialogs/logdlg.h"
#include "ui/dialogs/panelvaluesdlg.h"
#include "ui/dialogs/sourcesdlg.h"
#include "core/settings.h"
#include "core/alarm.h"
#include "ui/alarmbar.h"
#include "ui/alarmservice.h"
#include "ui/mdiarranger.h"
#include "service/metercontroller.h"
#include "device/meterconnection.h"
#include "ui/designs.h"
#include "ui/controlbar.h"
#include "ui/frontpanel.h"
#include "ui/shortcuts.h"
#include "service/devicesession.h"
#include "core/workspacefile.h"
#include "ui/dialogs/exportdlg.h"
#include "ui/labplot.h"
#include "ui/dialogs/workspacedlg.h"
#include <QBuffer>
#include <QMdiArea>
#include <QRegularExpression>
#include <QStandardPaths>
#include <QMdiSubWindow>
#include <QLoggingCategory>
#include <QIconEngine>

namespace
{
// An icon drawn by its paint(): the pixmap is the same drawing.
class PaintedIconEngine : public QIconEngine
{
public:
  QPixmap pixmap(const QSize &size, QIcon::Mode mode, QIcon::State state) override
  {
    QPixmap pm(size);
    pm.fill(Qt::transparent);
    QPainter painter(&pm);
    paint(&painter, QRect(QPoint(0, 0), size), mode, state);
    return pm;
  }
};

// The Record button's symbol, drawn at any size: a grey dot to start, a red
// square while it records
class RecordIconEngine : public PaintedIconEngine
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
  QIconEngine *clone() const override { return new RecordIconEngine(m_stop); }

private:
  bool m_stop;
};

/// A theme symbol with a "+" in its lower right corner (Add device: the
/// meter and "add"), looked up when painted, so it follows the symbol set.
class PlusIconEngine : public PaintedIconEngine
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
  QIconEngine *clone() const override { return new PlusIconEngine(m_name); }

private:
  QString m_name;
};
}

MainWindow::MainWindow(QCommandLineParser &parser, QWidget *parent)
  : QMainWindow(parent)
  , m_running(false)
  , m_menu(nullptr)
  , m_helpDlg(nullptr)
{
  setupUi(this);
  setupIcons();
  m_configId = parser.value("config-id");

  QWidget* spacer = new QWidget();
  spacer->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
  this->toolBarMenu->addWidget(spacer);
  this->toolBarMenu->addAction(this->action_Menu);
  // the meters of the process share the network services; the window has
  // none of its own (Connect, --connect, SCPI QTDMM:DEVice:OPEN)
  m_session = new DeviceSession(this);
  m_session->setInstanceId(m_configId.isEmpty() ? QString("default") : m_configId);
  m_wid = new InstanceWidget(m_configId, parser.value("config-dir"), m_session, this);
  m_session->setConfigDir(m_wid->settings()->configDir());
  m_session->setSigrokExe(m_wid->settings()->getString("Port settings/sigrok_exe", "sigrok-cli"));
  // the window's meter before 26.4: one of My devices now, connected first
  m_oldDevice = m_wid->takeOldDevice();
  m_wid->setFrameShape(QFrame::NoFrame);
  setConsoleLogging(parser.isSet("debug"));
  // one device: its frames only, without what the other categories print
  if (parser.isSet("debug-device"))
    m_wid->setConsoleLogging(true, parser.values("debug-device"));

  createActions();

  createViews();
  createSidebar();
  fillViewToolbar();
  const QList<QAction *> workspace = createArrangeMenu();
  updateRecentWorkspaces();
  setCurrentWorkspace(m_wid->settings()->getString("Windows/workspace-current"));
  createDesignMenu();

  updateWindowTitle();
  connect(m_session, &DeviceSession::opened, this, &MainWindow::updateWindowTitle);
  connect(m_session, &DeviceSession::closed, this, &MainWindow::updateWindowTitle);
  // the design can be chosen in the settings (General), too
  connect(m_wid, &InstanceWidget::configChanged, this, [this]
  {
    const Designs::Design d = Designs::fromName(m_wid->settings()->getString("Windows/design", "dark-metal"));
    if (d != Designs::current())
      setDesign(d);
  });

  createExtraActions();
  createShortcuts(workspace);

  // the recording is the session's, whichever meter the graph shows
  connect(m_session, &DeviceSession::recordingStateChanged, this, [this]
  {
    if (m_session->isRecording() != m_running)
      runningSLOT(m_session->isRecording());
    else
      updateRecorderActions();
  });
  connect(m_wid, &InstanceWidget::recorderState, this, &MainWindow::updateRecorderActions);
  // a file loaded without a device: the start page makes way for its graph
  connect(m_wid, &InstanceWidget::recorderState, this, &MainWindow::updateEmptyStart);
  // with or without a device, the graph shows what was loaded
  connect(m_wid->graph(), &GraphWidget::fileImported, action_Graph, [this] { action_Graph->setChecked(true); });

  // Record while a meter is connected
  auto recordable = [this] { connectSLOT(!m_session->controllers().isEmpty()); };
  recordable();
  connect(m_session, &DeviceSession::opened, this, recordable);
  connect(m_session, &DeviceSession::closed, this, recordable);

  createStatusBar();
  placeWindow();

  // the meters connected last time
  QTimer::singleShot(1000, this, &MainWindow::openPending);
  // --connect <id> (--also before 26.4): more of "My devices", after those of last time
  QTimer::singleShot(1000, this, [this, ids = parser.values("connect") + parser.values("also")]
  {
    // one by one like the workspace's: a view of the next stays with it
    const QScopedValueRollback<QStringList> opening(m_openingPending, ids);
    for (const QString &id : ids)
    {
      QString error;
      if (!m_session->controller("device:" + id) && !m_session->open(id, &error))
      {
        qWarning().noquote() << "--connect" << id << ":" << error.section(',', 1);
        // one that does not connect said why in the status bar already
        if (!error.startsWith("-240,"))
          m_error->setText(tr("--connect %1: %2").arg(id, error.section(',', 1)));
      }
    }
  });
  updateEmptyStart();
  connect(m_session, &DeviceSession::opened, this, &MainWindow::updateEmptyStart);
  connect(m_session, &DeviceSession::closed, this, &MainWindow::updateEmptyStart);
  connect(m_wid->devices(), &DeviceLibrary::changed, this, &MainWindow::updateEmptyStart);
  connect(m_wid, &InstanceWidget::configChanged, this, [this]
  {
    for (QMdiSubWindow *win : std::as_const(m_panelWins))
      applyLcdLook(panelOf(win));
  });
  connect(m_wid, &InstanceWidget::configChanged, this, &MainWindow::updateRecentWorkspaces);
  // the sidebar as left, with two devices or more
  const bool sidebarSet = !m_wid->settings()->getString("Windows/sidebar").isEmpty();
  showSidebar(DeviceSidebar::shownAtStart(sidebarSet ? QVariant(m_wid->settings()->getBool("Windows/sidebar"))
                                                     : QVariant(),
                                          int(m_wid->devices()->list().size())));
  updateServices();   // the servers may have started before this window listened
}

// The MDI area with the alarm banner, the empty start, the views and an action per view.
void MainWindow::createViews()
{
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
  // with entries in My devices: one of them, from the sidebar
  m_emptyConnect = new QPushButton(tr("&Connect a device from My devices"), m_emptyStart);
  m_emptyConnect->setObjectName("ui_emptyConnect");
  m_emptyConnect->setIcon(QIcon::fromTheme("network-connect"));
  emptyLayout->addWidget(m_emptyConnect, 1, 0, Qt::AlignHCenter | Qt::AlignTop);
  emptyLayout->setRowStretch(0, 1);
  emptyLayout->setRowStretch(1, 1);
  // the entries to choose one from, connected at once; the sidebar opens
  // with them, where the others stay at hand
  connect(m_emptyConnect, &QPushButton::clicked, this, [this]
  {
    showSidebar(true);
    m_wid->settings()->setBool("Windows/sidebar", true);
    m_sidebar->devicesNode()->setExpanded(true);
    QMenu menu(this);
    for (const MyDevice &d : m_wid->devices()->list())
    {
      const QString id = d.id;
      QTreeWidgetItem *item = m_sidebar->deviceItem(id);
      menu.addAction(item ? item->icon(0) : QIcon(), QString(d.name).replace('&', "&&"), this, [this, id] { openBeside(id); });
    }
    menu.exec(m_emptyConnect->mapToGlobal(QPoint(0, m_emptyConnect->height())));
  });
  m_emptyStart->hide();
  m_mdi->installEventFilter(this);
  // a value or an entry of My devices dragged out of the sidebar
  m_mdi->viewport()->setAcceptDrops(true);
  m_mdi->viewport()->installEventFilter(this);
  connect(addButton, &QToolButton::clicked, this, &MainWindow::addDevice);

  // the alarms of every open meter: sounds, popups, one banner
  m_alarmService = new AlarmService(m_session, this, this);
  connect(m_alarmService, &AlarmService::bannerChanged, m_alarmBar, &AlarmBar::setAlarms);
  connect(m_alarmBar, &AlarmBar::acknowledged, m_alarmService, &AlarmService::acknowledgeAll);

  // the views; none of them knows another, they show the values of the
  // meters they are given (see InstanceWidget)

  m_meter = new AnalogMeter(this);
  m_meter->setWindowTitle(tr("Needle meter"));
  m_wid->addMeter(m_meter);
  m_meterWin = addView(m_meter, m_meter->windowTitle(), MdiArranger::Instrument, "meter");
  // no cell bigger than the instrument can fill
  m_arranger->setContentAspect(m_meterWin, AnalogMeter::kMinAspect, AnalogMeter::kMaxAspect);

  m_graphWin = addView(m_wid, tr("Graph"), MdiArranger::Graph, "graph");
  // the drawing clock of the views; 0 = off, the graph moves with each reading only
  FrameClock::instance()->setRate(m_wid->settings()->getInt("Graph/frame-rate", 30));

  m_readings = new ReadingsWidget(this);
  m_wid->setReadingLog(m_readings->log());   // the row limit is the store's: set it after
  m_readings->setMaxRows(m_wid->settings()->getInt("ReadingLog/max-rows", 10000));
  m_readingsWin = addView(m_readings, tr("Readings"), MdiArranger::Table, "readings");

  // the scatter of the main value: every reading, also while hidden
  m_poincare = new PoincarePlot(this);
  m_poincare->setWindowTitle(tr("Poincaré plot"));
  m_poincare->setSettings(m_wid->settings());
  m_poincareWin = addView(m_poincare, m_poincare->windowTitle(), MdiArranger::Table, "poincare");

  // the files of a project, a folder; it stays open until another is
  m_basket = new BasketWidget(this);
  m_basketWin = addView(m_basket, m_basket->windowTitle(), MdiArranger::Table, "basket");
  const QString basketDir = m_wid->settings()->getString("Basket/folder");
  if (QFileInfo(basketDir).isDir())
  {
    m_basket->setFolder(basketDir);
    recentProjects().add(basketDir);   // the one open at the start is one of the last ones too
  }
  auto basketBase = [this]
  {
    const QString base = m_wid->settings()->getString("Basket/base-dir");
    m_basket->setBaseFolder(base.isEmpty() ? GeneralPrefs::defaultBasketBase() : base);
  };
  basketBase();
  connect(m_wid, &InstanceWidget::configChanged, this, basketBase);
  connect(m_basket, &BasketWidget::folderChanged, this, [this](const QString &folder)
  {
    m_wid->settings()->setString("Basket/folder", folder);
    m_wid->settings()->save();   // set only stages it: the next start reads the file
    if (!folder.isEmpty())
      recentProjects().add(folder);
    updateWindowTitle();
    // without a basket everything is as before
    for (QAction *a : { m_graphToBasket, m_tableToBasket })
      a->setVisible(!folder.isEmpty());
  });
  connect(m_basket, &BasketWidget::error, m_wid, &InstanceWidget::error);
  connect(m_basket, &BasketWidget::openRequested, this, [this](const QString &path)
  {
    // a bench is loaded, a recording of QtDMM goes into the graph, anything
    // else to its program
    const QString suffix = QFileInfo(path).suffix();
    if (suffix == kWorkspaceSuffix)
      loadWorkspaceFile(path);
    else if ((suffix.compare("csv", Qt::CaseInsensitive) == 0 || suffix == RecordingFile::kSuffix)
             && RecordingFile::read(path).has_value())
    {
      if (m_wid->importFile(path))
        action_Graph->setChecked(true);
    }
    else
      m_basket->openWithSystem(path);
  });
  m_wid->addPoincare(m_poincare);

  // right-click on the meter or the plot: the window's menu (also without title bar)
  for (auto [view, win] : { std::pair<QWidget *, QMdiSubWindow *>{m_meter, m_meterWin},
                            std::pair<QWidget *, QMdiSubWindow *>{m_poincare, m_poincareWin} })
  {
    view->setContextMenuPolicy(Qt::CustomContextMenu);
    connect(view, &QWidget::customContextMenuRequested, this, [this, view = view, w = win](const QPoint &pos)
    {
      windowMenu(w, view->mapToGlobal(pos));
    });
  }

  // one checkable action per window: toolbar buttons, menu entries,
  // shortcuts; LCD for the front panels of all meters
  m_displayAction = new QAction(QIcon::fromTheme("qtdmm-display"), tr("LC&D"), this);
  m_displayAction->setShortcut(QKeySequence("Ctrl+1"));
  m_displayAction->setCheckable(true);
  m_displayAction->setWhatsThis(
    tr("<html><head/><body><p><span style=\" font-weight:600;\">LCD</span></p>"
       "<p>Show or hide the front panels of all devices connected: the LCD-style digital display, with "
       "the keys of meters that have them.</p></body></html>"));
  connect(m_displayAction, &QAction::toggled, this, [this](bool on)
  {
    if (m_restoring)
      return;
    for (QMdiSubWindow *win : std::as_const(m_panelWins))
    {
      win->setVisible(on);
      if (on)
        growFor(win);
    }
    m_arranger->arrange();
  });
  m_meterAction = windowAction(m_meterWin, tr("Needle &meter"), "Ctrl+2", "qtdmm-meter",
    tr("<html><head/><body><p><span style=\" font-weight:600;\">Needle meter</span></p>"
       "<p>Show the reading on a moving-coil style instrument.</p></body></html>"));
  m_readingsAction = windowAction(m_readingsWin, tr("&Readings table"), "Ctrl+4", "table",
    tr("<html><head/><body><p><span style=\" font-weight:600;\">Readings table</span></p>"
       "<p>Every reading the meter sent, one row each, with time, mode and range - "
       "the raw protocol of the session next to the graph. Copy rows to a "
       "spreadsheet or export them as CSV.</p></body></html>"));
  m_newMeterAction = new QAction(QIcon::fromTheme("qtdmm-meter"), tr("&Add needle meter"), this);
  m_newMeterAction->setObjectName("action_NewMeter");
  m_newMeterAction->setWhatsThis(
    tr("<html><head/><body><p><span style=\" font-weight:600;\">Add needle meter</span></p>"
       "<p>Another needle meter window: pick its value first, later with Sources in its window "
       "menu; closing it removes it.</p></body></html>"));
  connect(m_newMeterAction, &QAction::triggered, this, [this]
  {
    const MeterController *ctl = m_session->controllers().value(0);
    newView(Added::Meter, ctl ? ValueRef(0, ctl->source()) : ValueRef(), true);
  });
  m_newPoincareAction = new QAction(QIcon::fromTheme("qtdmm-poincare"), tr("Add Po&incaré plot"), this);
  m_newPoincareAction->setObjectName("action_NewPoincare");
  m_newPoincareAction->setWhatsThis(
    tr("<html><head/><body><p><span style=\" font-weight:600;\">Add Poincaré plot</span></p>"
       "<p>Another Poincaré plot window: pick its value first, later with Sources in its window "
       "menu; closing it removes it.</p></body></html>"));
  connect(m_newPoincareAction, &QAction::triggered, this, [this]
  {
    const MeterController *ctl = m_session->controllers().value(0);
    newView(Added::Poincare, ctl ? ValueRef(0, ctl->source()) : ValueRef(), true);
  });
  m_basketAction = windowAction(m_basketWin, tr("Pro&ject"), "Ctrl+6", "document-open",
    tr("<html><head/><body><p><span style=\" font-weight:600;\">Project</span></p>"
       "<p>The files of a project: its benches, recordings, pictures, PDFs, LabPlot projects. "
       "A folder; drop files in to copy them there.</p></body></html>"));
  if (m_basketAction->icon().isNull())
    m_basketAction->setIcon(style()->standardIcon(QStyle::SP_DirIcon));
  m_poincareAction = windowAction(m_poincareWin, tr("Poi&ncaré plot"), "Ctrl+5", "qtdmm-poincare",
    tr("<html><head/><body><p><span style=\" font-weight:600;\">Poincaré plot</span></p>"
       "<p>Each reading against the next one: noise widens the cloud across the diagonal, "
       "drift stretches it along it. SD1 and SD2 put both in numbers.</p></body></html>"));
  // the graph keeps its action from the .ui (toolbar button with Ctrl+3)
  bindWindowAction(action_Graph, m_graphWin);
  // graph and table have menus of their own: they begin with the window's
  // entry, as the window menu of the others
  auto hideWindow = [this](QMdiSubWindow *win)
  {
    QAction *hide = new QAction(tr("&Hide view"), this);
    connect(hide, &QAction::triggered, win, &QWidget::hide);
    return hide;
  };
  // the values of the open meters they show, with more than one
  m_graphSources = new QAction(tr("S&ources..."), this);
  m_tableSources = new QAction(tr("S&ources..."), this);
  connect(m_graphSources, &QAction::triggered, this, [this] { editSources(ValueMenu::View::Graph); });
  connect(m_tableSources, &QAction::triggered, this, [this] { editSources(ValueMenu::View::Table); });
  auto sourcesShown = [this]
  {
    const bool several = m_session->controllers().size() > 1;
    m_graphSources->setVisible(several);
    m_tableSources->setVisible(several);
  };
  sourcesShown();
  connect(m_session, &DeviceSession::opened, this, sourcesShown);
  connect(m_session, &DeviceSession::closed, this, sourcesShown);
  // what the view shows, as files into the open project basket
  m_graphToBasket = new QAction(tr("Into the pro&ject"), this);
  m_graphToBasket->setToolTip(tr("The recording as CSV and the graph as a picture, into the project"));
  connect(m_graphToBasket, &QAction::triggered, this, &MainWindow::graphToBasket);
  m_tableToBasket = new QAction(tr("Into the pro&ject"), this);
  m_tableToBasket->setToolTip(tr("The readings as CSV, into the project"));
  connect(m_tableToBasket, &QAction::triggered, this, &MainWindow::tableToBasket);
  for (QAction *a : { m_graphToBasket, m_tableToBasket })
    a->setVisible(!m_basket->folder().isEmpty());
  // what, which format, where to, in one dialog
  m_graphExport = new QAction(tr("E&xport..."), this);
  m_graphExport->setObjectName("action_ExportDialog");
  m_graphExport->setShortcut(QKeySequence("Ctrl+E"));
  m_graphExport->setToolTip(tr("Export the recording, the readings or a picture: format and place in a dialog"));
  connect(m_graphExport, &QAction::triggered, this, [this] { exportDialog(ExportDlg::Recording); });
  m_labPlotLive = new QAction(tr("Live in &LabPlot"), this);
  m_labPlotLive->setObjectName("action_LabPlotLive");
  m_labPlotLive->setToolTip(tr("LabPlot follows the recording while it goes on: its graph grows with it"));
  m_labPlotLive->setEnabled(false);   // while it records (updateRecorderActions())
  connect(m_labPlotLive, &QAction::triggered, this, &MainWindow::labPlotLive);
  m_tableExport = new QAction(m_graphExport->text(), this);
  connect(m_tableExport, &QAction::triggered, this, [this] { exportDialog(ExportDlg::Table); });
  m_wid->graph()->setWindowActions(
    { hideWindow(m_graphWin), m_graphSources, m_graphToBasket, m_graphExport, m_labPlotLive });
  m_wid->graph()->setPrintAction(action_Print);
  m_readings->setWindowActions({ hideWindow(m_readingsWin), m_tableSources, m_tableToBasket, m_tableExport });
  m_displayAction->setObjectName("action_WindowDisplay");
  m_meterAction->setObjectName("action_WindowMeter");
  m_readingsAction->setObjectName("action_WindowReadings");
  m_poincareAction->setObjectName("action_WindowPoincare");
  m_basketAction->setObjectName("action_WindowBasket");

  // a front panel for each meter, also those opened before the window
  connect(m_session, &DeviceSession::opened, this, &MainWindow::addPanel);
  // a meter of the table opened again: its readings come in again
  // an empty view takes the meter connected: the graph its main value, the
  // analog meter and the Poincaré plot it when theirs is not connected -
  // while the workspace's meters open one by one only when none was chosen
  connect(m_session, &DeviceSession::opened, this, [this](MeterController *ctl)
  {
    auto free = [this](const ValueRef &v)
    { return !m_session->controller(v) && !m_openingPending.contains(v.source.section(':', 1)); };
    if (m_wid->graph()->values().isEmpty())
      m_wid->graph()->setValues({ ValueRef(0, ctl->source()) });
    for (MeterView *meter : m_wid->meters())
      if (free(meter->value()))
        meter->setValue(ValueRef(0, ctl->source()));
    for (PoincareView *plot : m_wid->poincares())
      if (free(plot->value()))
        plot->setValue(ValueRef(0, ctl->source()));
  });
  // a meter opened or closed: in the table when it shows all, else only its readings to it
  auto tableFollows = [this]
  {
    if (m_tableMeters.isEmpty())
      applyTableSources();
    else
      connectTableSources();
  };
  connect(m_session, &DeviceSession::opened, this, tableFollows);
  connect(m_session, &DeviceSession::closed, this, tableFollows);
  connect(m_session, &DeviceSession::closed, this, &MainWindow::removePanel);
  for (MeterController *ctl : m_session->controllers())
    addPanel(ctl);
}

void MainWindow::addPanel(MeterController *ctl)
{
  if (m_panelWins.contains(ctl->source()))
    return;
  const QString id = ctl->deviceId();
  auto *panel = new FrontPanel(m_session, this);
  panel->display()->installEventFilter(this);   // a click: the active device
  panel->setValue(ValueRef(0, ctl->source()));
  applyLcdLook(panel);
  applyFrame(panel);
  QMdiSubWindow *win = addView(panel, ctl->deviceName(), MdiArranger::Instrument, "panel-" + id);
  m_panelWins.insert(ctl->source(), win);
  connect(panel, &FrontPanel::foldToggled, this, [this] { updateControls(); });
  connect(panel, &FrontPanel::keysChanged, this, [this] { updateControls(); });
  connect(panel, &FrontPanel::keyFailed, this, [this, panel]
  {
    statusBar()->showMessage(tr("%1: the key did not reach the meter: it is not connected yet.")
                             .arg(panel->controller() ? panel->controller()->deviceName() : QString()), 4000);
  });
  panel->display()->setContextMenuPolicy(Qt::CustomContextMenu);
  connect(panel->display(), &QWidget::customContextMenuRequested, this, [this, panel, win](const QPoint &pos)
  {
    // not from inside the display's event: its device may close while the
    // menu is open, and the panel with it
    const QPoint at = panel->display()->mapToGlobal(pos);
    QTimer::singleShot(0, this, [this, at, w = QPointer<QMdiSubWindow>(win)]
    {
      if (w)
        windowMenu(w, at);
    });
  });
  PanelState state = m_panelStates.take(id);
  state.shown = state.shown && m_displayAction->isChecked();
  applyPanelState(win, state);
  if (state.shown && !m_restoring)
    growFor(win);
}

void MainWindow::applyPanelState(QMdiSubWindow *win, const PanelState &state)
{
  FrontPanel *panel = panelOf(win);
  MeterController *ctl = panel->controller();
  if (ctl)
  {
    panel->setValue(ValueRef(state.value.id, ctl->source()));
    ctl->setMinMaxValue(state.value.id);
  }
  if (ctl)
    setPanelLcdVariant(ctl->source(), state.lcdVariant);
  panel->setFolded(state.folded);
  if (state.secondaries)
    panel->setSecondaries(state.secondaries);
  const QStringList g = state.geometry.split(' ');
  if (m_arranger->mode() == MdiArranger::Free && g.size() == 4)
    win->setGeometry(g[0].toInt(), g[1].toInt(), g[2].toInt(), g[3].toInt());
  win->setVisible(state.shown);
  updateControls();
}

void MainWindow::removePanel(const QString &source)
{
  m_panelLcd.remove(source);   // connected again: the settings' colours
  QMdiSubWindow *win = m_panelWins.take(source);
  if (!win)
    return;
  m_grown.remove(win->widget());
  m_grown.remove(win);
  delete win;
  m_arranger->arrange();
}

QMdiSubWindow *MainWindow::addViewWindow(Added kind, const ValueRef &value, int number)
{
  const QString prefix = kind == Added::Meter ? QStringLiteral("meter-") : QStringLiteral("poincare-");
  if (number < 2)
  {
    QStringList taken;
    for (const QMdiSubWindow *w : std::as_const(m_added))
      taken << w->objectName();
    for (number = 2; taken.contains(prefix + QString::number(number)); ++number) {}
  }
  QWidget *view;
  QMdiSubWindow *win;
  if (kind == Added::Meter)
  {
    auto *meter = new AnalogMeter(this);
    meter->setWindowTitle(tr("Needle meter %1").arg(number));
    m_wid->addMeter(meter)->setValue(value);
    win = addView(meter, meter->windowTitle(), MdiArranger::Instrument, prefix + QString::number(number));
    m_arranger->setContentAspect(win, AnalogMeter::kMinAspect, AnalogMeter::kMaxAspect);
    applyFrame(meter);
    view = meter;
  }
  else
  {
    auto *plot = new PoincarePlot(this);
    plot->setWindowTitle(tr("Poincaré plot %1").arg(number));
    plot->setSettings(m_wid->settings());
    m_wid->addPoincare(plot)->setValue(value);
    win = addView(plot, plot->windowTitle(), MdiArranger::Table, prefix + QString::number(number));
    applyPoincareColors(plot);
    view = plot;
  }
  view->setContextMenuPolicy(Qt::CustomContextMenu);
  connect(view, &QWidget::customContextMenuRequested, this, [this, view, win](const QPoint &pos)
  {
    windowMenu(win, view->mapToGlobal(pos));
  });
  m_added << win;
  return win;
}

void MainWindow::newView(Added kind, const ValueRef &value, bool ask)
{
  QMdiSubWindow *win = addViewWindow(kind, value);
  // from the menu its value is asked first (the active device's first one
  // ticked); cancelled, there is no new view. With no device open there is
  // nothing to pick from.
  if (ask && !m_session->controllers().isEmpty())
  {
    MeterView *meter = meterViewOf(win);
    PoincareView *plot = poincareViewOf(win);
    if (!editSources(meter ? ValueMenu::View::Meter : ValueMenu::View::Poincare, meter, plot))
    {
      removeAddedWindow(win);
      return;
    }
  }
  win->show();
  m_mdi->setActiveSubWindow(win);
  growFor(win);
  m_arranger->arrange();
}

void MainWindow::removeAddedWindow(QMdiSubWindow *win)
{
  if (!m_added.removeOne(win))
    return;
  if (MeterView *meter = meterViewOf(win))
    m_wid->removeMeter(meter);
  if (PoincareView *plot = poincareViewOf(win))
    m_wid->removePoincare(plot);
  m_grown.remove(win->widget());
  m_grown.remove(win);
  delete win;
  m_arranger->arrange();
}

MeterView *MainWindow::meterViewOf(const QMdiSubWindow *win) const
{
  for (MeterView *view : m_wid->meters())
    if (win && view->meter() == win->widget())
      return view;
  return nullptr;
}

PoincareView *MainWindow::poincareViewOf(const QMdiSubWindow *win) const
{
  for (PoincareView *view : m_wid->poincares())
    if (win && view->plot() == win->widget())
      return view;
  return nullptr;
}

QList<QMdiSubWindow *> MainWindow::meterWindows() const
{
  QList<QMdiSubWindow *> wins { m_meterWin };
  for (QMdiSubWindow *w : m_added)
    if (meterViewOf(w))
      wins << w;
  return wins;
}

QList<QMdiSubWindow *> MainWindow::poincareWindows() const
{
  QList<QMdiSubWindow *> wins { m_poincareWin };
  for (QMdiSubWindow *w : m_added)
    if (poincareViewOf(w))
      wins << w;
  return wins;
}

bool MainWindow::isColumn(const QMdiSubWindow *win) const
{
  return win == m_readingsWin || poincareViewOf(win);
}

bool MainWindow::isInstrument(const QMdiSubWindow *win) const
{
  return meterViewOf(win) || panelOf(win);
}

FrontPanel *MainWindow::panelOf(const QMdiSubWindow *win)
{
  return win ? qobject_cast<FrontPanel *>(win->widget()) : nullptr;
}

void MainWindow::applyLcdLook(FrontPanel *panel)
{
  panel->setBlink(m_wid->settingsDialog()->alarmPage()->blink());
  const GuiPrefs *gui = m_wid->settingsDialog()->guiPage();
  LcdWidget *lcd = panel->display();
  lcd->setFaceColor(gui->displayBgColor());
  const MeterController *ctl = panel->controller();
  const int variant = m_panelLcd.value(ctl ? ctl->source() : QString(), gui->lcdVariant());
  lcd->setLcdVariant(static_cast<LcdWidget::LcdVariant>(variant));
  if (ctl)
  {
    const DmmDecoder::DMMInfo &info = ctl->dmm()->dmmInfo();
    // the controller's count: the Simulated meter's values are not the model's
    lcd->setDisplayMode(info.display, gui->showMinMax(), gui->showBar(), ctl->valueCount());
    lcd->setLook(LcdWidget::lookOf(DeviceType::of(info)));
  }
}

void MainWindow::setPanelLcdVariant(const QString &source, std::optional<int> variant)
{
  if (variant)
    m_panelLcd.insert(source, *variant);
  else
    m_panelLcd.remove(source);
  if (QMdiSubWindow *win = m_panelWins.value(source))
    applyLcdLook(panelOf(win));
}

bool MainWindow::keepRecordingOf(const QString &id, const QString &text)
{
  const QStringList unsaved = m_session->unsavedRecordings(id);
  if (unsaved.isEmpty())
    return true;
  return QMessageBox::question(this, tr("QtDMM: Unsaved data"), tr("%1: %2").arg(unsaved.first(), text),
                               QMessageBox::Discard | QMessageBox::Cancel, QMessageBox::Cancel)
         == QMessageBox::Discard;
}

void MainWindow::openPending()
{
  const QStringList ids = std::exchange(m_pendingOpen, {});
  {
    const QScopedValueRollback<QStringList> opening(m_openingPending, ids);
    for (const QString &id : ids)
      if (!m_session->controller("device:" + id))
        openBeside(id, true);   // one not plugged in yet: its panel waits for it
  }
  updateEmptyStart();
  // a layout naming panels takes once they are there
  if (!m_pendingLayout.isEmpty() && m_arranger->setLayoutText(m_pendingLayout))
    m_arranger->arrange();
  m_pendingLayout.clear();
}

// The sidebar Devices: My devices, the open devices, the services.
void MainWindow::createSidebar()
{
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
  sidebar->setToolTip(tr("Devices: show or hide the sidebar with My devices and the open devices"));
  addAction(sidebar);   // F9 also with the toolbar hidden
  sidebar->setWhatsThis(tr("<html><head/><body><p><span style=\" font-weight:600;\">Devices</span></p>"
                           "<p>The sidebar with My devices: the context menu of one connects it, has its settings, "
                           "rename and remove; a double click shows its info and settings. Drag an entry to change "
                           "the order."
                           "</p></body></html>"));
  connect(sidebar, &QAction::triggered, this, [this](bool on)
  {
    showSidebar(on);
    m_wid->settings()->setBool("Windows/sidebar", on);
  });
  connect(m_sidebar, &DeviceSidebar::settingsRequested, this, [this](const QString &id) { deviceSettings(id); });
  connect(m_sidebar, &DeviceSidebar::valueMenuRequested, this, &MainWindow::valueMenu);
  connect(m_sidebar, &DeviceSidebar::viewsHighlightRequested, this, &MainWindow::pointToViews);
  // Shift+F8 for the device chosen in the sidebar
  action_ConfigureDMM->setEnabled(false);
  connect(m_sidebar, &QTreeWidget::itemSelectionChanged, this, [this]
          { action_ConfigureDMM->setEnabled(!m_sidebar->chosenDevice().isEmpty()); });
  connect(m_wid->graph(), &GraphWidget::notice, this, [this](const QString &text)
  {
    statusBar()->showMessage(text, 5000);
  });
  connect(m_sidebar, &DeviceSidebar::openBesideRequested, this, [this](const QString &id) { openBeside(id); });
  connect(m_sidebar, &DeviceSidebar::closeRequested, this, [this](const QString &id)
  {
    if (keepRecordingOf(id, tr("Closing it discards its recording, which is not saved.")))
      m_session->close(id);
  });
  connect(m_sidebar, &DeviceSidebar::frontPanelRequested, this, [this](const QString &id)
  {
    QMdiSubWindow *win = m_panelWins.value("device:" + id);
    if (!win)
      return;
    if (win->isHidden())
    {
      win->show();
      growFor(win);
    }
    m_mdi->setActiveSubWindow(win);
    m_arranger->arrange();
  });
  connect(m_sidebar, &DeviceSidebar::connectRequested, this, [this](const QString &id, bool on)
  {
    if (MeterController *ctl = m_session->controller("device:" + id))
    {
      ctl->connectMeter(on);
    }
    updateOpenDevices();
  });
  // an open device by the sidebar's id (OpenDevice::id)
  auto openController = [this](const QString &id)
  {
    return m_session->controller("device:" + id);
  };
  connect(m_sidebar, &DeviceSidebar::alarmsRequested, this, [this, openController](const QString &id)
  {
    if (const MeterController *ctl = openController(id))
      m_wid->openAlarms(ctl->source());
  });
  connect(m_sidebar, &DeviceSidebar::limitsRequested, this, [this, openController](const QString &id)
  {
    if (MeterController *ctl = openController(id))
      editLimits(ctl, false);
  });
  // an alarm of a meter marks the graph while it shows a value of it
  connect(m_session, &DeviceSession::opened, this, [this](MeterController *ctl)
  {
    connect(ctl, &MeterController::markRequested, this, [this, ctl](const QColor &color, const QString &name, bool graph, bool)
    {
      const QList<ValueRef> shown = m_wid->graph()->values();
      if (graph && std::any_of(shown.cbegin(), shown.cend(), [this, ctl](const ValueRef &v)
                               { return m_wid->graph()->resolved(v).source == ctl->source(); }))
        m_wid->graph()->addMark(color, tr("%1: %2").arg(ctl->deviceName(), name));
    });
  });
  connect(m_session, &DeviceSession::opened, this, &MainWindow::updateOpenDevices);
  connect(m_session, &DeviceSession::closed, this, &MainWindow::updateOpenDevices);
  connect(m_sidebar, &DeviceSidebar::addDeviceRequested, this, &MainWindow::addDevice);
  connect(m_sidebar, &DeviceSidebar::serviceSettingsRequested, m_wid, &InstanceWidget::configScpiSLOT);
  connect(m_sidebar, &DeviceSidebar::workspaceRequested, this, &MainWindow::loadWorkspaceFile);
  connect(m_sidebar, &DeviceSidebar::removeWorkspaceRequested, this, [this](const QString &file)
  {
    recentWorkspaces().remove(file);
    updateRecentWorkspaces();
  });
  connect(m_sidebar, &DeviceSidebar::favoriteRequested, this, [this](const QString &file, bool on)
  {
    recentWorkspaces().setFavorite(file, on);
    updateRecentWorkspaces();
  });
  // the readings of the others change all the time
  m_openTimer = new QTimer(this);
  m_openTimer->setInterval(1000);
  connect(m_openTimer, &QTimer::timeout, this, &MainWindow::updateOpenDevices);
  m_openTimer->start();
}

// Add device, Devices and the views on the toolbar.
void MainWindow::fillViewToolbar()
{
  toolBarDMM->insertAction(toolBarDMM->actions().value(0), m_sidebarAction);
  auto *plusIcon = new PlusIconEngine("qtdmm-dmm");   // QIcon owns it
  m_addDeviceAction = new QAction(QIcon(plusIcon), tr("&Add device..."), this);
  m_addDeviceAction->setObjectName("action_AddDevice");
  m_addDeviceAction->setToolTip(tr("Add device: a meter, a sensor or a calculated value"));
  m_addDeviceAction->setWhatsThis(tr("<html><head/><body><p><span style=\" font-weight:600;\">Add device</span></p>"
                                     "<p>Step by step: how the meter is connected, which one it is, and whether it "
                                     "goes into this window or a new one. It is kept in My devices.</p></body></html>"));
  connect(m_addDeviceAction, &QAction::triggered, this, &MainWindow::addDevice);
  toolBarDMM->insertAction(m_sidebarAction, m_addDeviceAction);

  // the project basket, second: a click opens one, the arrow makes one,
  // takes one of the last ones or closes the open one
  m_projectAction = new QAction(QIcon::fromTheme("document-open", style()->standardIcon(QStyle::SP_DirOpenIcon)),
                                tr("Open project"), this);
  m_projectAction->setObjectName("action_ProjectBasket");
  m_projectAction->setToolTip(tr("Open a project; the arrow: a new one, the last ones"));
  connect(m_projectAction, &QAction::triggered, m_basket->openAction(), &QAction::trigger);
  toolBarDMM->insertAction(m_sidebarAction, m_projectAction);
  addViewMenu(m_projectAction, [this](QMenu *menu)
  {
    menu->addAction(m_basket->newAction());
    menu->addAction(m_basket->openAction());
    const QStringList dirs = recentProjects().list().mid(0, 10);
    if (!dirs.isEmpty())
      menu->addSeparator();
    for (int i = 0; i < dirs.size(); ++i)
    {
      const QString &dir = dirs[i];
      QString name = BasketWidget::readProject(dir).value("name").toString();
      if (name.isEmpty())
        name = QFileInfo(dir).fileName();
      name.replace('&', "&&");
      // 1 ... 9 as the letters, then none
      QAction *a = menu->addAction(i < 9 ? QString("&%1 %2").arg(i + 1).arg(name) : name);
      a->setToolTip(QDir::toNativeSeparators(dir));
      a->setCheckable(true);
      a->setChecked(dir == m_basket->folder());
      a->setEnabled(QFileInfo(dir).isDir());
      connect(a, &QAction::triggered, this, [this, dir] { m_basket->setFolder(dir); });
    }
    if (!m_basket->folder().isEmpty())
    {
      menu->addSeparator();
      menu->addAction(m_basket->closeAction());
    }
    menu->setToolTipsVisible(true);
  });

  toolBarDMM->insertSeparator(action_Connect);
  toolBarDMM->addSeparator();
  toolBarDMM->addAction(m_displayAction);
  toolBarDMM->addAction(m_meterAction);
  toolBarDMM->addAction(m_poincareAction);
  toolBarDMM->addAction(action_Graph);
  toolBarDMM->addAction(m_readingsAction);
  connect(m_displayAction, &QAction::toggled, this, &MainWindow::storeDisplaySLOT);

  // a click shows or hides the views of the kind, the arrow lists them; the
  // table is one window, without an arrow
  addViewMenu(m_displayAction, [this](QMenu *menu)
  {
    for (auto it = m_panelWins.cbegin(); it != m_panelWins.cend(); ++it)
      if (const FrontPanel *panel = panelOf(it.value()); panel && panel->controller())
        addViewEntry(menu, it.value(), panel->controller()->deviceName());
  });
  addViewMenu(m_meterAction, [this](QMenu *menu)
  {
    for (QMdiSubWindow *win : meterWindows())
      addViewEntry(menu, win, viewName(win, meterViewOf(win)->value()));
    menu->addSeparator();
    connect(menu->addAction(tr("&New...")), &QAction::triggered, m_newMeterAction, &QAction::trigger);
  });
  addViewMenu(m_poincareAction, [this](QMenu *menu)
  {
    for (QMdiSubWindow *win : poincareWindows())
      addViewEntry(menu, win, viewName(win, poincareViewOf(win)->value()));
    menu->addSeparator();
    connect(menu->addAction(tr("&New...")), &QAction::triggered, m_newPoincareAction, &QAction::trigger);
  });
  addViewMenu(action_Graph, [this](QMenu *menu)
  {
    addViewEntry(menu, m_graphWin, m_graphWin->windowTitle());
    if (m_graphSources->isVisible())
      menu->addAction(m_graphSources);
  });
  // no value yet: the click is the way to choose one; the others of the
  // kind follow the button
  auto kindClicked = [this](QAction *action, const std::function<QList<QMdiSubWindow *>()> &windows,
                            const std::function<bool()> &choose)
  {
    connect(action, &QAction::triggered, this, [this, action, windows, choose](bool on)
    {
      if (on && !choose())
      {
        action->setChecked(false);
        return;
      }
      for (QMdiSubWindow *win : windows())
        if (m_added.contains(win))
        {
          win->setVisible(on);
          if (on)
            growFor(win);
        }
      m_arranger->arrange();
    });
  };
  kindClicked(m_meterAction, [this] { return meterWindows(); }, [this]
  {
    const bool none = meterWindows().size() == 1 && meterViewOf(m_meterWin)->value().source.isEmpty();
    return !none || m_session->controllers().isEmpty() || editSources(ValueMenu::View::Meter, meterViewOf(m_meterWin));
  });
  kindClicked(m_poincareAction, [this] { return poincareWindows(); }, [this]
  {
    const bool none = poincareWindows().size() == 1 && poincareViewOf(m_poincareWin)->value().source.isEmpty();
    return !none || m_session->controllers().isEmpty()
           || editSources(ValueMenu::View::Poincare, nullptr, poincareViewOf(m_poincareWin));
  });
}

void MainWindow::addViewMenu(QAction *action, const std::function<void(QMenu *)> &fill)
{
  auto *button = qobject_cast<QToolButton *>(toolBarDMM->widgetForAction(action));
  if (!button)
    return;
  auto *menu = new QMenu(button);
  connect(menu, &QMenu::aboutToShow, this, [menu, fill]
  {
    menu->clear();
    fill(menu);
  });
  button->setMenu(menu);
  button->setPopupMode(QToolButton::MenuButtonPopup);
}

void MainWindow::addViewEntry(QMenu *menu, QMdiSubWindow *win, const QString &name)
{
  QAction *entry = menu->addAction(QString(name).replace('&', "&&"));
  entry->setCheckable(true);
  entry->setChecked(!win->isHidden());
  connect(entry, &QAction::toggled, this, [this, win](bool on)
  {
    // the windows with an action of their own through it, it follows them
    for (QAction *a : { m_meterAction, m_poincareAction, action_Graph })
      if (a->property("window").value<QObject *>() == win)
      {
        a->setChecked(on);
        m_arranger->arrange();
        return;
      }
    win->setVisible(on);
    if (on)
      growFor(win);
    m_arranger->arrange();
  });
}

QString MainWindow::viewName(const QMdiSubWindow *win, const ValueRef &value) const
{
  // several of a kind: which is which
  const MeterController *ctl = m_session->controller(value.source);
  return ctl ? QString("%1 · %2").arg(win->windowTitle(), ctl->deviceName()) : win->windowTitle();
}

// Arrange: the arrangement, title bars, workspaces; returns Load and Save workspace for the keys.
QList<QAction *> MainWindow::createArrangeMenu()
{
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
  m_titleBars->setObjectName("action_TitleBars");
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
  QAction *loadWs = m_arrangeMenu->addAction(QIcon::fromTheme("document-open"), tr("L&oad bench..."));
  loadWs->setObjectName("action_LoadWorkspace");
  loadWs->setWhatsThis(tr("<html><head/><body><p><span style=\" font-weight:600;\">Load bench</span></p>"
                          "<p>Takes a set-up saved with <i>Save bench</i>: the devices connected (others "
                          "close), which windows are shown with which values, the "
                          "arrangement, title bars, toolbar, design and the size of the main window. A device "
                          "missing in My devices is added back from the file.</p></body></html>"));
  connect(loadWs, &QAction::triggered, this, &MainWindow::loadWorkspace);
  // filled from the file each time: another QtDMM may have added one
  m_recentMenu = m_arrangeMenu->addMenu(QIcon::fromTheme("document-open-recent"), tr("&Recent benches"));
  m_recentMenu->setObjectName("menu_RecentWorkspaces");
  connect(m_recentMenu, &QMenu::aboutToShow, this, &MainWindow::updateRecentWorkspaces);
  QAction *saveWs = m_arrangeMenu->addAction(QIcon::fromTheme("document-save-as"), tr("&Save bench..."));
  saveWs->setObjectName("action_SaveWorkspace");
  saveWs->setWhatsThis(tr("<html><head/><body><p><span style=\" font-weight:600;\">Save bench</span></p>"
                          "<p>Writes the set-up to a file, under a name and a description, with a picture of "
                          "the windows: the devices connected (a copy of their entries in My devices), which "
                          "windows are shown with which values, the arrangement, "
                          "title bars, toolbar, design and the size of the main window.</p></body></html>"));
  connect(saveWs, &QAction::triggered, this, &MainWindow::saveWorkspace);
  // the same at the sidebar's Recent benches
  m_sidebar->setWorkspaceActions({ loadWs, saveWs });
  m_autoSaveLayout = m_arrangeMenu->addAction(tr("Save layout on e&xit"));
  m_autoSaveLayout->setCheckable(true);
  m_autoSaveLayout->setChecked(true);
  m_autoSaveLayout->setWhatsThis(tr("<html><head/><body><p><span style=\" font-weight:600;\">Save layout on exit</span></p>"
                                    "<p>On: QtDMM starts with the window layout it had when it was closed. Off: it "
                                    "starts with the layout last saved while this was on, or the bench last "
                                    "loaded.</p></body></html>"));
  QAction *arrangeButton = m_arrangeMenu->menuAction();
  arrangeButton->setIcon(QIcon::fromTheme("qtdmm-arrange"));
  arrangeButton->setToolTip(tr("Arrange the windows"));
  // on the right, next to the menu button: it arranges the whole window,
  // not the meter
  toolBarMenu->insertAction(action_Menu, arrangeButton);
  if (auto *button = qobject_cast<QToolButton *>(toolBarMenu->widgetForAction(arrangeButton)))
    button->setPopupMode(QToolButton::InstantPopup);
  return { loadWs, saveWs };
}

// Design: the colour designs of the window.
void MainWindow::createDesignMenu()
{
  // colour designs of the window (the LCD tint and the meter style are
  // settings of their own)
  m_designMenu = new QMenu(tr("D&esign"), this);
  auto *designGroup = new QActionGroup(this);
  for (auto [d, text] : { std::pair{Designs::System, tr("&System")}, std::pair{Designs::Silver, tr("&Light")},
                          std::pair{Designs::Dark, tr("&Dark")}, std::pair{Designs::BrushedMetal, tr("Light &Metal")},
                          std::pair{Designs::DarkMetal, tr("Dark Me&tal")} })
  {
    QAction *a = m_designMenu->addAction(text);
    a->setCheckable(true);
    a->setData(int(d));
    designGroup->addAction(a);
    connect(a, &QAction::triggered, this, [this, d = d] { setDesign(d); });
  }
}

// The readings dot, the connection and the info field, and what feeds them.
void MainWindow::createStatusBar()
{
  m_error = new QLabel(statusBar());
  m_error->setFrameStyle(QFrame::Panel | QFrame::Sunken);
  statusBar()->addWidget(m_error, 20);
  m_error->setLineWidth(1);

  m_info = new QLabel(statusBar());
  m_info->setFrameStyle(QFrame::Panel | QFrame::Sunken);
  statusBar()->addWidget(m_info, 10);
  m_info->setLineWidth(1);

  // the toolbar on and off: the way back with the mouse, always there
  m_toolbarButton = new QToolButton(statusBar());
  m_toolbarButton->setObjectName("toolbarButton");
  m_toolbarButton->setAutoRaise(true);
  statusBar()->addPermanentWidget(m_toolbarButton);
  connect(m_toolbarButton, &QToolButton::clicked, m_toolbars, &QAction::trigger);
  connect(m_toolbars, &QAction::toggled, this, &MainWindow::updateToolbarButton);
  updateToolbarButton();

  // the SCPI server and HiSLIP are in the sidebar (Services), not here
  connect(m_wid, &InstanceWidget::servicesChanged, this, &MainWindow::updateServices);

  // messages such as the permission hint span several lines; the status bar
  // shows the first one and keeps the rest in the tooltip
  connect(m_wid, &InstanceWidget::error, this, [this](const QString &text)
  {
    const QString firstLine = text.section('\n', 0, 0);
    m_error->setText(firstLine);
    m_error->setToolTip(text.contains('\n') ? text : QString());
  });
  connect(m_wid, &InstanceWidget::info, this, [this](const QString &text)
  {
    // without a device the graph's Live time says nothing
    if (m_session->controllers().isEmpty() && m_wid->graph()->store()->state() == RecordingStore::Live)
      return;
    m_info->setText(text.section('\n', 0, 0));
    m_info->setToolTip(text.section('\n', 1));
  });
  connect(m_wid, &InstanceWidget::useTextLabel, this, &MainWindow::setUseTextLabel);
  connect(m_wid, &InstanceWidget::iconSet, this, [](const QString &set)
          { Designs::setIconSet(Designs::iconSetFromName(set)); });
  // the graph's menu: Connect and Disconnect, as the toolbar
  connect(m_wid->graph(), &GraphWidget::connectDMM, this, &MainWindow::connectBench);
  connect(m_wid, &InstanceWidget::showDisplay, this, &MainWindow::showDisplaySLOT);
}

// Applies the settings and puts the window where it was, or sizes it to the views shown.
void MainWindow::placeWindow()
{
  QRect winRect = m_wid->settingsDialog()->winRect();

  m_wid->applySLOT();
  // the layout of the windows; the dock state of versions before the MDI
  // window (MainWindow/state) is not taken over
  restoreWindows();

  setMinimumSize(19 * em(), 13 * em());
  // winRect() falls back to 500 x 350: only a stored width means a stored
  // geometry; a first start lets the window manager place the window
  const bool stored = m_wid->settings()->getInt("Position/width", 0) > 0;
  if (stored && m_wid->settingsDialog()->generalPage()->saveWindowPosition())
    move(winRect.x(), winRect.y());
  // the views shown at the start count as grown for; the table column gets
  // the width the window has for it
  for (QMdiSubWindow *w : { m_meterWin, m_graphWin, m_readingsWin, m_poincareWin })
    if (!w->isHidden())
      m_grown.insert(w);
  if (m_grown.contains(m_readingsWin) || m_grown.contains(m_poincareWin))
    m_arranger->setTableWidth(growthFor(m_readingsWin).width());
  if (stored && m_wid->settingsDialog()->generalPage()->saveWindowSize())
  {
    resize(winRect.width(), winRect.height());
    m_userSized = m_wid->settings()->getBool("Windows/user-sized", false);
  }
  else
  {
    // from the content: an instrument, plus what each shown view adds; the
    // front panels grow it when their meters connect
    QSize start(28 * em(), 22 * em());
    for (QMdiSubWindow *w : { m_graphWin, m_readingsWin, m_poincareWin })
      if (m_grown.contains(w))
        start += growthFor(w);
    // at least 800×600 at the first start, the views fill it; they grow
    // it once they need more
    m_content = start;
    start = start.expandedTo(QSize(800, 600));
    const QRect av = screen()->availableGeometry();
    resize(qMin(start.width(), int(av.width() * 0.84)), qMin(start.height(), int(av.height() * 0.84)));
  }
  if (!m_content.isValid())
    m_content = size();
  m_expectSize = size();
}

void MainWindow::updateEmptyStart()
{
  const bool empty = m_session->controllers().isEmpty() && m_pendingOpen.isEmpty() && !m_wid->graph()->showsFile();
  m_emptyConnect->setVisible(!m_wid->devices()->list().isEmpty());
  // nothing to see behind the start page; checked stays what comes back
  for (QAction *a : { m_displayAction, m_meterAction, m_poincareAction, action_Graph, m_readingsAction, m_basketAction })
    a->setEnabled(!empty);
  if (empty)
  {
    // the last device closed: its messages go with it
    if (m_emptyStart->isHidden())
      for (QLabel *l : { m_error, m_info })
      {
        l->clear();
        l->setToolTip(QString());
      }
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
  dlg.setCalcInputs(m_session->calcInputs());
  dlg.setPlacesInUse(placesInUse());
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
  // Connect: in My devices and connected; Save only: My devices alone
  if (dlg.target() == AddDeviceDlg::Connect)
    openBeside(id);
  // the second device: from now on there is a choice
  if (DeviceSidebar::opensAfterAdd(before, int(m_wid->devices()->list().size())))
  {
    showSidebar(true);
    m_wid->settings()->setBool("Windows/sidebar", true);
  }
}

QMap<QString, const MeterController *> MainWindow::placeOwners() const
{
  QMap<QString, const MeterController *> owners;
  auto add = [&owners](const QVariantMap &keys, const MeterController *ctl)
  {
    // a formula is no port: any number of meters may compute it
    const QString place = DeviceLibrary::place(keys);
    if (!place.isEmpty() && !place.startsWith(QLatin1String("calc ")))
      owners.insert(place, ctl);
  };
  for (const MeterController *ctl : m_session->controllers())
    if (const std::optional<MyDevice> device = m_wid->devices()->find(ctl->deviceId()))
      add(device->keys, ctl);
  return owners;
}

QMap<QString, QString> MainWindow::placesInUse() const
{
  QMap<QString, QString> inUse;
  const QMap<QString, const MeterController *> owners = placeOwners();
  for (auto it = owners.constBegin(); it != owners.constEnd(); ++it)
    inUse.insert(it.key(), tr("Connected: %1").arg(it.value()->deviceName()));
  return inUse;
}

// "Ripple › Bench 1 – QtDMM": the project and the bench in use, either
// alone when the other is not; without both the devices, "QtDMM: UT61E,
// Victron". With the --config-id when one is given
void MainWindow::updateWindowTitle()
{
  QString title = APP_NAME;
  if (!m_configId.isEmpty())
    title += QString(" [%1]").arg(m_configId);
  QStringList where;
  if (const QString project = m_basket ? m_basket->folder() : QString(); !project.isEmpty())
  {
    const QString name = BasketWidget::readProject(project).value("name").toString();
    where << (name.isEmpty() ? QFileInfo(project).fileName() : name);
  }
  // a bench without a name of its own by its file, as the sidebar has it
  if (!m_workspaceName.isEmpty() || !m_workspaceFile.isEmpty())
    where << (m_workspaceName.isEmpty() ? QFileInfo(m_workspaceFile).completeBaseName() : m_workspaceName);
  if (!where.isEmpty())
  {
    setWindowTitle(where.join(QStringLiteral(" \u203A ")) + QStringLiteral(" \u2013 ") + title);
    return;
  }
  QStringList names;
  for (const MeterController *ctl : m_session->controllers())
    names << ctl->deviceName();
  setWindowTitle(names.isEmpty() ? title : QString("%1: %2").arg(title, names.join(", ")));
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
  toolBarMenu->setToolButtonStyle(Style);
}

void MainWindow::createActions()
{
  // Connect and Reset act on the whole bench; one device by its context menu
  connect(action_Connect, &QAction::triggered, this, &MainWindow::connectBench);
  connect(action_Reset, &QAction::triggered, this, [this]
  {
    for (MeterController *ctl : m_session->controllers())
      ctl->resetMinMax();
  });
  connect(action_Start, &QAction::triggered, this, &MainWindow::startSLOT);
  // one button for both: a grey dot starts, the red square stops
  connect(action_Record, &QAction::triggered, this, &MainWindow::toggleRecordingSLOT);
  connect(action_Live, &QAction::triggered, this, [this]
  {
    m_wid->liveSLOT();
    updateRecorderActions();   // pressed again when the question was cancelled
  });
  // the graph's context menu starts and stops like the button; it records
  // only the meters whose values it shows
  connect(m_wid->graph(), &GraphWidget::recordRequested, this, [this](bool start)
  {
    if (!start)
      action_Stop->trigger();
    else if (action_Start->isEnabled())
    {
      QStringList sources;
      for (const ValueRef &v : m_wid->graph()->values())
        sources << m_wid->graph()->resolved(v).source;
      m_session->setRecordingScope(sources);
      action_Start->trigger();
      if (!m_session->isRecording())
        m_session->clearRecordingScope();   // cancelled
    }
  });
  connect(action_Stop, &QAction::triggered, m_wid, &InstanceWidget::stopSLOT);
  connect(action_Clear, &QAction::triggered, m_wid, &InstanceWidget::clearSLOT);
  connect(action_Print, &QAction::triggered, m_wid, &InstanceWidget::printSLOT);
  connect(action_Import, &QAction::triggered, m_wid, &InstanceWidget::importSLOT);
  connect(action_Configure, &QAction::triggered, m_wid, &InstanceWidget::configSLOT);
  // Shift+F8: the settings of the device chosen in the sidebar, as its context menu there
  connect(action_ConfigureDMM, &QAction::triggered, this, [this]
  {
    if (const QString id = m_sidebar->chosenDevice(); !id.isEmpty())
      deviceSettings(id, m_session->controller("device:" + id) != nullptr);
  });
  connect(actionConfigureRecorder, &QAction::triggered, m_wid, &InstanceWidget::configRecorderSLOT);
  connect(action_Quit, &QAction::triggered, this, &MainWindow::storeDisplaySLOT);
  connect(action_Quit, &QAction::triggered, m_wid, &InstanceWidget::quitSLOT);
  connect(action_Direct_help, &QAction::triggered, m_wid, &InstanceWidget::helpSLOT);


}

// Actions that live only in the popup menu are not attached to any widget,
// so their shortcuts would be dead - adding them to the window fixes that.
void MainWindow::createExtraActions()
{
  // beside the windows, to look into a fault or attach to a report
  m_logAction = new QAction(tr("&Log..."), this);
  m_logAction->setObjectName("action_Log");
  m_logAction->setWhatsThis(tr("<html><head/><body><p><span style=\" font-weight:600;\">Log</span></p>"
                               "<p>What QtDMM did: errors, connection states and messages of the program, with the "
                               "levels to show, a search and the export to attach to a report.</p></body></html>"));
  connect(m_logAction, &QAction::triggered, this, [this]
  {
    if (!m_logDlg)
      m_logDlg = new LogDlg(AppLog::instance(), this);
    m_logDlg->show();
    m_logDlg->raise();
    m_logDlg->activateWindow();
  });
  m_fullScreen = new QAction(tr("&Full screen"), this);
  m_fullScreen->setObjectName("action_FullScreen");
  m_fullScreen->setCheckable(true);
  m_fullScreen->setShortcut(QKeySequence("F11"));
  m_fullScreen->setWhatsThis(tr("<html><head/><body><p><span style=\" font-weight:600;\">Full screen</span></p>"
                                "<p>Use the whole screen for the instruments, e.g. on a lab monitor. The same key "
                                "again returns to the normal window.</p></body></html>"));
  connect(m_fullScreen, &QAction::toggled, this, &MainWindow::setFullScreen);

  m_toolbars = new QAction(tr("Hide &toolbar"), this);
  m_toolbars->setObjectName("action_Toolbars");
  m_toolbars->setCheckable(true);
  m_toolbars->setShortcut(QKeySequence("Ctrl+M"));
  m_toolbars->setWhatsThis(tr("<html><head/><body><p><span style=\" font-weight:600;\">Hide toolbar</span></p>"
                              "<p>More room for the instruments. The menu stays at hand with F7, the same key "
                              "again brings the toolbar back.</p></body></html>"));
  connect(m_toolbars, &QAction::toggled, this, &MainWindow::setToolbarsHidden);
  // hidden by hand (not by a workspace): say how it comes back, a pressed Ctrl+M is easily missed
  connect(m_toolbars, &QAction::triggered, this, [this](bool hidden)
  {
    if (hidden)
      QTimer::singleShot(0, this, &MainWindow::toolbarHiddenHint);
  });

  m_zoomIn = new QAction(tr("Zoom &in"), this);
  m_zoomIn->setObjectName("action_ZoomIn");
  m_zoomIn->setShortcuts({QKeySequence::ZoomIn, QKeySequence("Ctrl+=")});
  connect(m_zoomIn, &QAction::triggered, m_wid->graph(), &GraphWidget::zoomInSLOT);
  m_zoomOut = new QAction(tr("Zoom &out"), this);
  m_zoomOut->setObjectName("action_ZoomOut");
  m_zoomOut->setShortcut(QKeySequence::ZoomOut);
  connect(m_zoomOut, &QAction::triggered, m_wid->graph(), &GraphWidget::zoomOutSLOT);
  m_zoomFit = new QAction(tr("Show &whole recording"), this);
  m_zoomFit->setObjectName("action_ZoomFit");
  m_zoomFit->setShortcut(QKeySequence("Ctrl+0"));
  connect(m_zoomFit, &QAction::triggered, m_wid->graph(), &GraphWidget::zoomFitSLOT);
  m_copyImage = new QAction(tr("Cop&y graph image"), this);
  m_copyImage->setObjectName("action_CopyImage");
  m_copyImage->setShortcut(QKeySequence("Ctrl+Shift+C"));
  m_copyImage->setWhatsThis(tr("<html><head/><body><p><span style=\" font-weight:600;\">Copy graph image</span></p>"
                               "<p>Puts a picture of the graph on the clipboard, ready to paste into a "
                               "report or a chat.</p></body></html>"));
  connect(m_copyImage, &QAction::triggered, m_wid->graph(), &GraphWidget::copyImageSLOT);

  // Space (Record) toggles the recorder; Start and Stop have neither a
  // button nor a key of their own any more
  addActions({action_Configure, action_ConfigureDMM, action_Direct_help, action_Help, action_Quit,
              m_displayAction, m_meterAction, m_readingsAction, m_poincareAction, m_basketAction, m_titleBars,
              m_fullScreen, m_toolbars, m_zoomIn, m_zoomOut, m_zoomFit, m_copyImage,
              // no buttons of their own: the keys stay, the graph's menu has them
              action_Connect, action_Reset, action_Record, action_Live, action_Clear,
              m_graphExport, action_Import, action_Print});
}

// Every command that may have a key, in the groups of the page Keyboard;
// the keys the user set (shortcuts.conf, all instances) replace the ones
// given above, and the tooltips name them
void MainWindow::createShortcuts(const QList<QAction *> &workspace)
{
  m_shortcuts = new ShortcutRegistry(this, QDir(m_wid->settings()->configDir()).filePath("shortcuts.conf"), this);
  const QString meter = tr("Meter"), recording = tr("Recording and graph"), windows = tr("Windows"),
                help = tr("Settings and help");
  for (QAction *a : { action_Connect, action_Reset, action_ConfigureDMM, m_addDeviceAction })
    m_shortcuts->add(a, meter);
  m_shortcuts->add(action_Record, recording, tr("Record / Stop"));
  for (QAction *a : { action_Live, action_Clear, m_zoomIn, m_zoomOut, m_zoomFit, m_copyImage,
                      m_graphExport, action_Import, action_Print })
    m_shortcuts->add(a, recording);
  for (QAction *a : { m_sidebarAction, m_displayAction, m_meterAction, m_newMeterAction, action_Graph,
                      m_readingsAction, m_poincareAction, m_newPoincareAction, m_basketAction, m_titleBars, m_toolbars,
                      m_fullScreen })
    m_shortcuts->add(a, windows);
  for (QAction *a : workspace)
    m_shortcuts->add(a, windows);
  m_shortcuts->add(action_Configure, help, tr("Settings"));
  m_shortcuts->add(actionConfigureRecorder, help, tr("Recording settings"));
  for (QAction *a : { action_Help, action_Direct_help, m_logAction, action_About, action_Menu, action_Quit })
    m_shortcuts->add(a, help);
  m_shortcuts->load();
  // Record names its key in a tooltip of its own
  connect(m_shortcuts, &ShortcutRegistry::changed, this, &MainWindow::updateRecorderActions);
  m_wid->settingsDialog()->keyboardPage()->setRegistry(m_shortcuts);
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

void MainWindow::setToolbarsHidden(bool hide)
{
  for (QToolBar *bar : { toolBarDMM, toolBarMenu })
    bar->setVisible(!hide);
}

void MainWindow::toolbarHiddenHint()
{
  Settings *settings = m_wid->settings();
  if (!m_toolbars->isChecked() || !settings->getBool("Windows/toolbar-hint", true))
    return;
  const QString key = m_toolbars->shortcut().toString(QKeySequence::NativeText);
  const QString menuKey = action_Menu->shortcut().toString(QKeySequence::NativeText);
  QString text = key.isEmpty() ? tr("The toolbar is hidden. The arrow at the right end of the status line "
                                    "brings it back.")
                               : tr("The toolbar is hidden. %1 brings it back, and so does the arrow at the right "
                                    "end of the status line.").arg(key);
  if (!menuKey.isEmpty())
    text += ' ' + tr("The menu opens with %1.").arg(menuKey);
  QMessageBox box(QMessageBox::Information, tr("Hide toolbar"), text, QMessageBox::Ok, this);
  auto *again = new QCheckBox(tr("&Do not show this again"), &box);
  box.setCheckBox(again);
  box.exec();
  if (again->isChecked())
  {
    settings->setBool("Windows/toolbar-hint", false);
    settings->save();   // set only stages it, get reads the file
  }
}

void MainWindow::startSLOT()
{
  // a recording viewed and not saved yet: the new one clears it
  if (!m_wid->confirmRecording())
    return;
  if (!m_wid->askRecordingLength())
    return;
  // all meters connected record, the leading one sets the pace (DeviceSession)
  m_wid->startSLOT();
}

void MainWindow::runningSLOT(bool on)
{
  m_running = on;
  if (on)
    action_Graph->setChecked(true);   // a recording wants to be seen

  action_Start->setEnabled(!on);
  action_Stop->setEnabled(on);
  action_Print->setEnabled(!on);
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
  const MeterController *lead = m_session->lead();
  const RecordingStore::State state = (lead ? lead->recorder() : m_wid->graph()->store())->state();
  const bool recording = state == RecordingStore::Record;
  action_Record->setEnabled(recording ? action_Stop->isEnabled() : action_Start->isEnabled());
  action_Record->setIcon(QIcon(new RecordIconEngine(recording)));
  action_Record->setText(recording ? tr("&Stop") : tr("Rec&ord"));
  const QString key = action_Record->shortcut().toString(QKeySequence::NativeText);
  action_Record->setToolTip(QString("%1 (%2)").arg(recording ? tr("Stop recording") : tr("Start recording"), key));
  action_Live->setEnabled(!recording);
  action_Live->setChecked(state == RecordingStore::Live);
  if (m_labPlotLive)
    m_labPlotLive->setEnabled(recording || (m_live && m_live->isRunning()));
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
                   "<p>Built with <b>Qt</b> %2. Licensed under the <b>GNU GPL 3 or later</b> "
                   "(versions before 0.9.0 under GPL 2 or later).</p>"
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
    // for the whole bench, also without the graph
    m_menu->addAction(action_Record);
    m_menu->addSeparator();
    // as on the toolbar
    m_menu->addAction(m_displayAction);
    m_menu->addAction(m_meterAction);
    m_menu->addAction(m_poincareAction);
    m_menu->addAction(action_Graph);
    m_menu->addAction(m_readingsAction);
    m_menu->addAction(m_basketAction);
    // Arrange has its button next to the menu's; here only while the toolbar is hidden
    m_arrangeEntry = new QAction(m_arrangeMenu->title(), m_menu);
    m_arrangeEntry->setMenu(m_arrangeMenu);
    m_menu->addAction(m_arrangeEntry);
    m_menu->addMenu(m_designMenu);
    m_menu->addAction(m_toolbars);
    m_menu->addAction(m_fullScreen);
    m_menu->addSeparator();
    m_menu->addAction(action_Help);
    m_menu->addAction(action_Direct_help);
    m_menu->addAction(m_logAction);
    m_menu->addAction(action_About);
    m_menu->addSeparator();
    m_menu->addAction(action_Quit);
  }
  m_arrangeEntry->setVisible(m_toolbars->isChecked());

  // below the menu button; with the toolbar hidden at the top right of the window
  QWidget* widget = this->toolBarMenu->widgetForAction(this->action_Menu);
  if (widget && widget->isVisible())
  {
    m_menu->popup(widget->mapToGlobal(
      QPoint( widget->width() - m_menu->sizeHint().width(), widget->height())
    ));
  }
  else
    m_menu->popup(mapToGlobal(QPoint(width() - m_menu->sizeHint().width(), 0)));
}


void MainWindow::closeEvent(QCloseEvent *ev)
{
  storeDisplaySLOT();
  // the settings dialog saves "Show display" as well: without auto-save it
  // keeps the start layout's
  if (!m_autoSaveLayout->isChecked())
    m_wid->settingsDialog()->guiPage()->setShowDisplay(m_startDisplay);
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
  if (watched == m_mdi->viewport()
      && (event->type() == QEvent::DragEnter || event->type() == QEvent::DragMove || event->type() == QEvent::Drop))
  {
    auto *drag = static_cast<QDropEvent *>(event);
    const QMimeData *data = drag->mimeData();
    const bool value = data->hasFormat(DeviceSidebar::kValueMime);
    const QString id = QString::fromUtf8(data->data(DeviceSidebar::kDeviceMime));
    if (!value && (id.isEmpty() || m_session->controller("device:" + id)))
      return false;
    // taken, but the sidebar keeps its row (no move)
    drag->setDropAction(Qt::IgnoreAction);
    drag->accept();
    if (event->type() == QEvent::DragEnter || event->type() == QEvent::DragMove)
    {
      drag->setDropAction(Qt::CopyAction);
      return true;
    }
    if (value)
      dropValue(ValueRef::fromString(QString::fromUtf8(data->data(DeviceSidebar::kValueMime))),
                drag->position().toPoint());
    else
      openBeside(id);
    return true;
  }
  if (watched == m_mdi && event->type() == QEvent::Resize)
    m_emptyStart->setGeometry(m_mdi->rect());
  // closing a window (its title bar button, Ctrl+F4) only hides it: a real
  // close would close the view inside as well, and it would not come back
  if (event->type() == QEvent::Close)
    if (auto *win = qobject_cast<QMdiSubWindow *>(watched))
    {
      // an analog meter added goes; not inside its own event
      if (m_added.contains(win))
        QTimer::singleShot(0, this, [this, w = QPointer<QMdiSubWindow>(win)]
        {
          if (w)
            removeAddedWindow(w);
        });
      win->hide();
      event->ignore();
      return true;
    }
  // minimizing the main window hides its children spontaneously; they are
  // not closed, so only explicit show/hide counts (isHidden() stays false)
  if ((event->type() == QEvent::Show || event->type() == QEvent::Hide) && !event->spontaneous())
    for (QAction *a : { m_displayAction, m_meterAction, m_readingsAction, m_poincareAction, action_Graph,
                        m_basketAction })
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
  MeterView *meter = meterViewOf(win);
  const bool added = m_added.contains(win);
  PoincareView *plot = poincareViewOf(win);
  QAction *hide = menu.addAction(added ? tr("&Close window") : tr("&Hide view"));
  FrontPanel *panel = panelOf(win);
  // a front panel goes with its device, also while the menu is open
  QPointer<QMdiSubWindow> window(win);
  QPointer<FrontPanel> open(panel);
  if (panel && panel->keysSupported())
  {
    QAction *hideControls = menu.addAction(tr("Hide &controls"));
    hideControls->setCheckable(true);
    hideControls->setChecked(panel->isFolded());
    connect(hideControls, &QAction::toggled, this, [this, open](bool on)
    {
      if (!open)
        return;
      open->setFolded(on);
      updateControls();
    });
  }
  if (panel)
  {
    QMenu *lcd = menu.addMenu(tr("&LCD colours"));
    const std::pair<LcdWidget::LcdVariant, QString> variants[] = {
      { LcdWidget::Classic, tr("&Classic") }, { LcdWidget::BacklightBlue, tr("&Backlight blue") },
      { LcdWidget::Amber, tr("&Amber") }, { LcdWidget::HighContrast, tr("&High contrast") },
      { LcdWidget::Custom, tr("C&ustom (from the settings)") } };
    // this panel's own, or those of the settings
    const QString source = panel->controller() ? panel->controller()->source() : QString();
    auto *group = new QActionGroup(lcd);
    QAction *settings = lcd->addAction(tr("As in the &settings"));
    settings->setCheckable(true);
    settings->setChecked(!m_panelLcd.contains(source));
    group->addAction(settings);
    connect(settings, &QAction::triggered, this, [this, source] { setPanelLcdVariant(source, std::nullopt); });
    lcd->addSeparator();
    for (const auto &[v, text] : variants)
    {
      QAction *a = lcd->addAction(text);
      a->setCheckable(true);
      a->setChecked(m_panelLcd.value(source, -1) == v);
      group->addAction(a);
      connect(a, &QAction::triggered, this, [this, source, v = v] { setPanelLcdVariant(source, v); });
    }
  }
  // which of its device's values it shows, big and on the small line
  if (panel && panel->controller() && panel->controller()->valueCount() > 1)
    connect(menu.addAction(tr("&Values...")), &QAction::triggered, this, [this, open]
    {
      if (open)
        editPanelValues(open);
    });
  if (panel && panel->controller())
  {
    menu.addSeparator();
    addRulesMenu(&menu, panel->controller());
  }
  // the value it shows, of any open meter
  if ((meter || plot) && !m_session->controllers().isEmpty())
    connect(menu.addAction(tr("S&ources...")), &QAction::triggered, this, [this, meter, plot]
    {
      editSources(meter ? ValueMenu::View::Meter : ValueMenu::View::Poincare, meter, plot);
    });
  if (meter)
  {
    // this meter's own, the settings' until one is picked
    QMenu *style = menu.addMenu(tr("Meter &style"));
    const std::pair<int, QString> styles[] = { { 0, tr("&Dark studio") }, { 1, tr("Classic &ivory") } };
    for (const auto &[s, text] : styles)
    {
      QAction *a = style->addAction(text);
      a->setCheckable(true);
      a->setChecked(meter->shownStyle() == s);
      connect(a, &QAction::triggered, meter, [meter, s = s] { meter->setStyle(s); });
    }
    // another one, first with the same value
    connect(menu.addAction(m_newMeterAction->text()), &QAction::triggered, this,
            [this, meter] { newMeter(meter->value()); });
  }
  if (plot)
  {
    // another one, first with the same value
    connect(menu.addAction(m_newPoincareAction->text()), &QAction::triggered, this,
            [this, plot] { newView(Added::Poincare, plot->value()); });
    menu.addSeparator();
    plot->plot()->addMenuActions(&menu);
  }
  if (menu.exec(globalPos) != hide || !window)
    return;
  if (added)
    removeAddedWindow(win);
  else
    win->hide();
}

void MainWindow::addRulesMenu(QMenu *parent, MeterController *ctl)
{
  // together in one submenu. The limit test: on and off here, the limits in
  // the dialog; on without limits set yet asks for them
  QMenu *menu = parent->addMenu(tr("Limits && &alarms"));
  QAction *test = menu->addAction(tr("PASS/FAIL &test"));
  test->setCheckable(true);
  test->setChecked(ctl->limitTest().enabled);
  QPointer<MeterController> meter(ctl);
  connect(test, &QAction::toggled, this, [this, meter](bool on)
  {
    if (!meter)
      return;
    LimitTest t = meter->limitTest();
    t.enabled = on;
    if (on && t.low() == t.high())
      editLimits(meter, true);
    else
      meter->setLimitTest(t);
  });
  connect(menu->addAction(tr("L&imits...")), &QAction::triggered, this, [this, meter]
  {
    if (meter)
      editLimits(meter, false);
  });
  connect(menu->addAction(tr("&Alarms...")), &QAction::triggered, this, [this, meter]
  {
    if (meter)
      m_wid->openAlarms(meter->source());
  });
}

void MainWindow::editLimits(MeterController *ctl, bool enable, int id)
{
  LimitTest t = ctl->limitTest();
  if (enable)
    t.enabled = true;
  if (id >= 0)
    t.value = ValueRef(id);
  const Reading r = ctl->lastReading(t.value.id);
  const double reading = r.overload || r.noNumber() || r.text.isEmpty() ? std::nan("") : r.value;
  LimitDlg dlg(t, r.baseUnit.isEmpty() && t.value.id == 0 ? ctl->baseUnit() : r.baseUnit, reading, this);
  QPointer<MeterController> meter(ctl);   // it may close meanwhile
  if (dlg.exec() == QDialog::Accepted && meter)
  {
    LimitTest chosen = dlg.limitTest();
    chosen.value = t.value;
    meter->setLimitTest(chosen);
  }
}

void MainWindow::addAlarm(const ValueRef &value)
{
  MeterController *ctl = m_session->controller(value);
  if (!ctl)
    return;
  AlarmPrefs *page = m_wid->settingsDialog()->alarmPage();
  const AlarmPrefs::Device device = page->device(ctl->source());
  QList<Alarm> alarms = ctl->alarms()->alarms();
  Alarm al;
  al.name = AlarmPrefs::alarmName(int(alarms.size()) + 1);
  al.value = ValueRef(value.id);
  AlarmDlg dlg(al, device.units, device.valueNames, this);
  QPointer<MeterController> meter(ctl);
  if (dlg.exec() != QDialog::Accepted || !meter)
    return;
  alarms << dlg.alarm();
  meter->setAlarms(alarms);
}

void MainWindow::showSidebar(bool show)
{
  // the views keep their room and their arrangement: the window grows by
  // the sidebar's width and shrinks again, unless it fills the screen
  const bool keep = isVisible() && !isMaximized() && !isFullScreen() && show != m_sidebarDock->isVisible();
  const int before = m_mdi->width();
  m_sidebarDock->setVisible(show);
  m_sidebarAction->setChecked(show);
  if (keep)
  {
    layout()->activate();
    const int dw = before - m_mdi->width();
    const int maxW = screen()->availableGeometry().width() - (frameGeometry().width() - width());
    m_content.rwidth() += dw;
    if (dw != 0)
      resizeOnScreen(QSize(qMin(width() + dw, maxW), height()));
  }
  updateOpenDevices();
}

void MainWindow::updateServices()
{
  QList<DeviceSidebar::Service> rows;
  for (const ServiceState &state : m_session->host()->services())
  {
    DeviceSidebar::Service row;
    row.id = state.id;
    row.name = state.name;
    row.running = state.running;
    row.active = state.clients > 0;
    row.address = state.address;
    row.failed = !state.running && !state.error.isEmpty();
    row.value = state.running ? QString::number(state.port) : row.failed ? tr("failed") : tr("off");
    const QString clients = state.clients == 1 ? tr("1 client") : tr("%1 clients").arg(state.clients);
    if (row.failed)
      row.tooltip = tr("%1 could not start: %2. Double-click for its settings.").arg(state.name, state.error);
    else if (!state.running)
      row.tooltip = tr("%1 is off. Double-click for its settings.").arg(state.name);
    else if (state.id == QLatin1String("hislip"))
      row.tooltip = tr("HiSLIP for VISA (NI-VISA, LabVIEW, PyVISA): %1, %2 connected.").arg(state.address, clients);
    else if (state.id == QLatin1String("vxi11"))
      row.tooltip = tr("VXI-11 for VISA and lxi-tools: %1, %2 connected.").arg(state.address, clients);
    else
      row.tooltip = tr("SCPI server (raw socket) on %1, %2 connected.").arg(state.address, clients);
    rows << row;
  }
  m_sidebar->setServices(rows);
}

// A reading as the sidebar shows it: "1.5 V DC", "OL", "" without a number.
static QString readingText(const Reading &r)
{
  if (r.overload)
    return QStringLiteral("OL");
  if (r.noNumber())
    return QString();
  QString prefix;
  const QString value = SiPrefix::format(r.value, r.baseUnit, &prefix);
  // as the displays write the unit: °C, not the stored C
  return QString("%1 %2 %3").arg(value.left(8), SiPrefix::displayText(prefix + r.baseUnit), couplingText(r.port.defining))
    .trimmed();
}

bool MainWindow::editSources(ValueMenu::View view, MeterView *meter, PoincareView *plot)
{
  if (!meter)
    meter = meterViewOf(m_meterWin);
  if (!plot)
    plot = poincareViewOf(m_poincareWin);
  // the open meters with their values
  QList<SourcesDlg::Meter> meters;
  QList<ValueRef> all;
  for (const MeterController *ctl : m_session->controllers())
  {
    SourcesDlg::Meter meter;
    meter.source = ctl->source();
    meter.name = ctl->deviceName();
    for (int id = 0; id < ctl->valueCount(); ++id)
    {
      meter.values << otherValueRow(ctl, id).name;
      all << ValueRef(id, meter.source);
    }
    meters << meter;
  }
  GraphWidget *graph = m_wid->graph();
  const bool several = view == ValueMenu::View::Graph || view == ValueMenu::View::Table;
  QList<ValueRef> chosen;
  QString name;
  switch (view)
  {
    case ValueMenu::View::Graph:
      name = tr("Graph");
      chosen = graph->values();   // each with its meter's source, as the dialog lists them
      break;
    case ValueMenu::View::Table:
      name = tr("Readings table");
      for (const ValueRef &v : std::as_const(all))
        if (tableShows(v))
          chosen << v;
      break;
    case ValueMenu::View::Meter:
      name = meter->meter()->windowTitle();
      chosen << meter->value();
      break;
    default:
      name = plot->plot()->windowTitle();
      chosen << plot->value();
      break;
  }
  SourcesDlg dlg(name, several, meters, this);
  dlg.setChosen(chosen);
  if (dlg.exec() != QDialog::Accepted)
    return false;
  const QList<ValueRef> picked = dlg.chosen();
  if (!several && picked.isEmpty())
    return false;
  switch (view)
  {
    case ValueMenu::View::Graph:
      graph->setValues(picked);
      action_Graph->setChecked(true);
      break;
    case ValueMenu::View::Table:
    {
      // the meters of the values picked
      ReadingsModel *log = m_readings->log();
      QStringList sources;
      for (const ValueRef &v : std::as_const(all))
      {
        log->setValueHidden(v, !picked.contains(v));
        if (picked.contains(v) && !sources.contains(v.source))
          sources << v.source;
      }
      if (sources != tableSources())
      {
        m_tableMeters = sources;
        applyTableSources();
      }
      m_readingsAction->setChecked(true);
      break;
    }
    case ValueMenu::View::Meter:
      meter->setValue(picked.first());
      if (meter == meterViewOf(m_meterWin))
        m_meterAction->setChecked(true);
      break;
    default:
      plot->setValue(picked.first());
      if (plot == poincareViewOf(m_poincareWin))
        m_poincareAction->setChecked(true);
      break;
  }
  updateOpenDevices();
  return true;
}

void MainWindow::dropValue(const ValueRef &value, const QPoint &pos)
{
  // the window under the drop, the topmost first
  QMdiSubWindow *win = nullptr;
  const QList<QMdiSubWindow *> wins = m_mdi->subWindowList(QMdiArea::StackingOrder);
  for (auto it = wins.crbegin(); it != wins.crend() && !win; ++it)
    if ((*it)->isVisible() && (*it)->geometry().contains(pos))
      win = *it;
  if (MeterView *meter = meterViewOf(win))
    meter->setValue(value);
  else if (PoincareView *plot = poincareViewOf(win))
    plot->setValue(value);
  else if (win == m_graphWin)
    m_wid->graph()->setValueShown(value, true);
  else if (win == m_readingsWin)
    setTableShown(value, true);
  else if (FrontPanel *panel = panelOf(win))
  {
    // a front panel shows its own device's values: the big line
    if (!panel->controller() || panel->controller()->source() != value.source)
    {
      statusBar()->showMessage(tr("A front panel shows the values of its own device"), 4000);
      return;
    }
    const int before = panel->value().id;
    QList<int> small = panel->secondaries();
    if (const qsizetype at = small.indexOf(value.id); at >= 0)
      small[at] = before;
    panel->setValue(value);
    panel->setSecondaries(small);
    panel->controller()->setMinMaxValue(value.id);
  }
  else
  {
    // the free area: a new view for it
    QMenu menu(this);
    QAction *meter = menu.addAction(m_newMeterAction->icon(), tr("New &needle meter"));
    QAction *plot = menu.addAction(m_newPoincareAction->icon(), tr("New &Poincaré plot"));
    QAction *graph = menu.addAction(action_Graph->icon(), tr("In the &graph"));
    QAction *chosen = menu.exec(m_mdi->viewport()->mapToGlobal(pos));
    if (chosen == meter)
      newView(Added::Meter, value);
    else if (chosen == plot)
      newView(Added::Poincare, value);
    else if (chosen == graph)
    {
      m_wid->graph()->setValueShown(value, true);
      action_Graph->setChecked(true);
    }
  }
  updateOpenDevices();
}

QString MainWindow::sourceName(const QString &source) const
{
  const QString id = source.startsWith("device:") ? source.mid(7) : QString();
  if (const std::optional<MyDevice> device = m_wid->devices()->find(id))
    return device->name;
  return source;
}

QStringList MainWindow::tableSources() const
{
  if (!m_tableMeters.isEmpty())
    return m_tableMeters;
  QStringList all;
  for (const MeterController *ctl : m_session->controllers())
    all << ctl->source();
  return all;
}

bool MainWindow::tableShows(const ValueRef &value) const
{
  return tableSources().contains(value.source) && !m_readings->log()->hiddenValues().contains(value);
}

void MainWindow::setTableShown(const ValueRef &value, bool shown)
{
  const QString source = value.source;
  auto refOf = [source](int id) { return ValueRef(id, source); };
  ReadingsModel *log = m_readings->log();
  const MeterController *ctl = m_session->controller(source);
  const int count = ctl ? ctl->valueCount() : kMaxValues;
  QStringList sources = tableSources();
  if (shown && !sources.contains(source))
  {
    // a meter comes in with this value only
    for (int id = 0; id < count; ++id)
      log->setValueHidden(refOf(id), id != value.id);
    m_tableMeters = sources << source;
    applyTableSources();
    return;
  }
  log->setValueHidden(refOf(value.id), !shown);
  // none of its values left: it goes out of the table
  bool any = false;
  for (int id = 0; id < count; ++id)
    any = any || !log->hiddenValues().contains(refOf(id));
  if (!any)
  {
    for (int id = 0; id < kMaxValues; ++id)
      log->setValueHidden(refOf(id), false);
    sources.removeAll(source);
    m_tableMeters = sources;
    applyTableSources();
  }
}

void MainWindow::applyTableSources()
{
  ReadingsModel *log = m_readings->log();
  const int rows = log->maxRows();
  const QStringList sources = tableSources();
  // a column with the meter's name with more than one
  log->setDevices(sources.size() > 1, QString(), [this](const QString &source) { return sourceName(source); });
  // the rows the meters' recorders hold, merged by time, into the table's own store
  QList<LoggedReading> merged;
  for (const MeterController *ctl : m_session->controllers())
    if (sources.contains(ctl->source()))
      for (int i = 0; i < ctl->recorder()->readingCount(); ++i)
      {
        LoggedReading row = ctl->recorder()->readingAt(i);
        row.source = ctl->source();
        merged << row;
      }
  std::stable_sort(merged.begin(), merged.end(),
                   [](const LoggedReading &a, const LoggedReading &b) { return a.when < b.when; });
  log->setStore(nullptr);
  log->setMaxRows(rows);
  log->store()->logRows(merged);
  connectTableSources();
}

void MainWindow::connectTableSources()
{
  for (const QMetaObject::Connection &c : std::as_const(m_tableLinks))
    disconnect(c);
  m_tableLinks.clear();
  const QStringList sources = tableSources();
  ReadingsModel *log = m_readings->log();
  for (MeterController *ctl : m_session->controllers())
    if (sources.contains(ctl->source()))
    {
      m_tableLinks << connect(ctl, &MeterController::reading, log, &ReadingsModel::appendReading);
      // its alarms mark the table too
      m_tableLinks << connect(ctl, &MeterController::markRequested, log,
                              [log, ctl](const QColor &color, const QString &name, bool, bool table)
      {
        if (table)
          log->markLast(color, tr("%1: %2").arg(ctl->deviceName(), name));
      });
    }
}

DeviceSidebar::Value MainWindow::otherValueRow(const MeterController *ctl, int id) const
{
  const Reading r = ctl->lastReading(id);
  DeviceSidebar::Value row;
  row.id = id;
  if (r.msecs <= 0)
    row.name = tr("Value %1").arg(id + 1);
  else
  {
    row.name = ctl->valueName(id);
    row.text = readingText(r);
  }
  const ValueRef value(id, ctl->source());
  // a curve where one sees it
  if (!m_graphWin->isHidden())
    row.color = m_wid->graph()->drawnColor(value);
  // the views showing it, for the marks of its row
  if (QMdiSubWindow *win = m_panelWins.value(ctl->source()); win && !win->isHidden())
    if (const FrontPanel *panel = panelOf(win))
      row.onPanel = panel->value().id == id || panel->secondaries().contains(id);
  if (row.color.isValid())
    row.views << m_graphWin->windowTitle();
  if (row.onPanel)
    row.views << m_panelWins.value(ctl->source())->windowTitle();
  for (QMdiSubWindow *win : meterWindows())
    if (const MeterView *meter = meterViewOf(win); meter && !win->isHidden() && meter->value() == value)
    {
      ++row.meters;
      row.views << win->windowTitle();
    }
  for (QMdiSubWindow *win : poincareWindows())
    if (const PoincareView *plot = poincareViewOf(win); plot && !win->isHidden() && plot->value() == value)
    {
      ++row.poincares;
      row.views << win->windowTitle();
    }
  return row;
}

void MainWindow::updateOpenDevices()
{
  // pressed while a device of the bench is connected (also by SCPI, its menu)
  bool any = false;
  for (const MeterController *ctl : m_session->controllers())
    any = any || ctl->dmm()->isWanted();
  action_Connect->setChecked(any);
  action_Connect->setEnabled(!m_session->controllers().isEmpty());
  action_Reset->setEnabled(!m_session->controllers().isEmpty());
  if (!m_sidebarDock->isVisible())
    return;
  const qint64 now = QDateTime::currentMSecsSinceEpoch();
  QList<DeviceSidebar::OpenDevice> rows;
  for (const MeterController *ctl : m_session->controllers())
  {
    DeviceSidebar::OpenDevice row;
    row.id = ctl->deviceId();
    row.name = ctl->deviceName();
    row.connected = ctl->dmm()->isWanted();
    // the dot green while readings come in; why none come is on the front panel
    const Reading &r = ctl->lastReading();
    row.active = row.connected && ctl->dmm()->shortState().isEmpty() && r.msecs > 0 && now - r.msecs < 3000;
    // its values, with what the graph shows
    for (int value = 0; value < ctl->valueCount(); ++value)
      row.values << otherValueRow(ctl, value);
    rows << row;
  }
  m_sidebar->setOpenDevices(rows);
}

void MainWindow::pointToViews(const ValueRef &value)
{
  const MeterController *ctl = m_session->controller(value.source);
  if (!ctl)
    return;
  const DeviceSidebar::Value row = otherValueRow(ctl, value.id);
  QList<QMdiSubWindow *> wins;
  if (row.onPanel)
    wins << m_panelWins.value(value.source);
  if (row.color.isValid())
    wins << m_graphWin;
  for (QMdiSubWindow *win : meterWindows())
    if (const MeterView *meter = meterViewOf(win); meter && !win->isHidden() && meter->value() == value)
      wins << win;
  for (QMdiSubWindow *win : poincareWindows())
    if (const PoincareView *plot = poincareViewOf(win); plot && !win->isHidden() && plot->value() == value)
      wins << win;
  // a frame in the selection colour for a moment, above what the view draws
  for (QMdiSubWindow *win : std::as_const(wins))
  {
    auto *mark = new QFrame(win);
    mark->setAttribute(Qt::WA_TransparentForMouseEvents);
    mark->setStyleSheet(QString("QFrame { border: 3px solid %1; background: transparent; }")
                          .arg(palette().color(QPalette::Highlight).name()));
    mark->setGeometry(win->rect());
    mark->show();
    mark->raise();
    QTimer::singleShot(900, mark, &QObject::deleteLater);
  }
}

void MainWindow::valueMenu(const ValueRef &value, const QPoint &globalPos)
{
  QMenu menu(this);
  fillValueMenu(&menu, value);
  menu.exec(globalPos);
  updateOpenDevices();
}

void MainWindow::fillValueMenu(QMenu *menu, const ValueRef &value)
{
  ValueMenu::Target target;
  target.wid = m_wid;
  target.panel = [this](const QString &source) { return panelOf(m_panelWins.value(source)); };
  target.tableShown = [this](const ValueRef &v) { return tableShows(v); };
  target.setTableShown = [this](const ValueRef &v, bool on) { setTableShown(v, on); };
  target.addAlarm = [this](const ValueRef &v) { addAlarm(v); };
  target.editLimits = [this](const ValueRef &v)
  {
    if (MeterController *ctl = m_session->controller(v))
      editLimits(ctl, false, v.id);
  };
  target.newMeter = [this](const ValueRef &v) { newMeter(v); };
  target.newPoincare = [this](const ValueRef &v) { newView(Added::Poincare, v); };
  target.show = [this](ValueMenu::View view, const QString &source)
  {
    QMdiSubWindow *panel = m_panelWins.value(source);
    if (view == ValueMenu::View::Lcd && panel)
    {
      panel->show();
      m_mdi->setActiveSubWindow(panel);
      m_arranger->arrange();
      return;
    }
    QAction *action = view == ValueMenu::View::Lcd   ? m_displayAction
                    : view == ValueMenu::View::Table ? m_readingsAction
                                                      : action_Graph;
    action->setChecked(true);
  };
  ValueMenu::fill(menu, target, value);
}

void MainWindow::editPanelValues(FrontPanel *panel)
{
  MeterController *ctl = panel->controller();
  QStringList names;
  for (int id = 0; id < ctl->valueCount(); ++id)
    names << otherValueRow(ctl, id).name;
  PanelValuesDlg dlg(names, panel->value().id, panel->secondaries(), this);
  QPointer<FrontPanel> open(panel);   // they may close meanwhile
  QPointer<MeterController> meter(ctl);
  if (dlg.exec() != QDialog::Accepted || !open || !meter)
    return;
  panel->setValue(ValueRef(dlg.main(), ctl->source()));
  // its min/max memory follows the big line
  ctl->setMinMaxValue(dlg.main());
  panel->setSecondaries(dlg.secondaries());
  updateOpenDevices();
}

void MainWindow::deviceSettings(const QString &id, bool settingsTab)
{
  const std::optional<MyDevice> device = m_wid->devices()->find(id);
  if (!device)
    return;
  DeviceSettingsDlg dlg(device->name, this);
  dlg.settings()->setPorts(Transport::availablePorts() + m_wid->settings()->customPorts());
  dlg.settings()->setSigrokExe(m_wid->settings()->getString("Port settings/sigrok_exe", "sigrok-cli"));
  dlg.settings()->setCalcInputs(m_session->calcInputs());
  dlg.load(device->keys);
  dlg.setStatus([this, id] { return deviceStatus(id); });
  // Connect, as the sidebar's menu
  dlg.setConnect([this, id] { return m_sidebar->isConnected(id); }, [this, id] { openBeside(id); });
  if (settingsTab)
    dlg.showTab(DeviceSettingsDlg::SettingsTab);
  // the help beside the dialog: a child of it, the dialog blocks the others
  connect(&dlg, &DeviceSettingsDlg::helpRequested, &dlg, [this, &dlg](const QString &page)
  {
    auto *help = new HelpDlg(m_wid->settings(), &dlg);
    help->setAttribute(Qt::WA_DeleteOnClose);
    help->showPage(page);
    help->show();
  });
  if (dlg.exec() != QDialog::Accepted)
    return;
  QVariantMap keys = dlg.keys();
  keys.insert("Port settings/device", SerialDevice::stableDevice(keys.value("Port settings/device").toString()));
  if (DeviceLibrary::entryKeys(keys) == device->keys)
    return;   // nothing changed: the meter stays connected, the recording too
  m_wid->devices()->update(id, keys);
  // a device connected takes them at once: connected anew, its panel stays
  // as it was; its recording not saved: only when asked, else next time
  MeterController *ctl = m_session->controller("device:" + id);
  if (ctl && !keepRecordingOf(id, tr("The changes connect it anew, which discards its recording, which is not saved.")))
  {
    statusBar()->showMessage(tr("%1: the changes take effect when it is connected next").arg(ctl->deviceName()), 6000);
    return;
  }
  if (ctl)
  {
    if (QMdiSubWindow *win = m_panelWins.value(ctl->source()))
    {
      FrontPanel *panel = panelOf(win);
      PanelState state;
      state.value = panel->value();
      state.folded = panel->isFolded();
      if (panel->secondariesChosen())
        state.secondaries = panel->secondaries();
      state.shown = !win->isHidden();
      if (m_panelLcd.contains(ctl->source()))
        state.lcdVariant = m_panelLcd.value(ctl->source());
      m_panelStates.insert(id, state);
    }
    m_session->close(id);
    openBeside(id);
  }
}

QString MainWindow::deviceStatus(const QString &id) const
{
  const std::optional<MyDevice> device = m_wid->devices()->find(id);
  if (!device)
    return QString();
  auto para = [](const QString &text) { return QString("<p>%1</p>").arg(text.toHtmlEscaped()); };
  const MeterController *ctl = m_session->controller("device:" + id);
  if (!ctl)
  {
    const MeterController *owner = placeOwners().value(DeviceLibrary::place(device->keys));
    return para(owner ? tr("Its port is in use by %1.").arg(owner->deviceName())
                      : tr("Not connected. Connect takes it out of My devices."));
  }
  QString state;
  const Reading last = ctl->lastReading();
  const qint64 ago = last.msecs > 0 ? (QDateTime::currentMSecsSinceEpoch() - last.msecs) / 1000 : -1;
  switch (ctl->dmm()->linkState())
  {
    case MeterConnection::LinkState::Closed:     state = tr("Not connected."); break;
    case MeterConnection::LinkState::Connecting: state = tr("Connecting ..."); break;
    case MeterConnection::LinkState::Timeout:    state = tr("Connected, but the meter sends nothing."); break;
    case MeterConnection::LinkState::Error:      state = tr("The connection went away; QtDMM tries again."); break;
    case MeterConnection::LinkState::Connected:
      state = ago >= 0 && ago < 3 ? tr("Connected, readings are coming in.")
                                  : tr("Connected, no reading for %n s.", "", int(qMax(qint64(0), ago)));
      break;
  }
  QString html = para(state);
  if (ago >= 0)
    html += para(tr("Last reading %n s ago.", "", int(ago)));
  if (last.flags & SampleFlag::LowBattery)
    html += para(tr("The meter's battery is low."));
  // its values with their readings
  QString rows;
  for (int value = 0; value < ctl->valueCount(); ++value)
  {
    const DeviceSidebar::Value row = otherValueRow(ctl, value);
    rows += QString("<tr><td>%1&nbsp;&nbsp;</td><td>%2&nbsp;&nbsp;</td><td align=\"right\">%3</td></tr>")
              .arg(value + 1).arg(row.name.toHtmlEscaped(), row.text.toHtmlEscaped());
  }
  if (!rows.isEmpty())
    html += "<table cellspacing=\"0\" cellpadding=\"2\">" + rows + "</table>";
  return html;
}

void MainWindow::openBeside(const QString &id, bool waitForDevice)
{
  const std::optional<MyDevice> device = m_wid->devices()->find(id);
  if (!device)
    return;
  // one port, one reader
  if (const MeterController *owner = placeOwners().value(DeviceLibrary::place(device->keys)))
  {
    statusBar()->showMessage(owner->deviceId() == id ? tr("%1 is connected already").arg(device->name)
                                                     : tr("%1: its port is in use by %2").arg(device->name, owner->deviceName()),
                             4000);
    return;
  }
  QString error;
  if (const MeterController *ctl = m_session->open(id, &error, waitForDevice); ctl && !ctl->dmm()->isOpen())
    statusBar()->showMessage(tr("%1: waiting for the device").arg(device->name), 4000);
  else if (ctl)
    statusBar()->showMessage(tr("%1 connected").arg(device->name), 4000);
  // one that does not connect said why in the status bar already
  else if (!error.startsWith("-240,"))
    m_error->setText(error.section(',', 1));
}

void MainWindow::syncArrangeActions()
{
  m_arrangeTop->setChecked(m_arranger->mode() == MdiArranger::DisplaysOnTop);
  m_arrangeLeft->setChecked(m_arranger->mode() == MdiArranger::DisplaysOnLeft);
  m_arrangeFixed->setChecked(m_arranger->mode() == MdiArranger::Fixed);
  m_arrangeFree->setChecked(m_arranger->mode() == MdiArranger::Free);
  m_titleBars->setChecked(m_arranger->titleBarsHidden());
}

void MainWindow::updateToolbarButton()
{
  // an arrow where the toolbar goes or comes from
  const bool hidden = m_toolbars->isChecked();
  m_toolbarButton->setArrowType(hidden ? Qt::DownArrow : Qt::UpArrow);
  const QString key = m_toolbars->shortcut().toString(QKeySequence::NativeText);
  const QString tip = hidden ? tr("Show the toolbar") : tr("Hide the toolbar");
  m_toolbarButton->setToolTip(key.isEmpty() ? tip : QString("%1 (%2)").arg(tip, key));
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
  // the front panels of devices no longer connected: their keys go
  // (an id may have '-' in it: those kept are compared whole)
  const QStringList open = cfg->getString("Windows/open-devices").split(' ', Qt::SkipEmptyParts);
  auto kept = [&open](const QString &key)
  {
    return std::any_of(open.cbegin(), open.cend(), [&key](const QString &id)
    { return key.startsWith("Windows/panel-" + id + '-') || key == "Windows/geometry-panel-" + id; });
  };
  for (const QString &key : cfg->keysIn("Windows"))
    if ((key.startsWith(QLatin1String("Windows/panel-")) || key.startsWith(QLatin1String("Windows/geometry-panel-")))
        && !kept(key))
      cfg->remove(key);
}

void MainWindow::applyWorkspace(const WorkspaceGet &get)
{
  setDesign(Designs::fromName(get("Windows/design", QString("dark-metal")).toString()));
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
  // the meters the workspace had connected, with their panels: opened now,
  // at the start a second later (openPending()); the window's meter before
  // 26.4 first, its values by an empty source
  m_pendingOpen = get("Windows/open-devices", QString()).toString().split(' ', Qt::SkipEmptyParts);
  const QString old = std::exchange(m_oldDevice, QString());
  if (!old.isEmpty() && !m_pendingOpen.contains(old))
    m_pendingOpen.prepend(old);
  auto valueOf = [old](const QString &text)
  {
    const ValueRef v = ValueRef::fromString(text);
    return v.source.isEmpty() && !old.isEmpty() ? ValueRef(v.id, "device:" + old) : v;
  };
  for (const QString &id : std::as_const(m_pendingOpen))
  {
    const QString key = "Windows/panel-" + id;
    PanelState state;
    state.value = ValueRef::fromString(get(key + "-value", QString("0")).toString());
    state.folded = get(key + "-folded", false).toBool();
    const QString small = get(key + "-secondary-values", QStringLiteral("-")).toString();
    if (small != QLatin1String("-"))
    {
      state.secondaries = QList<int>();
      for (const QString &v : small.split(' ', Qt::SkipEmptyParts))
        state.secondaries->append(v.toInt());
    }
    state.shown = get(key + "-shown", true).toBool();
    if (const QString lcd = get(key + "-lcd", QString()).toString(); !lcd.isEmpty())
      state.lcdVariant = LcdWidget::lcdVariantFromName(lcd);
    state.geometry = get("Windows/geometry-panel-" + id, QString()).toString();
    if (QMdiSubWindow *win = m_panelWins.value("device:" + id))
      applyPanelState(win, state);
    else
      m_panelStates.insert(id, state);
  }
  // the analog meters: the first one's style ("" for the settings'), the
  // ones added ("2 3") with their values and styles; before the layout,
  // which names their windows
  const auto styleOf = [](const QVariant &v) { return v.toString().isEmpty() ? -1 : v.toInt(); };
  meterViewOf(m_meterWin)->setStyle(styleOf(get("Windows/meter-style", QString())));
  for (QMdiSubWindow *w : QList<QMdiSubWindow *>(m_added))
    removeAddedWindow(w);
  for (const QString &number : get("Windows/meters", QString()).toString().split(' ', Qt::SkipEmptyParts))
  {
    const QString key = "Windows/meter-" + number;
    QMdiSubWindow *win = addViewWindow(Added::Meter, valueOf(get(key + "-value", QString()).toString()), number.toInt());
    meterViewOf(win)->setStyle(styleOf(get(key + "-style", QString())));
    win->show();
  }
  for (const QString &number : get("Windows/poincares", QString()).toString().split(' ', Qt::SkipEmptyParts))
    addViewWindow(Added::Poincare, valueOf(get("Windows/poincare-" + number + "-value", QString()).toString()),
                  number.toInt())->show();
  const QString layout = get("Windows/layout", QString()).toString();
  if (!m_arranger->setLayoutText(layout))
  {
    m_arranger->setLayoutText(QString());   // a broken one: the rule
    // or one naming panels not there yet
    if (!m_pendingOpen.isEmpty())
      m_pendingLayout = layout;
  }
  m_arranger->setTitleBarsHidden(get("Windows/title-bars-hidden", !free).toBool());
  m_toolbars->setChecked(get("Windows/toolbar-hidden", false).toBool());
  if (free)
  {
    QMap<QString, QString> geometry;
    for (QMdiSubWindow *w : meterWindows() + poincareWindows() + QList<QMdiSubWindow *>{ m_graphWin, m_readingsWin })
      geometry[w->objectName()] = get("Windows/geometry-" + w->objectName(), QString()).toString();
    // once the window is shown and the area has its size: the automatic
    // layout for a start, then the stored positions on top of it
    QTimer::singleShot(0, this, [this, geometry]
    {
      m_arranger->arrangeNow();
      for (QMdiSubWindow *w : meterWindows() + poincareWindows() + QList<QMdiSubWindow *>{ m_graphWin, m_readingsWin })
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
  m_basketAction->setChecked(get("Windows/basket", false).toBool());
  m_wid->graph()->setReferenceSetting(get("Windows/graph-reference", QString()).toString());
  m_wid->graph()->setMinimapVisible(get("Windows/graph-minimap", true).toBool());
  m_wid->graph()->setEnvelopeVisible(get("Windows/graph-envelope", false).toBool());
  // "1@device:ab12cd34" (ValueRef)
  meterViewOf(m_meterWin)->setValue(valueOf(get("Windows/meter-value", 0).toString()));
  poincareViewOf(m_poincareWin)->setValue(valueOf(get("Windows/poincare-value", 0).toString()));
  // the axis of each value in the graph, "1@device:a:right 0@device:b:left";
  // before 26.4 the window's meter by its id alone ("1:right"); missing: automatic
  {
    const QStringList axes = get("Windows/graph-value-axes", QString()).toString().split(' ', Qt::SkipEmptyParts);
    for (int id = 1; id <= 3; ++id)
      m_wid->graph()->setValueAxis(id, GraphWidget::AxisAuto);
    for (const QString &entry : axes)
    {
      const qsizetype colon = entry.lastIndexOf(':');
      if (colon < 0)
        continue;
      const QString side = entry.mid(colon + 1);
      m_wid->graph()->setValueAxis(valueOf(entry.left(colon)),
                                   side == "left" ? GraphWidget::AxisLeft
                                   : side == "right" ? GraphWidget::AxisRight
                                   : side == "third" ? GraphWidget::AxisThird : GraphWidget::AxisAuto);
    }
  }
  // the graph's values, of any meter: "0@device:a 2@device:a 0@device:b";
  // before 26.4 the window's ids ("1 2") with the main value apart
  {
    QList<ValueRef> values;
    const QString list = get("Windows/graph-values", QStringLiteral("-")).toString();
    if (list != QLatin1String("-"))
      for (const QString &text : list.split(' ', Qt::SkipEmptyParts))
        values << valueOf(text);
    else
    {
      if (get("Windows/graph-main-shown", true).toBool())
        values << valueOf("0");
      for (const QString &text : get("Windows/graph-shown-values", QString()).toString().split(' ', Qt::SkipEmptyParts))
        values << valueOf(text);
    }
    // a file from before 26.4 names no meter: its values are those of a meter
    // connected. Without any they are none: the first meter connected takes
    // the empty graph
    if (list == QLatin1String("-") && !m_session->controllers().isEmpty())
      for (ValueRef &v : values)
        if (v.source.isEmpty())
          v.source = m_session->controllers().first()->source();
    values.removeIf([](const ValueRef &v) { return v.source.isEmpty(); });
    m_wid->graph()->setValues(values);
  }
  // the values the readings table leaves out, "1 2", another meter's
  // "1@device:ab12cd34"; none at first. The other meters in it, by source
  {
    const QStringList hidden = get("Windows/table-hidden-values", QString()).toString().split(' ', Qt::SkipEmptyParts);
    ReadingsModel *log = m_readings->log();
    for (const ValueRef &v : log->hiddenValues())
      log->setValueHidden(v, false);
    for (const QString &text : hidden)
      log->setValueHidden(valueOf(text), true);
    // the meters in it; none: all connected. Before 26.4 the window's always
    // and the others apart (table-sources)
    const QString devices = get("Windows/table-devices", QStringLiteral("-")).toString();
    if (devices != QLatin1String("-"))
      m_tableMeters = devices.split(' ', Qt::SkipEmptyParts);
    else
    {
      m_tableMeters = get("Windows/table-sources", QString()).toString().split(' ', Qt::SkipEmptyParts);
      if (!m_tableMeters.isEmpty() && !old.isEmpty())
        m_tableMeters.prepend("device:" + old);
    }
    applyTableSources();
  }
  // an unchecked action did not toggle: hide its window explicitly
  for (QAction *a : { m_meterAction, action_Graph, m_readingsAction, m_poincareAction, m_basketAction })
    qobject_cast<QWidget *>(a->property("window").value<QObject *>())->setVisible(a->isChecked());
  if (!m_displayAction->isChecked())
    for (QMdiSubWindow *win : std::as_const(m_panelWins))
      win->hide();
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
  set("Windows/toolbar-hidden", m_toolbars->isChecked());
  set("Windows/design", Designs::name(Designs::current()));
  if (m_arranger->mode() == MdiArranger::Free)
    for (QMdiSubWindow *w : meterWindows() + poincareWindows() + QList<QMdiSubWindow *>{ m_graphWin, m_readingsWin }
                            + panelWindows())
    {
      const QRect g = w->geometry();
      set("Windows/geometry-" + w->objectName(),
          QString("%1 %2 %3 %4").arg(g.x()).arg(g.y()).arg(g.width()).arg(g.height()));
    }
  QStringList open;
  for (QMdiSubWindow *win : std::as_const(m_panelWins))
  {
    const QString id = win->objectName().mid(QStringLiteral("panel-").size());
    const QString key = "Windows/panel-" + id;
    open << id;
    set(key + "-value", ValueRef(panelOf(win)->value().id).toString());
    set(key + "-folded", panelOf(win)->isFolded());
    if (panelOf(win)->secondariesChosen())
    {
      QStringList small;
      for (int v : panelOf(win)->secondaries())
        small << QString::number(v);
      set(key + "-secondary-values", small.join(' '));
    }
    set(key + "-shown", !win->isHidden());
    // its own LCD colours, none: the settings'
    const QString source = "device:" + id;
    set(key + "-lcd", m_panelLcd.contains(source)
                        ? LcdWidget::lcdVariantName(static_cast<LcdWidget::LcdVariant>(m_panelLcd.value(source)))
                        : QString());
  }
  set("Windows/open-devices", open.join(' '));
  set("Display/show", m_displayAction->isChecked());
  set("Windows/meter", m_meterAction->isChecked());
  set("Windows/readings", m_readingsAction->isChecked());
  set("Windows/poincare", m_poincareAction->isChecked());
  set("Windows/basket", m_basketAction->isChecked());
  set("MainWindow/show-graph", action_Graph->isChecked());
  set("Windows/graph-reference", m_wid->graph()->referenceSetting());
  set("Windows/graph-minimap", m_wid->graph()->minimapVisible());
  set("Windows/graph-envelope", m_wid->graph()->envelopeVisible());
  set("Windows/meter-value", meterViewOf(m_meterWin)->value().toString());
  const auto styleText = [](int style) { return style < 0 ? QString() : QString::number(style); };
  set("Windows/meter-style", styleText(meterViewOf(m_meterWin)->style()));
  QStringList meters;
  for (QMdiSubWindow *w : meterWindows().mid(1))
  {
    const QString number = w->objectName().section('-', 1);
    meters << number;
    set("Windows/meter-" + number + "-value", meterViewOf(w)->value().toString());
    set("Windows/meter-" + number + "-style", styleText(meterViewOf(w)->style()));
  }
  set("Windows/meters", meters.join(' '));
  set("Windows/poincare-value", poincareViewOf(m_poincareWin)->value().toString());
  QStringList plots;
  for (QMdiSubWindow *w : poincareWindows().mid(1))
  {
    const QString number = w->objectName().section('-', 1);
    plots << number;
    set("Windows/poincare-" + number + "-value", poincareViewOf(w)->value().toString());
  }
  set("Windows/poincares", plots.join(' '));
  GraphWidget *graph = m_wid->graph();
  QStringList shown;
  for (const ValueRef &v : graph->values())
    shown << v.toString();
  set("Windows/graph-values", shown.join(' '));
  QStringList tableHidden;
  for (const ValueRef &v : m_readings->log()->hiddenValues())
    tableHidden << v.toString();
  tableHidden.sort();
  set("Windows/table-hidden-values", tableHidden.join(' '));
  set("Windows/table-devices", m_tableMeters.join(' '));
  // the axes chosen, of the graph's meter also for its values not shown
  QList<ValueRef> withAxis;
  for (int id = 1; id <= 3; ++id)
    withAxis << ValueRef(id, graph->baseSource());
  for (const ValueRef &v : graph->otherValues())
    withAxis << v;
  QStringList axes;
  for (const ValueRef &v : std::as_const(withAxis))
    if (graph->valueAxis(v) != GraphWidget::AxisAuto)
    {
      const GraphWidget::ValueAxis axis = graph->valueAxis(v);
      axes << QString("%1:%2").arg(v.toString(), axis == GraphWidget::AxisLeft ? "left" : axis == GraphWidget::AxisRight ? "right" : "third");
    }
  set("Windows/graph-value-axes", axes.join(' '));
}

// A workspace file (WorkspaceFile): the window's keys, the devices connected,
// a name, a description and a picture.
QString MainWindow::basketName(const QString &what, const QString &suffix) const
{
  // "2026-11-08 0714 Sim A recording.csv": sorts by time, says what it is
  const QString device = m_session->lead() ? m_session->lead()->deviceName() : QString();
  QString name = QDateTime::currentDateTime().toString("yyyy-MM-dd HHmm") + (device.isEmpty() ? "" : " " + device)
                 + " " + what;
  name.replace(QRegularExpression(R"([/\\:*?"<>|])"), "_");
  return BasketWidget::freeName(m_basket->folder(), name + "." + suffix);
}

void MainWindow::graphToBasket()
{
  const QString csv = basketName(tr("recording"), "csv");
  if (!m_wid->writeRecordingTo(csv))
    return;
  // the picture beside it, with the same name
  const QString png = BasketWidget::freeName(m_basket->folder(), QFileInfo(csv).completeBaseName() + ".png");
  if (!m_wid->graph()->exportImageFile(png))
    Q_EMIT m_wid->error(tr("Project: cannot write %1").arg(QDir::toNativeSeparators(png)));
  else
    Q_EMIT m_wid->error(tr("Into the project: %1").arg(QFileInfo(csv).completeBaseName()));
}

void MainWindow::exportDialog(int view)
{
  ExportDlg dlg(static_cast<ExportDlg::What>(view), m_basket->folder(), m_wid->settings(), this);
  if (dlg.exec() != QDialog::Accepted)
    return;
  QString path = dlg.file();
  if (dlg.toBasket())
    path = basketName(dlg.what() == ExportDlg::Table     ? tr("readings")
                      : dlg.what() == ExportDlg::Picture ? tr("graph")
                                                         : tr("recording"), dlg.format());
  bool ok = false;
  QString error;
  switch (dlg.what())
  {
    case ExportDlg::Recording:
      ok = m_wid->writeRecordingTo(path, dlg.everyReading());   // says why itself
      break;
    case ExportDlg::Table:
      ok = m_readings->log()->writeAny(path, &error);
      break;
    case ExportDlg::Picture:
      ok = m_wid->graph()->exportImageFile(path);
      break;
  }
  QString labPlotError;
  if (ok && dlg.openInLabPlot()
      && !LabPlot::open(path, m_wid->settings()->getString("Programs/labplot"), &labPlotError))
    Q_EMIT m_wid->error(labPlotError);
  else if (ok)
    Q_EMIT m_wid->error(tr("Exported to %1").arg(QDir::toNativeSeparators(path)));
  else if (dlg.what() != ExportDlg::Recording)
    Q_EMIT m_wid->error(tr("Export failed: %1").arg(error.isEmpty() ? QDir::toNativeSeparators(path) : error));
}

void MainWindow::labPlotLive()
{
  const QString program = m_wid->settings()->getString("Programs/labplot");
  QString error;
  if (m_live && m_live->isRunning())   // again: the same project
  {
    if (!LabPlot::open(m_live->projectPath(), program, &error))
      Q_EMIT m_wid->error(error);
    return;
  }
  QPointer<MeterController> lead = m_session->lead();
  if (!lead || lead->recorder()->state() != RecordingStore::Record)
  {
    Q_EMIT m_wid->error(tr("Live in LabPlot: start a recording first"));
    return;
  }
  QString dir = m_basket->folder();
  if (dir.isEmpty())
  {
    dir = QDir(QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation)).filePath("QtDMM");
    QDir().mkpath(dir);
  }
  // "2026-11-09 1030 Sim A live.csv" and ".lml" beside it, both new
  QString name = QDateTime::currentDateTime().toString("yyyy-MM-dd HHmm") + " " + lead->deviceName() + " "
                 + tr("live");
  name.replace(QRegularExpression(R"([/\\:*?"<>|])"), "_");
  QString base = name;
  auto taken = [&dir](const QString &b)
  {
    return QFileInfo::exists(QDir(dir).filePath(b + ".csv")) || QFileInfo::exists(QDir(dir).filePath(b + ".lml"));
  };
  for (int n = 2; taken(base); ++n)
    base = QString("%1 (%2)").arg(name).arg(n);
  if (m_live)
    m_live->deleteLater();
  m_live = new LabPlotLive([lead] { return lead ? lead->recording() : Recording(); },
                           [lead] { return lead && lead->recorder()->state() == RecordingStore::Record; }, this);
  connect(m_live, &LabPlotLive::stopped, this, [this]
  {
    Q_EMIT m_wid->error(tr("Live in LabPlot: the recording ended, the file has all of it"));
    updateRecorderActions();
  });
  const QString csv = QDir(dir).filePath(base + ".csv"), project = QDir(dir).filePath(base + ".lml");
  if (!m_live->start(csv, project, &error))
  {
    Q_EMIT m_wid->error(tr("Live in LabPlot: %1").arg(error));
    return;
  }
  if (!LabPlot::open(project, program, &error))
    Q_EMIT m_wid->error(error);
  else
    Q_EMIT m_wid->error(tr("Live in LabPlot: %1").arg(QDir::toNativeSeparators(csv)));
}

void MainWindow::tableToBasket()
{
  const QString csv = basketName(tr("readings"), "csv");
  QString error;
  if (!m_readings->log()->write(csv, &error))
    Q_EMIT m_wid->error(tr("Project: cannot write %1: %2").arg(QDir::toNativeSeparators(csv), error));
  else
    Q_EMIT m_wid->error(tr("Into the project: %1").arg(QFileInfo(csv).fileName()));
}

void MainWindow::saveWorkspace()
{
  Settings *cfg = m_wid->settings();
  // the picture: the windows as they stand
  const QPixmap shot = m_mdi->grab().scaledToWidth(480, Qt::SmoothTransformation);
  WorkspaceDlg dlg(m_workspaceName, m_workspaceDescription, shot, this);
  if (dlg.exec() != QDialog::Accepted)
    return;
  // a project basket open: the bench goes into it
  const QString dir = !m_basket->folder().isEmpty()
                        ? m_basket->folder()
                        : cfg->getString("Windows/workspace-dir",
                                         QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation));
  // the file of the workspace in use, else one named after it
  QString base = dlg.name();
  base.replace(QRegularExpression(R"([/\\:*?"<>|])"), "_");
  const QString proposal = !m_workspaceFile.isEmpty() && (dlg.name() == m_workspaceName || base.isEmpty())
                             ? m_workspaceFile
                             : QDir(dir).filePath((base.isEmpty() ? QString("untitled") : base) + "." + kWorkspaceSuffix);
  // a bench loaded from elsewhere: into the open basket all the same
  QString file = QFileDialog::getSaveFileName(this, tr("Save bench"), ProjectFolder::proposal(proposal),
                                              tr("QtDMM bench (*.%1)").arg(kWorkspaceSuffix));
  if (file.isEmpty())
    return;
  if (QFileInfo(file).suffix().isEmpty())
    file += QString(".") + kWorkspaceSuffix;

  WorkspaceFile ws;
  ws.name = dlg.name().isEmpty() ? QFileInfo(file).completeBaseName() : dlg.name();
  ws.description = dlg.description();
  ws.width = width();
  ws.height = height();
  storeWorkspace([&ws](const QString &key, const QVariant &value) { ws.keys.insert(key, value); });
  // the devices as My devices has them: a copy goes with the file
  const DeviceLibrary *lib = m_wid->devices();
  for (const QString &id : ws.keys.value("Windows/open-devices").toString().split(' '))
    if (const std::optional<MyDevice> d = lib->find(id))
      ws.devices << *d;
  // JPEG: a screenshot of gradients takes a fifth of a PNG's room
  QBuffer jpeg(&ws.thumbnail);
  jpeg.open(QIODevice::WriteOnly);
  shot.save(&jpeg, "JPG", 80);
  QString error;
  if (!ws.write(file, &error))
  {
    QMessageBox::warning(this, tr("QtDMM: Save bench"), tr("Could not write %1.").arg(QDir::toNativeSeparators(file)));
    return;
  }
  cfg->setString("Windows/workspace-dir", QFileInfo(file).absolutePath());
  recentWorkspaces().add(file);
  setCurrentWorkspace(file);
  updateRecentWorkspaces();
  Q_EMIT m_wid->error(tr("Bench saved to %1").arg(QDir::toNativeSeparators(file)));
}

void MainWindow::loadWorkspace()
{
  Settings *cfg = m_wid->settings();
  const QString dir = !m_basket->folder().isEmpty()
                        ? m_basket->folder()
                        : cfg->getString("Windows/workspace-dir",
                                         QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation));
  const QString file = QFileDialog::getOpenFileName(this, tr("Load bench"), dir,
                                                    tr("QtDMM bench (*.%1)").arg(kWorkspaceSuffix));
  if (!file.isEmpty())
    loadWorkspaceFile(file);
}

bool MainWindow::loadWorkspaceFile(const QString &file)
{
  Settings *cfg = m_wid->settings();
  QString error;
  const std::optional<WorkspaceFile> ws = WorkspaceFile::read(file, &error);
  if (!ws)
  {
    QMessageBox::warning(this, tr("QtDMM: Load bench"), tr("%1: %2").arg(QDir::toNativeSeparators(file), error));
    return false;
  }
  DeviceLibrary *lib = m_wid->devices();
  // the set-up of the file: the devices connected that it does not have
  // close (version 1 only opened its own)
  const QStringList beside = ws->keys.value("Windows/open-devices").toString().split(' ', Qt::SkipEmptyParts);
  QList<MeterController *> closing;
  if (ws->version >= 2)
    for (MeterController *ctl : m_session->controllers())
      if (!beside.contains(ctl->deviceId()) && ctl->deviceId() != ws->windowDevice)
        closing << ctl;
  QStringList unsaved;
  for (const MeterController *ctl : std::as_const(closing))
    if (ctl->recorder()->state() == RecordingStore::View && ctl->recorder()->dirty())
      unsaved << ctl->deviceName();
  if (!unsaved.isEmpty()
      && QMessageBox::question(this, tr("QtDMM: Load bench"),
                               tr("The bench closes %1, its recording is not saved.").arg(unsaved.join(", ")),
                               QMessageBox::Discard | QMessageBox::Cancel, QMessageBox::Cancel) != QMessageBox::Discard)
    return false;
  // a device gone from My devices comes back from the copy, under its id:
  // the views name it so. A file may come from anyone: its alarms run no
  // program here (and keep the port open), the user sets them again
  QStringList restored, keyless;
  bool programsLeft = false;
  for (const MyDevice &d : ws->devices)
    if (!lib->find(d.id) && lib->add(d.name, d.keys, d.id) == d.id)
    {
      bool removed = false;
      lib->setRules(d.id, Alarm::listWithoutPrograms(d.alarms, &removed), d.limitTest);
      programsLeft = programsLeft || removed;
      // a Victron reads with its key, which no file has
      if (d.keys.value("Port settings/device").toString().startsWith(QLatin1String("ble "))
          && d.keys.value("Port settings/ble-key").toString().isEmpty())
        keyless << d.name;
      restored << d.name;
    }
  // the window's device of a file before 26.4: connected first
  if (lib->find(ws->windowDevice))
    m_oldDevice = ws->windowDevice;
  for (const MeterController *ctl : std::as_const(closing))
    m_session->close(ctl->deviceId());

  cfg->setString("Windows/workspace-dir", QFileInfo(file).absolutePath());
  // the size first: the arrangement fills the area it gets
  if (ws->width > 0 && ws->height > 0 && !isMaximized() && !isFullScreen())
  {
    resize(ws->width, ws->height);
    m_userSized = true;   // a size chosen on purpose: no growing
  }
  const QVariantMap keys = ws->keys;
  applyWorkspace([&keys](const QString &key, const QVariant &def) -> QVariant
  {
    const QVariant v = keys.value(key, def);
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
  openPending();
  recentWorkspaces().add(file);
  setCurrentWorkspace(file);
  updateRecentWorkspaces();
  QString text = tr("Bench loaded from %1").arg(QDir::toNativeSeparators(file));
  // the bench belongs to the project in whose folder it lies: loading it
  // opens that one, or closes the open one when it lies outside; no
  // question, the status bar says so
  const QString before = m_basket->folder();
  const QString project = BasketWidget::projectOf(file, before);
  if (project != before)
  {
    m_basket->setFolder(project);
    text += "; " + (project.isEmpty() ? tr("project %1 closed: the bench is not in it").arg(QFileInfo(before).fileName())
                                      : tr("project %1 opened: the bench is in it").arg(QFileInfo(project).fileName()));
  }
  if (!restored.isEmpty())
    text += "; " + tr("back in My devices: %1").arg(restored.join(", "));
  if (programsLeft)
    text += "; " + tr("the programs of their alarms were left out, set them again in Alarms...");
  if (!keyless.isEmpty())
    text += "; " + tr("%1 without its key: enter it in the device settings").arg(keyless.join(", "));
  Q_EMIT m_wid->error(text);
  return true;
}

void MainWindow::setCurrentWorkspace(const QString &file)
{
  const std::optional<WorkspaceFile> ws = file.isEmpty() ? std::nullopt : WorkspaceFile::read(file);
  m_workspaceFile = ws ? file : QString();
  m_workspaceName = ws ? ws->name : QString();
  m_workspaceDescription = ws ? ws->description : QString();
  m_wid->settings()->setString("Windows/workspace-current", m_workspaceFile);
  m_sidebar->setCurrentWorkspace(m_workspaceName, ws ? workspaceTooltip(file, *ws) : QString());
  updateWindowTitle();
}

QString MainWindow::workspaceTooltip(const QString &file, const WorkspaceFile &ws)
{
  QString tip = "<b>" + ws.name.toHtmlEscaped() + "</b>";
  if (!ws.description.isEmpty())
    tip += "<br>" + ws.description.toHtmlEscaped().replace('\n', "<br>");
  if (!ws.thumbnail.isEmpty())
    tip += QString("<br><img src=\"data:image/%1;base64,%2\" width=\"240\">")
             .arg(ws.thumbnail.startsWith("\x89PNG") ? "png" : "jpeg", QString::fromLatin1(ws.thumbnail.toBase64()));
  return tip + "<br><small>" + QDir::toNativeSeparators(file).toHtmlEscaped() + "</small>";
}

RecentFiles MainWindow::recentWorkspaces() const
{
  return RecentFiles(QDir(m_wid->settings()->configDir()).filePath("workspaces.conf"));
}

RecentFiles MainWindow::recentProjects() const
{
  return RecentFiles(QDir(m_wid->settings()->configDir()).filePath("projects.conf"), QStringLiteral("Projects/recent"),
                     20);
}

void MainWindow::updateRecentWorkspaces()
{
  // the favourites first, then the others used last
  const RecentFiles recent = recentWorkspaces();
  const QStringList files = recent.shown(m_wid->settings()->getInt("Sidebar/recent-workspaces", 10));
  const QStringList favorites = recent.favorites();
  // their names and pictures from the files
  QList<DeviceSidebar::Workspace> rows;
  for (const QString &file : files)
  {
    const std::optional<WorkspaceFile> ws = QFileInfo::exists(file) ? WorkspaceFile::read(file) : std::nullopt;
    rows.append({ file, ws ? ws->name : QString(), ws ? workspaceTooltip(file, *ws) : QString(), favorites.contains(file) });
  }
  m_sidebar->setWorkspaces(rows);
  m_recentMenu->clear();
  for (int i = 0; i < rows.size(); ++i)
  {
    const QString &file = files[i];
    // 1 ... 9 as the letters, then none
    const QString name = (rows[i].name.isEmpty() ? QFileInfo(file).completeBaseName() : rows[i].name).replace('&', "&&");
    QAction *a = m_recentMenu->addAction(i < 9 ? QString("&%1 %2").arg(i + 1).arg(name) : name);
    a->setToolTip(rows[i].tooltip.isEmpty() ? QDir::toNativeSeparators(file) : rows[i].tooltip);
    a->setEnabled(QFileInfo::exists(file));
    connect(a, &QAction::triggered, this, [this, file] { loadWorkspaceFile(file); });
  }
  m_recentMenu->setToolTipsVisible(true);
  m_recentMenu->menuAction()->setEnabled(!files.isEmpty());
  if (files.isEmpty())
    return;
  m_recentMenu->addSeparator();
  QAction *clear = m_recentMenu->addAction(QIcon::fromTheme("edit-clear-history"), tr("&Clear list"));
  connect(clear, &QAction::triggered, this, [this]
  {
    recentWorkspaces().clear();
    updateRecentWorkspaces();
  });
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
  if (isColumn(win))
    return QSize(19 * em(), 0);    // a column on the right
  if (isInstrument(win))
    return QSize(20 * em(), 0);    // next to the other instruments
  return QSize();
}

void MainWindow::growFor(QMdiSubWindow *win)
{
  if (m_grown.contains(win))
    return;
  m_grown.insert(win);
  // the front panels and the analog meter share the top row: the first of
  // them needs no room of its own
  if (isInstrument(win)
      && std::none_of(m_grown.cbegin(), m_grown.cend(), [this, win](const QObject *o)
                      { return o != win && isInstrument(qobject_cast<const QMdiSubWindow *>(o)); }))
    return;
  // the same for the readings table and the Poincaré plots in the column
  if (isColumn(win)
      && std::any_of(m_grown.cbegin(), m_grown.cend(), [this, win](const QObject *o)
                     { return o != win && isColumn(qobject_cast<const QMdiSubWindow *>(o)); }))
    return;
  const QSize before = size();
  autoGrow(growthFor(win));
  if (isColumn(win))
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
  m_content += delta;
  const QSize n(qMax(s.width(), qMin(m_content.width(), maxW)), qMax(s.height(), qMin(m_content.height(), maxH)));
  if (n != s)
    resizeOnScreen(n);
}

void MainWindow::resizeOnScreen(const QSize &n)
{
  const QRect av = screen()->availableGeometry();
  const QSize frameExtra = frameGeometry().size() - size();
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
  // the desktop switched between light and dark: the design afresh, or the
  // palettes set on the views and the sidebar's background stay the old
  // ones. Not again for the palette change the design itself makes
  if ((event->type() == QEvent::ThemeChange || event->type() == QEvent::ApplicationPaletteChange)
      && !m_designPending && (!m_designApplied.isValid() || m_designApplied.elapsed() > 500))
  {
    m_designPending = true;
    QTimer::singleShot(0, this, [this]
    {
      m_designPending = false;
      setDesign(Designs::current());
    });
  }
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

void MainWindow::updateControls()
{
  for (QMdiSubWindow *win : panelWindows())
  {
    // the keys below the LCD are part of its window
    FrontPanel *panel = panelOf(win);
    m_arranger->setContentAspect(win, LcdWidget::kMinAspect, LcdWidget::kMaxAspect, panel->keysHeight());
    // the first time a panel's keys appear, the window grows for them
    if (panel->keysShown() && !m_grown.contains(panel))
    {
      m_grown.insert(panel);
      autoGrow(QSize(0, 2 * em()));
    }
  }
  m_arranger->arrange();
}

void MainWindow::setDesign(int design)
{
  const auto d = static_cast<Designs::Design>(design);
  m_designApplied.start();
  Designs::apply(d);
  m_mdi->setBackground(Designs::areaBrush(d));
  QPalette empty = m_emptyStart->palette();
  empty.setBrush(QPalette::Window, Designs::areaBrush(d));
  m_emptyStart->setPalette(empty);
  // the instruments are drawn on a transparent background: the window's
  // (metal in Silver and Brushed metal) shows around them. The front
  // panels, not the LCDs: the meters' keys below them stand on the same sheet
  const QBrush frame = Designs::frameBrush(d);
  for (QMdiSubWindow *win : meterWindows())
    applyFrame(win->widget());
  applyFrame(m_readings, false);
  for (QMdiSubWindow *win : std::as_const(m_panelWins))
    applyFrame(win->widget());
  const Designs::GraphColors g = Designs::graphColors(d);
  m_wid->graph()->setThemeColors(g.background, g.grid, g.labels, g.data);
  for (QMdiSubWindow *win : poincareWindows())
    applyPoincareColors(static_cast<PoincarePlot *>(win->widget()));
  for (QAction *a : m_designMenu->actions())
    a->setChecked(a->data().toInt() == design);
  // at once, so the settings dialog shows the design of the menu (and its
  // OK does not set the one of before again)
  m_wid->settings()->setString("Windows/design", Designs::name(d));
  m_wid->settingsDialog()->guiPage()->setDesign(Designs::name(d));
}

void MainWindow::applyPoincareColors(PoincarePlot *plot)
{
  const Designs::Design d = Designs::current();
  const QBrush frame = Designs::frameBrush(d);
  const Designs::GraphColors g = Designs::graphColors(d);
  // the Poincaré plot on the background around the instruments, not on the
  // graph's: it stands next to the display and the meter. Not on a texture
  // though, the points would get lost in it: there on the graph's
  QBrush background = frame.style() != Qt::NoBrush ? frame : QApplication::palette().window();
  if (frame.style() == Qt::TexturePattern)
    background = g.background;
  plot->setThemeColors(background, g.grid, g.labels, g.data);
}

void MainWindow::applyFrame(QWidget *view, bool instrument)
{
  const Designs::Design design = Designs::current();
  const QBrush frame = instrument && design == Designs::Silver ? Designs::areaBrush(design) : Designs::frameBrush(design);
  const bool own = frame.style() != Qt::NoBrush;
  QPalette pal = view->palette();
  pal.setBrush(QPalette::Window, own ? frame : QApplication::palette().window());
  view->setPalette(pal);
  view->setAutoFillBackground(own);
  Designs::fitTexture(view, frame.style() == Qt::TexturePattern);
  view->update();
}

void MainWindow::storeDisplaySLOT()
{
  m_wid->settingsDialog()->guiPage()->setShowDisplay(m_displayAction->isChecked());
}

void MainWindow::connectBench(bool on)
{
  // all devices of the bench; the button stays pressed while one is
  for (MeterController *ctl : m_session->controllers())
    ctl->connectMeter(on);
  updateOpenDevices();
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
