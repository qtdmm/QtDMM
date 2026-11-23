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
// SPDX-License-Identifier: GPL-3.0-or-later
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
#include "ui/xyview.h"
#include "ui/graphmodel.h"
#include "ui/graphview.h"
#include "ui/settings/graphprefs.h"
#include "ui/views/graphwidget.h"
#include "ui/histogramview.h"
#include "ui/heatmapview.h"
#include "ui/dialogs/helpdlg.h"
#include "ui/dialogs/devicesettingsdlg.h"
#include "ui/devicesettings.h"
#include "ui/devicesidebar.h"
#include "ui/valuemenu.h"
#include "ui/tilebutton.h"
#include "ui/dialogs/adddevicedlg.h"
#include "device/metersetup.h"
#include "logic/formulanode.h"
#include "logic/generatornode.h"
#include "logic/integralnode.h"
#include "service/noderunner.h"
#include "ui/dialogs/integralnodedlg.h"
#include "logic/legacynodes.h"
#include "ui/dialogs/generatornodedlg.h"
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
#include "ui/views/lcdwidget.h"
#include "ui/views/poincareplot.h"
#include "ui/views/xyplot.h"
#include "ui/views/histogramplot.h"
#include "ui/views/heatmapplot.h"
#include "ui/views/colormap.h"
#include "ui/settings/settingsdialog.h"
#include "ui/settings/alarmprefs.h"
#include "ui/settings/generalprefs.h"
#include "ui/settings/guiprefs.h"
#include "ui/settings/keyboardprefs.h"
#include "ui/views/analogmeter.h"
#include "ui/views/basketwidget.h"

static const char *kWorkspaceSuffix = "qtdmm-workspace";
#include "ui/views/readingswidget.h"
#include "ui/dialogs/capturedlg.h"
#include "ui/dialogs/photodlg.h"
#include "core/photostore.h"
#include "ui/dialogs/logdlg.h"
#include "ui/dialogs/panelvaluesdlg.h"
#include "ui/dialogs/sourcesdlg.h"
#include "core/settings.h"
#include "core/alarm.h"
#include "ui/alarmbar.h"
#include "ui/updatebar.h"
#include "core/flatpakupdate.h"
#include "ui/alarmservice.h"
#include "ui/mdiarranger.h"
#include "service/metercontroller.h"
#include "device/devicetype.h"
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
#include <QDirIterator>
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

MainWindow::~MainWindow()
{
  // the members go first, the child windows after them: a window activated
  // meanwhile would look the graph up in a hash already gone
  disconnect(m_mdi, nullptr, this, nullptr);
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
  // without a leading meter Record, Live, Clear and the export are for the
  // file the graph used last shows
  m_wid->setFileView({ [this] { const GraphView *view = pictureGraph(); return view && view->showsFile(); },
                       [this] { if (GraphView *view = pictureGraph()) view->closeFile(); },
                       [this](const QString &path, bool everyReading)
                       { GraphView *view = pictureGraph(); return view && writeGraphRecording(view, path, everyReading); } });
  // a file loaded without a device: the start page makes way for its graph
  connect(m_wid, &InstanceWidget::recorderState, this, &MainWindow::updateEmptyStart);

  // Record while a meter is connected
  auto recordable = [this] { connectSLOT(!m_session->controllers().isEmpty()); };
  recordable();
  connect(m_session, &DeviceSession::opened, this, recordable);
  connect(m_session, &DeviceSession::closed, this, recordable);

  createStatusBar();
  placeWindow();

  // the meters connected last time
  QTimer::singleShot(1000, this, &MainWindow::openPending);
  // a new version: once a day, when allowed, after the start has settled
  connect(m_updateAction, &QAction::triggered, m_updateBar, &UpdateBar::checkNow);
  // the Windows installer: QtDMM closes as at Quit (asking for unsaved
  // data), then the installer replaces it and starts it again
  connect(m_updateBar, &UpdateBar::installReady, this, [this](const QString &path) {
    if (close() && !UpdateCheck::runInstaller(path))
      QMessageBox::warning(nullptr, tr("Update"),
                           tr("The installer could not be started: %1").arg(QDir::toNativeSeparators(path)));
  });
  // the AppImage, the macOS bundle or a build from the sources replaced,
  // the Flatpak updated: closes as at Quit, the new one starts (a program
  // file is started as an AppImage is, without its variables), with the
  // command line of this one (--config-id and the like)
  connect(m_updateBar, &UpdateBar::restartReady, this, [this](const QString &path) {
    auto restart = [&path, args = UpdateCheck::restartArguments()] {
      if (path.isEmpty())
        return FlatpakUpdate::restart(args);
      return path.endsWith(".app") ? UpdateCheck::restartBundle(path, args)
                                   : UpdateCheck::restartAppImage(path, args);
    };
    if (close() && !restart())
      QMessageBox::warning(nullptr, tr("Update"),
                           tr("QtDMM could not be started again: %1").arg(QDir::toNativeSeparators(path)));
  });
  QTimer::singleShot(5000, m_updateBar, &UpdateBar::startupCheck);
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
  // the Graph page: the graphs take its look
  connect(m_wid, &InstanceWidget::configChanged, this, [this]
  {
    for (GraphView *view : std::as_const(m_graphViews))
      applyGraphLook(view);
  });
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
  m_updateBar = new UpdateBar(m_wid->settings(), central);
  centralLayout->addWidget(m_updateBar);
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
  connect(addButton, &QToolButton::clicked, this, [this] { addDevice(); });

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

  // the first graph: hidden, never closed; the session widget has no window
  m_wid->hide();
  m_graphWin = addGraphWindow("graph", tr("Graph"), ValueRef());
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

  m_xyPlot = new XYPlot(this);
  m_xyPlot->setWindowTitle(tr("XY diagram"));
  auto *xy = new XYView(m_session, m_xyPlot, this);
  m_xyViews.insert(m_xyPlot, xy);
  applyPlotColors(m_xyPlot);
  m_xyPlot->setContextMenuPolicy(Qt::CustomContextMenu);
  m_xyWin = addView(m_xyPlot, m_xyPlot->windowTitle(), MdiArranger::Table, "xy");
  connect(m_xyPlot, &QWidget::customContextMenuRequested, this, [this](const QPoint &pos)
  {
    windowMenu(m_xyWin, m_xyPlot->mapToGlobal(pos));
  });

  m_histogram = new HistogramPlot(this);
  m_histogram->setWindowTitle(tr("Histogram"));
  auto *histogram = new HistogramView(m_session, m_histogram, this);
  m_histogramViews.insert(m_histogram, histogram);
  m_histogramWin = addView(m_histogram, m_histogram->windowTitle(), MdiArranger::Table, "histogram");
  applyPlotColors(m_histogram);
  m_histogram->setContextMenuPolicy(Qt::CustomContextMenu);
  connect(m_histogram, &QWidget::customContextMenuRequested, this, [this](const QPoint &pos)
  {
    windowMenu(m_histogramWin, m_histogram->mapToGlobal(pos));
  });

  m_heatmap = new HeatmapPlot(this);
  m_heatmap->setWindowTitle(tr("Heatmap"));
  auto *heatmap = new HeatmapView(m_session, m_heatmap, this);
  m_heatmapViews.insert(m_heatmap, heatmap);
  m_heatmapWin = addView(m_heatmap, m_heatmap->windowTitle(), MdiArranger::Table, "heatmap");
  applyPlotColors(m_heatmap);
  m_heatmap->setContextMenuPolicy(Qt::CustomContextMenu);
  connect(m_heatmap, &QWidget::customContextMenuRequested, this, [this](const QPoint &pos)
  {
    windowMenu(m_heatmapWin, m_heatmap->mapToGlobal(pos));
  });

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
    ProjectFolder::setByType(m_wid->settings()->getBool("Basket/by-type", true));
  };
  basketBase();
  connect(m_wid, &InstanceWidget::configChanged, this, basketBase);
  connect(m_basket, &BasketWidget::folderChanged, this, [this](const QString &folder)
  {
    m_wid->settings()->setString("Basket/folder", folder);
    // a message of before belongs to the project of before (a bench
    // switching it says so after this)
    for (QLabel *l : { m_error, m_info })
    {
      l->clear();
      l->setToolTip(QString());
    }
    m_wid->settings()->save();   // set only stages it: the next start reads the file
    if (!folder.isEmpty())
      recentProjects().add(folder);
    updateWindowTitle();
    updateProjectSidebar();
    // without a basket everything is as before
    for (QAction *a : { m_graphToBasket, m_tableToBasket })
      a->setVisible(!folder.isEmpty());
    // a closed project leaves nothing to show in its window
    if (folder.isEmpty() && m_basketAction)
      m_basketAction->setChecked(false);
    // the photos went into the project of before
    if (m_photoDlg && m_photoDlg->project() != folder)
      m_photoDlg->close();
  });
  connect(m_basket, &BasketWidget::error, m_wid, &InstanceWidget::error);
  connect(m_basket, &BasketWidget::photoRequested, this, &MainWindow::photoSetup);
  // made, opened or imported: its window shows (a bench switching the
  // project leaves the windows as the bench has them)
  connect(m_basket, &BasketWidget::projectOpened, this, [this] { m_basketAction->setChecked(true); });
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
      importRecording(path);
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
  m_newHistogramAction = new QAction(QIcon::fromTheme("qtdmm-histogram"), tr("Add &histogram"), this);
  m_newHistogramAction->setObjectName("action_NewHistogram");
  connect(m_newHistogramAction, &QAction::triggered, this, [this]
  {
    const MeterController *ctl = m_session->controllers().value(0);
    newView(Added::Histogram, ctl ? ValueRef(0, ctl->source()) : ValueRef(), true);
  });
  m_newHeatmapAction = new QAction(QIcon::fromTheme("qtdmm-heatmap"), tr("Add &heatmap"), this);
  m_newHeatmapAction->setObjectName("action_NewHeatmap");
  connect(m_newHeatmapAction, &QAction::triggered, this, [this]
  {
    const MeterController *ctl = m_session->controllers().value(0);
    const ValueRef first = ctl ? ValueRef(0, ctl->source()) : ValueRef();
    newView(Added::Heatmap, first, true);
  });
  m_newGraphAction = new QAction(action_Graph->icon(), tr("Add g&raph"), this);
  m_newGraphAction->setObjectName("action_NewGraph");
  m_newGraphAction->setWhatsThis(
    tr("<html><head/><body><p><span style=\" font-weight:600;\">Add graph</span></p>"
       "<p>Another graph window on the same readings, with its own curves, axes, time window, "
       "cursors and statistics; closing it removes it.</p></body></html>"));
  connect(m_newGraphAction, &QAction::triggered, this, [this]
  {
    const MeterController *ctl = m_session->controllers().value(0);
    newView(Added::Graph, ctl ? ValueRef(0, ctl->source()) : ValueRef(), true);
  });
  m_newXYAction = new QAction(QIcon::fromTheme("qtdmm-xy"), tr("Add &XY diagram"), this);
  m_newXYAction->setObjectName("action_NewXY");
  m_newXYAction->setToolTip(tr("Add an XY diagram"));
  connect(m_newXYAction, &QAction::triggered, this, [this]
  {
    const MeterController *ctl = m_session->controllers().value(0);
    newView(Added::XY, ctl ? ValueRef(0, ctl->source()) : ValueRef(), true);
  });
  m_xyAction = windowAction(m_xyWin, tr("&XY diagram"), "", "qtdmm-xy",
    tr("Plot one measured value against another in an XY diagram."));
  m_xyAction->setObjectName("action_WindowXY");
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
  m_histogramAction = windowAction(m_histogramWin, tr("H&istogram"), "", "qtdmm-histogram",
    tr("Show the distribution of recent readings for one measured value."));
  m_heatmapAction = windowAction(m_heatmapWin, tr("He&atmap"), "", "",
    tr("Plot three measured values: two for position and one for color."));
  m_heatmapAction->setIcon(QIcon::fromTheme("qtdmm-heatmap"));
  m_heatmapAction->setObjectName("action_WindowHeatmap");
  // the graph keeps its action from the .ui (toolbar button with Ctrl+3)
  bindWindowAction(action_Graph, m_graphWin);
  // graph and table have menus of their own: they begin with the window's
  // entry, as the window menu of the others
  auto hideWindow = [this](QMdiSubWindow *win) { return hideAction(win); };
  // the values of the open meters they show, with more than one
  m_graphSources = new QAction(tr("S&ources..."), this);
  m_tableSources = new QAction(tr("S&ources..."), this);
  connect(m_graphSources, &QAction::triggered, this, [this]
  {
    // the graph used last, else the first one
    if (GraphView *view = currentGraph())
      editGraphSources(view);
  });
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
  // the actions are for the graph used last, also the one whose window is active
  connect(m_mdi, &QMdiArea::subWindowActivated, this, [this](QMdiSubWindow *win)
  {
    if (GraphView *graphView = graphViewOf(win))
      setPictureGraph(graphView);
  });
  m_readings->setWindowActions({ hideWindow(m_readingsWin), m_tableSources, m_tableToBasket, m_tableExport });
  m_displayAction->setObjectName("action_WindowDisplay");
  m_meterAction->setObjectName("action_WindowMeter");
  m_readingsAction->setObjectName("action_WindowReadings");
  m_poincareAction->setObjectName("action_WindowPoincare");
  m_histogramAction->setObjectName("action_WindowHistogram");
  m_basketAction->setObjectName("action_WindowBasket");

  // a front panel for each meter, also those opened before the window
  connect(m_session, &DeviceSession::opened, this, &MainWindow::addPanel);
  // a meter of the table opened again: its readings come in again
  // an empty view takes the meter connected: the graph its main value, the
  // analog meter and the Poincaré plot and XY plots it when theirs is not connected -
  // while the workspace's meters open one by one only when none was chosen
  connect(m_session, &DeviceSession::opened, this, [this](MeterController *ctl)
  {
    auto free = [this](const ValueRef &v)
    { return !m_session->controller(v) && !m_openingPending.contains(v.source.section(':', 1)); };
    // a microcontroller's values all, as the Arduino IDE's plotter shows
    // them [E 18.11.]: a board sends them to be plotted
    QList<ValueRef> plotted { ValueRef(0, ctl->source()) };
    if (DeviceType::ofProtocol(ctl->sourceInfo().protocol) == DeviceType::Microcontroller)
      for (int id = 1; id < kMaxValues; ++id)   // they come with its lines; one never sent is not drawn
        plotted << ValueRef(id, ctl->source());
    for (GraphView *view : std::as_const(m_graphViews))
      if (view->model()->values().isEmpty())
        view->model()->setValues(plotted);
    for (MeterView *meter : m_wid->meters())
      if (free(meter->value()))
        meter->setValue(ValueRef(0, ctl->source()));
    for (PoincareView *plot : m_wid->poincares())
      if (free(plot->value()))
        plot->setValue(ValueRef(0, ctl->source()));
    for (XYView *view : m_xyViews)
    {
      ValueRef x = view->xValue(), y = view->yValue();
      if (free(x)) x = ValueRef(0, ctl->source());
      if (free(y)) y = ValueRef(0, ctl->source());
      if (x != view->xValue() || y != view->yValue())
        view->setValues(x, y);
    }
    for (HistogramView *histogram : m_histogramViews)
      if (free(histogram->value()))
        histogram->setValue(ValueRef(0, ctl->source()));
    for (HeatmapView *heatmap : m_heatmapViews)
    {
      ValueRef x = heatmap->value(0), y = heatmap->value(1), z = heatmap->value(2);
      bool changed = false;
      for (ValueRef *value : { &x, &y, &z })
        if (free(*value)) { *value = ValueRef(0, ctl->source()); changed = true; }
      if (changed)
        heatmap->setValues(x, y, z);
    }
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
  connect(panel, &FrontPanel::settingsRequested, this, [this](const QString &id) { deviceSettings(id); });
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

QMdiSubWindow *MainWindow::addGraphWindow(const QString &name, const QString &title, const ValueRef &value)
{
  auto *graph = new GraphWidget(this);
  graph->setWindowTitle(title);
  auto *view = new GraphView(m_session, graph, name, this);
  m_graphViews.insert(graph, view);
  view->model()->setWindowSeconds(m_wid->settingsDialog()->graphPage()->windowSeconds());
  const Designs::GraphColors g = Designs::graphColors(Designs::current());
  graph->setThemeColors(g.background, g.grid, g.labels, g.data);
  applyGraphLook(view);
  if (!value.source.isEmpty())
    view->model()->setValues({ value });
  // a file shown: Record and Live say so, the start page makes way
  connect(view, &GraphView::fileShownChanged, this, [this]
  {
    updateRecorderActions();
    updateEmptyStart();
  });
  // the start threshold dragged: the Recording page takes it
  connect(graph, &GraphWidget::startThresholdDragged, m_wid->settingsDialog(), &SettingsDialog::thresholdChangedSLOT);
  QMdiSubWindow *win = addView(graph, title, MdiArranger::Graph, name);
  graph->setContextMenuPolicy(Qt::CustomContextMenu);
  connect(graph, &QWidget::customContextMenuRequested, this, [this, graph, win](const QPoint &pos)
  {
    const QPoint globalPos = graph->mapToGlobal(pos);
    // on a mark's flag or a note's line: that mark
    if (const int mark = graph->markAtGlobal(globalPos); mark >= 0 && graph->marksEditable())
    {
      const qint64 t = graph->markTime(mark);
      const QString name = graph->markName(mark);
      QMenu menu(this);
      const QString photo = graph->markPhoto(mark);
      const QAction *show = photo.isEmpty() ? nullptr : menu.addAction(tr("Show &photo"));
      const QAction *edit = menu.addAction(QCoreApplication::translate("GraphWidget", "&Edit note..."));
      const QAction *remove = menu.addAction(QCoreApplication::translate("GraphWidget", "&Remove mark"));
      const QAction *chosen = menu.exec(globalPos);
      // an alarm may have come meanwhile: only the mark the menu was opened on
      if (!chosen || graph->markTime(mark) != t || graph->markName(mark) != name)
        return;
      if (chosen == show)
        showPhoto(photo);
      else if (chosen == edit)
        graph->editMark(mark);
      else if (chosen == remove)
        graph->removeMark(mark);
      return;
    }
    windowMenu(win, globalPos);
  });
  connect(graph, &GraphWidget::photoClicked, this, &MainWindow::showPhoto);
  // a note set in the graph marks its reading in the table
  connect(graph, &GraphWidget::noteAdded, this, [this](const QDateTime &when, const QString &title)
  {
    m_readings->log()->markAt(when, QColor::fromRgba(RecordingStore::kNoteColor), title);
  });
  connect(graph, &GraphWidget::noteMoved, this, [this](const QDateTime &from, const QDateTime &to, const QString &title)
  {
    m_readings->log()->moveMark(from, to, QColor::fromRgba(RecordingStore::kNoteColor), title);
  });
  return win;
}

QMdiSubWindow *MainWindow::addViewWindow(Added kind, const ValueRef &value, int number)
{
  const QString prefix = kind == Added::Meter ? QStringLiteral("meter-")
                         : kind == Added::Graph ? QStringLiteral("graph-")
                         : kind == Added::XY ? QStringLiteral("xy-")
                         : kind == Added::Histogram ? QStringLiteral("histogram-")
                         : kind == Added::Heatmap ? QStringLiteral("heatmap-") : QStringLiteral("poincare-");
  if (number < 2)
  {
    QStringList taken;
    for (const QMdiSubWindow *w : std::as_const(m_added))
      taken << w->objectName();
    for (number = 2; taken.contains(prefix + QString::number(number)); ++number) {}
  }
  QWidget *view;
  QMdiSubWindow *win;
  if (kind == Added::Graph)
  {
    win = addGraphWindow(prefix + QString::number(number), tr("Graph %1").arg(number), value);
    m_added << win;
    return win;
  }
  if (kind == Added::XY)
  {
    auto *plot = new XYPlot(this);
    plot->setWindowTitle(tr("XY diagram %1").arg(number));
    auto *view = new XYView(m_session, plot, this);
    m_xyViews.insert(plot, view);
    applyPlotColors(plot);
    if (!value.source.isEmpty())
      view->setValues(value, value);
    win = addView(plot, plot->windowTitle(), MdiArranger::Table, prefix + QString::number(number));
    view->plot()->setContextMenuPolicy(Qt::CustomContextMenu);
    connect(plot, &QWidget::customContextMenuRequested, this, [this, plot, win](const QPoint &pos)
    {
      windowMenu(win, plot->mapToGlobal(pos));
    });
    m_added << win;
    return win;
  }
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
  else if (kind == Added::Histogram)
  {
    auto *plot = new HistogramPlot(this);
    plot->setWindowTitle(tr("Histogram %1").arg(number));
    auto *source = new HistogramView(m_session, plot, this);
    m_histogramViews.insert(plot, source);
    source->setValue(value);
    win = addView(plot, plot->windowTitle(), MdiArranger::Table, prefix + QString::number(number));
    applyPlotColors(plot);
    view = plot;
  }
  else if (kind == Added::Heatmap)
  {
    auto *plot = new HeatmapPlot(this);
    plot->setWindowTitle(tr("Heatmap %1").arg(number));
    auto *source = new HeatmapView(m_session, plot, this);
    m_heatmapViews.insert(plot, source);
    source->setValues(value, value, value);
    win = addView(plot, plot->windowTitle(), MdiArranger::Table, prefix + QString::number(number));
    applyPlotColors(plot);
    view = plot;
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

QMdiSubWindow *MainWindow::newView(Added kind, const ValueRef &value, bool ask)
{
  QMdiSubWindow *win = addViewWindow(kind, value);
  // from the menu its value is asked first (the active device's first one
  // ticked); cancelled, there is no new view. With no device open there is
  // nothing to pick from.
  if (ask && kind == Added::Graph)
  {
    // one device with one value has nothing to choose from
    const QList<MeterController *> meters = m_session->controllers();
    if ((meters.size() > 1 || (meters.size() == 1 && meters.first()->valueCount() > 1)) && !editGraphSources(graphViewOf(win)))
    {
      removeAddedWindow(win);
      return nullptr;
    }
  }
  else if (ask && !m_session->controllers().isEmpty())
  {
    if (kind == Added::XY)
    {
      if (!editXYSources(xyViewOf(win)))
      {
        removeAddedWindow(win);
        return nullptr;
      }
    }
    else
    {
      MeterView *meter = meterViewOf(win);
      PoincareView *plot = poincareViewOf(win);
      HistogramView *histogram = histogramViewOf(win);
      HeatmapView *heatmap = heatmapViewOf(win);
      if (!editSources(meter ? ValueMenu::View::Meter : heatmap ? ValueMenu::View::Heatmap
                             : histogram ? ValueMenu::View::Histogram : ValueMenu::View::Poincare,
                       meter, plot, nullptr, histogram, heatmap))
      {
        removeAddedWindow(win);
        return nullptr;
      }
    }
  }
  win->show();
  m_mdi->setActiveSubWindow(win);
  growFor(win);
  m_arranger->arrange();
  return win;
}

void MainWindow::removeAddedWindow(QMdiSubWindow *win)
{
  if (!m_added.removeOne(win))
    return;
  if (MeterView *meter = meterViewOf(win))
    m_wid->removeMeter(meter);
  if (PoincareView *plot = poincareViewOf(win))
    m_wid->removePoincare(plot);
  if (HistogramView *histogram = histogramViewOf(win))
  {
    m_histogramViews.remove(histogram->plot());
    delete histogram;
  }
  if (HeatmapView *heatmap = heatmapViewOf(win))
  {
    m_heatmapViews.remove(heatmap->plot());
    delete heatmap;
  }
  if (XYView *view = xyViewOf(win))
  {
    m_xyViews.remove(view->plot());
    delete view;
  }
  if (GraphView *graph = graphViewOf(win))
  {
    m_graphViews.remove(graph->graph());
    delete graph;
  }
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

HistogramView *MainWindow::histogramViewOf(const QMdiSubWindow *win) const
{
  for (HistogramView *view : m_histogramViews)
    if (win && view->plot() == win->widget())
      return view;
  return nullptr;
}

HeatmapView *MainWindow::heatmapViewOf(const QMdiSubWindow *win) const
{
  for (HeatmapView *view : m_heatmapViews)
    if (win && view->plot() == win->widget())
      return view;
  return nullptr;
}

QList<QMdiSubWindow *> MainWindow::heatmapWindows() const
{
  QList<QMdiSubWindow *> wins { m_heatmapWin };
  for (QMdiSubWindow *win : m_added)
    if (heatmapViewOf(win))
      wins << win;
  return wins;
}

XYView *MainWindow::xyViewOf(const QMdiSubWindow *win) const
{
  for (XYView *view : m_xyViews)
    if (win && view->plot() == win->widget())
      return view;
  return nullptr;
}

// A graph draws the marks of its meters' own recordings
RecordingStore *MainWindow::graphStoreFor(const MeterController *ctl) const
{
  for (const GraphView *view : m_graphViews)
  {
    const QList<ValueRef> shown = view->model()->values();
    if (std::any_of(shown.cbegin(), shown.cend(), [ctl](const ValueRef &v) { return v.source == ctl->source(); }))
      return ctl->recorder();
  }
  return nullptr;
}

void MainWindow::showGraphWindow(QMdiSubWindow *win)
{
  if (win == m_graphWin)
    action_Graph->setChecked(true);
  else if (win->isHidden())
  {
    win->show();
    growFor(win);
    m_arranger->arrange();
  }
}

QAction *MainWindow::hideAction(QMdiSubWindow *win)
{
  // a view added goes when closed (eventFilter()), the first of a kind hides
  QAction *hide = new QAction(m_added.contains(win) ? tr("&Close window") : tr("&Hide view"), win);
  connect(hide, &QAction::triggered, win, &QWidget::close);
  return hide;
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

QList<QMdiSubWindow *> MainWindow::histogramWindows() const
{
  QList<QMdiSubWindow *> wins { m_histogramWin };
  for (QMdiSubWindow *w : m_added)
    if (histogramViewOf(w))
      wins << w;
  return wins;
}

GraphView *MainWindow::graphViewOf(const QMdiSubWindow *win) const
{
  for (GraphView *view : m_graphViews)
    if (win && view->graph() == win->widget())
      return view;
  return nullptr;
}

QList<QMdiSubWindow *> MainWindow::graphWindows() const
{
  QList<QMdiSubWindow *> wins { m_graphWin };
  for (QMdiSubWindow *win : m_added)
    if (graphViewOf(win))
      wins << win;
  return wins;
}

void MainWindow::loadIntoGraph(GraphView *view)
{
  if (const QString path = askRecordingPath(); !path.isEmpty())
    loadRecordingInto(view, path);
}

QString MainWindow::askRecordingPath()
{
  return QFileDialog::getOpenFileName(this, tr("Load recording"), m_wid->settings()->getString("QtDMM/LastUsesPath"),
                                      tr("Recordings (*.%1 *.csv *.txt);;All files (*)").arg(RecordingFile::kSuffix));
}

bool MainWindow::loadRecordingInto(GraphView *view, const QString &path)
{
  m_wid->settings()->setString("QtDMM/LastUsesPath", QDir().absoluteFilePath(path));
  QString error;
  if (view->loadFile(path, &error))
    return true;
  QMessageBox::warning(this, tr("Load recording"), error);
  return false;
}

void MainWindow::importRecording()
{
  if (const QString path = askRecordingPath(); !path.isEmpty())
    importRecording(path);
}

bool MainWindow::importRecording(const QString &path)
{
  // the graph used last, else the first one; it is the one used now, so
  // Record and Live follow its file
  GraphView *view = pictureGraph() ? pictureGraph() : graphViewOf(m_graphWin);
  if (!loadRecordingInto(view, path))
    return false;
  for (QMdiSubWindow *win : graphWindows())
    if (graphViewOf(win) == view)
      showGraphWindow(win);
  setPictureGraph(view);
  return true;
}

void MainWindow::setPictureGraph(GraphView *view)
{
  if (m_pictureGraph == view)
    return;
  m_pictureGraph = view;
  updateRecorderActions();
}

void MainWindow::newGraph(const ValueRef &value)
{
  const MeterController *ctl = m_session->controllers().value(0);
  // from the button its values are asked; next to another graph it shows that one's first value
  newView(Added::Graph, !value.source.isEmpty() ? value : ctl ? ValueRef(0, ctl->source()) : ValueRef(),
          value.source.isEmpty());
}

// The Graph page of the settings on a graph: variant, colours, line,
// points and width.
void MainWindow::applyGraphLook(GraphView *view)
{
  const GraphPrefs *prefs = m_wid->settingsDialog()->graphPage();
  GraphLook::Settings settings;
  settings.background = prefs->bgColor();
  settings.grid = prefs->gridColor();
  settings.data = prefs->dataColor();
  settings.lineMode = prefs->lineMode();
  settings.pointMode = prefs->pointMode();
  settings.lineWidth = prefs->lineWidth();
  settings.cursor = prefs->cursorColor();
  settings.trigger = prefs->startColor();
  settings.crosshair = prefs->crosshair();
  settings.statsLeft = prefs->statsLeft();
  view->graph()->setLookSettings(settings);
  view->setDefaultVariant(prefs->variant());
}

bool MainWindow::editGraphSources(GraphView *view)
{
  // the open meters with their values
  QList<SourcesDlg::Meter> meters;
  for (const MeterController *ctl : m_session->controllers())
  {
    SourcesDlg::Meter meter;
    meter.source = ctl->source();
    meter.name = ctl->deviceName();
    for (int id = 0; id < ctl->valueCount(); ++id)
      meter.values << otherValueRow(ctl, id).name;
    meters << meter;
  }
  SourcesDlg dlg(view->graph()->windowTitle(), true, meters, this);
  dlg.setChosen(view->model()->values());
  if (dlg.exec() != QDialog::Accepted)
    return false;
  view->model()->setValues(dlg.chosen());
  return true;
}

// "Axis" in the menu of a graph: for each value Automatic or one of the
// axes the graph has. The entries are words of the context "GraphWidget"
// (graphwords.h)
void MainWindow::fillGraphReferenceMenu(QMenu *menu, GraphView *view, const QPoint &globalPos)
{
  using namespace GraphReference;
  auto word = [](const char *text) { return QCoreApplication::translate("GraphWidget", text); };
  GraphModel *model = view->model();
  const GraphWidget *graph = view->graph();
  const Line now = model->reference();
  auto entry = [&](const QString &text, const Line &line)
  {
    QAction *a = menu->addAction(text);
    a->setCheckable(true);
    a->setChecked(now == line);
    connect(a, &QAction::triggered, model, [model, line] { model->setReference(line); });
    return a;
  };
  entry(word("At &0"), line(Zero));
  entry(word("&Mean of the window"), line(Mean));
  entry(word("M&inimum of the window"), line(Min));
  entry(word("Ma&ximum of the window"), line(Max));
  if (!graph->alarmThresholds().isEmpty())
    menu->addSeparator();
  for (double a : graph->alarmThresholds())
    entry(word("Alarm threshold %1").arg(graph->levelText(a)), line(GraphReference::Alarm, a));
  if (now.kind == Value)
  {
    menu->addSeparator();
    entry(word("At %1 (dragged)").arg(graph->levelText(now.value)), now)->setEnabled(false);
  }
  menu->addSeparator();
  entry(word("&Off"), line(Off));
  // further lines at fixed values: where the menu was opened, else in the middle
  menu->addSeparator();
  const QList<double> lines = model->referenceLines();
  const double at = graph->roundedValueAt(globalPos);
  QAction *add = menu->addAction(word("&Add a line at %1").arg(graph->levelText(at)));
  add->setEnabled(lines.size() < kMaxLines && graph->leftValue().id >= 0);
  connect(add, &QAction::triggered, model, [model, lines, at] { model->setReferenceLines(lines + QList<double>{ at }); });
  for (int i = 0; i < lines.size(); ++i)
    connect(menu->addAction(word("Remove the line at %1").arg(graph->levelText(lines.at(i)))), &QAction::triggered, model, [model, lines, i]
    {
      QList<double> rest = lines;
      rest.removeAt(i);
      model->setReferenceLines(rest);
    });
  menu->addSeparator();
  QAction *crossings = menu->addAction(tr("&Crossings"));
  crossings->setCheckable(true);
  crossings->setChecked(model->crossingsVisible());
  connect(crossings, &QAction::toggled, model, &GraphModel::setCrossingsVisible);
}

void MainWindow::fillGraphAxisMenu(QMenu *menu, GraphView *view)
{
  const QList<ValueRef> values = view->model()->values();
  const QString places[] = { tr("&Left axis"), tr("&Right axis"), tr("&Third axis") };
  for (const ValueRef &value : values)
  {
    const MeterController *ctl = m_session->controller(value);
    if (!ctl)
      continue;
    QString name = otherValueRow(ctl, value.id).name;
    if (m_session->controllers().size() > 1)
      name = ctl->deviceName() + " · " + name;
    QMenu *sub = values.size() > 1 ? menu->addMenu(name.replace('&', "&&")) : menu;
    auto *group = new QActionGroup(sub);
    auto entry = [&](const QString &text, int axis)
    {
      QAction *a = sub->addAction(text);
      a->setCheckable(true);
      a->setChecked(view->model()->valueAxis(value) == axis);
      group->addAction(a);
      connect(a, &QAction::triggered, view, [view, value, axis] { view->model()->setValueAxis(value, axis); });
    };
    entry(tr("&Automatic"), GraphModel::kAxisAuto);
    for (int axis = 0; axis < GraphWidget::kMaxAxes; ++axis)
      entry(axis < 3 ? places[axis] : tr("Axis %1").arg(axis + 1), axis);
  }
  // the range of each axis drawn: fitted to the values, or fixed by unit
  // (words of the context GraphWidget)
  auto word = [](const char *text) { return QCoreApplication::translate("GraphWidget", text); };
  const QStringList units = view->graph()->axisUnits();
  if (!units.isEmpty())
    menu->addSeparator();
  for (const QString &unit : units)
  {
    QMenu *sub = menu->addMenu(tr("Range [%1]").arg(GraphWidget::unitText(unit)).replace('&', "&&"));
    const QMap<QString, GraphModel::Range> ranges = view->model()->fixedRanges();
    const bool fixed = ranges.contains(unit);
    auto *group = new QActionGroup(sub);
    QAction *automatic = sub->addAction(word("&Automatic"));
    QAction *range = sub->addAction(fixed ? word("&Fixed: %1 .. %2 %3...")
                                              .arg(QLocale().toString(ranges.value(unit).first),
                                                   QLocale().toString(ranges.value(unit).second),
                                                   unit == GraphWidget::kNoUnit ? QString() : SiPrefix::displayText(unit))
                                          : word("&Fixed range..."));
    for (QAction *a : { automatic, range })
    {
      a->setCheckable(true);
      group->addAction(a);
    }
    (fixed ? range : automatic)->setChecked(true);
    connect(automatic, &QAction::triggered, view, [view, unit] { view->model()->clearFixedRange(unit); });
    connect(range, &QAction::triggered, view, [view, unit, fixed, ranges]
    {
      double min = ranges.value(unit).first, max = ranges.value(unit).second;
      if (view->graph()->askFixedRange(unit, min, max, fixed))
        view->model()->setFixedRange(unit, min, max);
    });
  }
}

QList<QMdiSubWindow *> MainWindow::xyWindows() const
{
  QList<QMdiSubWindow *> wins { m_xyWin };
  for (QMdiSubWindow *win : m_added)
    if (xyViewOf(win))
      wins << win;
  return wins;
}

bool MainWindow::isColumn(const QMdiSubWindow *win) const
{
  return win == m_readingsWin || poincareViewOf(win) || xyViewOf(win) || histogramViewOf(win) || heatmapViewOf(win);
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
    const DmmDecoder::DMMInfo &info = ctl->sourceInfo();
    // the controller's count: a node's values are not a model's; a sum or a
    // formula has no range for a bar
    lcd->setDisplayMode(info.display, gui->showMinMax(), gui->showBar() && !ctl->isNode(), ctl->valueCount());
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
  connect(m_sidebar, &DeviceSidebar::nodeResetRequested, this, [this](const QString &id)
  {
    if (NodeRunner *runner = NodeRunner::of(m_session->controller("device:" + id)))
      runner->reset();
  });
  connect(m_sidebar, &DeviceSidebar::captureRequested, this, [this, openController](const QString &id)
  {
    // from My devices: connected first, beside the others
    if (!openController(id))
      openBeside(id);
    MeterController *ctl = openController(id);
    if (!ctl || !ctl->dmm())   // a logic node has no frames
      return;
    QPointer<CaptureDlg> &dlg = m_captureDlgs[id];
    if (!dlg)
    {
      const QString name = ctl->deviceName().isEmpty() ? ctl->sourceInfo().name : ctl->deviceName();
      dlg = new CaptureDlg(ctl->dmm(), name, m_wid->settings(), this);
    }
    dlg->show();
    dlg->raise();
    dlg->activateWindow();
  });
  // an alarm of a meter marks the graph while it shows a value of it
  connect(m_session, &DeviceSession::opened, this, [this](MeterController *ctl)
  {
    connect(ctl, &MeterController::markRequested, this, [this, ctl](const QColor &color, const QString &name, bool graph, bool)
    {
      if (!graph)
        return;
      if (RecordingStore *own = graphStoreFor(ctl))
        own->addMark(color.rgba(), tr("%1: %2").arg(ctl->deviceName(), name));
    });
    // a SCPI client's note in the graph (the table: markRequested())
    connect(ctl, &MeterController::noteRequested, this, [this, ctl](const QString &title, const QString &note)
    {
      if (RecordingStore *own = graphStoreFor(ctl))
        own->addNote(-1, tr("%1: %2").arg(ctl->deviceName(), title), note);
    });
  });
  connect(m_session, &DeviceSession::opened, this, &MainWindow::updateOpenDevices);
  connect(m_session, &DeviceSession::closed, this, &MainWindow::updateOpenDevices);
  connect(m_sidebar, &DeviceSidebar::addDeviceRequested, this, [this] { addDevice(); });
  connect(m_sidebar, &DeviceSidebar::serviceSettingsRequested, m_wid, &InstanceWidget::configScpiSLOT);
  connect(m_sidebar, &DeviceSidebar::workspaceRequested, this, &MainWindow::loadWorkspaceFile);
  connect(m_sidebar, &DeviceSidebar::newBenchRequested, this, [this] { saveWorkspace(true); });
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
  connect(m_addDeviceAction, &QAction::triggered, this, [this] { addDevice(); });
  toolBarDMM->insertAction(m_sidebarAction, m_addDeviceAction);

  // the project, second: the arrow makes one, opens one, takes one of the
  // last ones or closes the open one
  m_projectAction = new QAction(QIcon::fromTheme("document-open", style()->standardIcon(QStyle::SP_DirOpenIcon)),
                                tr("Project"), this);
  m_projectAction->setObjectName("action_ProjectBasket");
  m_projectAction->setToolTip(projectTip());
  // [E 09.11.] with a project its window, without one a new project (whose
  // window then shows); pressed while the window shows
  m_projectAction->setCheckable(true);
  m_benchActions << m_projectAction << m_basketAction << m_basket->newAction() << m_basket->openAction()
                 << m_basket->importAction();
  connect(m_basketAction, &QAction::toggled, this, &MainWindow::updateEmptyStart);
  connect(m_projectAction, &QAction::triggered, this, [this]
  {
    if (m_basket->folder().isEmpty())
      m_basket->newAction()->trigger();   // a new one shows its window (projectOpened)
    else
      m_basketAction->toggle();
    m_projectAction->setChecked(m_basketAction->isChecked());
  });
  connect(m_basketAction, &QAction::toggled, m_projectAction, &QAction::setChecked);
  m_projectAction->setChecked(m_basketAction->isChecked());
  toolBarDMM->insertAction(m_sidebarAction, m_projectAction);
  addViewMenu(m_projectAction, [this](QMenu *menu)
  {
    menu->addAction(m_basket->newAction());
    menu->addAction(m_basket->openAction());
    menu->addAction(m_benchIntoProject);
    const QStringList dirs = recentProjects().list().mid(0, 10);
    if (!dirs.isEmpty())
      menu->addSeparator();
    for (int i = 0; i < dirs.size(); ++i)
    {
      const QString &dir = dirs[i];
      QString name = BasketWidget::projectName(dir).replace('&', "&&");
      // 1 ... 9 as the letters, then none
      QAction *a = menu->addAction(i < 9 ? QString("&%1 %2").arg(i + 1).arg(name) : name);
      a->setToolTip(QDir::toNativeSeparators(dir));
      a->setCheckable(true);
      a->setChecked(dir == m_basket->folder());
      a->setEnabled(QFileInfo(dir).isDir());
      connect(a, &QAction::triggered, this, [this, dir]
      {
        m_basket->setFolder(dir);
        m_basketAction->setChecked(true);
      });
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
  toolBarDMM->addAction(m_xyAction);
  toolBarDMM->addAction(m_histogramAction);
  toolBarDMM->addAction(m_heatmapAction);
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
  addViewMenu(m_xyAction, [this](QMenu *menu)
  {
    for (QMdiSubWindow *win : xyWindows())
      addViewEntry(menu, win, win->windowTitle());
    menu->addSeparator();
    connect(menu->addAction(tr("&New...")), &QAction::triggered, m_newXYAction, &QAction::trigger);
  });
  addViewMenu(m_histogramAction, [this](QMenu *menu)
  {
    for (QMdiSubWindow *win : histogramWindows())
      addViewEntry(menu, win, viewName(win, histogramViewOf(win)->value()));
    menu->addSeparator();
    connect(menu->addAction(tr("&New...")), &QAction::triggered, m_newHistogramAction, &QAction::trigger);
  });
  addViewMenu(m_heatmapAction, [this](QMenu *menu)
  {
    for (QMdiSubWindow *win : heatmapWindows())
      addViewEntry(menu, win, win->windowTitle());
    if (!m_session->controllers().isEmpty())
    {
      menu->addSeparator();
      connect(menu->addAction(tr("S&ources...")), &QAction::triggered, this,
              [this] { editSources(ValueMenu::View::Heatmap, nullptr, nullptr, nullptr, nullptr, heatmapViewOf(m_heatmapWin)); });
    }
    menu->addSeparator();
    connect(menu->addAction(tr("&New...")), &QAction::triggered, m_newHeatmapAction, &QAction::trigger);
  });
  addViewMenu(action_Graph, [this](QMenu *menu)
  {
    for (QMdiSubWindow *win : graphWindows())
      addViewEntry(menu, win, win->windowTitle());
    if (m_graphSources->isVisible())
      menu->addAction(m_graphSources);
    menu->addSeparator();
    connect(menu->addAction(tr("&New...")), &QAction::triggered, m_newGraphAction, &QAction::trigger);
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
  kindClicked(action_Graph, [this] { return graphWindows(); }, [] { return true; });
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
  kindClicked(m_xyAction, [this] { return xyWindows(); }, [this]
  {
    const XYView *view = m_xyViews.value(m_xyPlot);
    const bool none = view->xValue().source.isEmpty() || view->yValue().source.isEmpty();
    return !none || m_session->controllers().isEmpty() || editXYSources(m_xyViews.value(m_xyPlot));
  });
  kindClicked(m_histogramAction, [this] { return histogramWindows(); }, [this]
  {
    const bool none = histogramViewOf(m_histogramWin)->value().source.isEmpty();
    return !none || m_session->controllers().isEmpty()
           || editSources(ValueMenu::View::Histogram, nullptr, nullptr, nullptr, histogramViewOf(m_histogramWin));
  });
  kindClicked(m_heatmapAction, [this] { return heatmapWindows(); }, [this]
  {
    const HeatmapView *view = heatmapViewOf(m_heatmapWin);
    const bool none = view->value(0).source.isEmpty() || view->value(1).source.isEmpty() || view->value(2).source.isEmpty();
    return !none || m_session->controllers().isEmpty()
           || editSources(ValueMenu::View::Heatmap, nullptr, nullptr, nullptr, nullptr, heatmapViewOf(m_heatmapWin));
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
    for (QAction *a : { m_meterAction, m_poincareAction, m_xyAction, m_histogramAction, m_heatmapAction, action_Graph })
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
  connect(saveWs, &QAction::triggered, this, [this] { saveWorkspace(); });
  m_benchIntoProject = m_arrangeMenu->addAction(tr("Put the bench into a &new project..."));
  m_benchIntoProject->setObjectName("action_BenchIntoProject");
  m_benchIntoProject->setWhatsThis(tr("<html><head/><body><p><span style=\" font-weight:600;\">Put the bench into a "
                                      "new project</span></p><p>Makes a new project, opens it and saves the bench "
                                      "into its folder: the way from a single bench to a project of several."
                                      "</p></body></html>"));
  connect(m_benchIntoProject, &QAction::triggered, this, &MainWindow::benchIntoNewProject);
  // the same at the sidebar's Recent benches
  m_sidebar->setWorkspaceActions({ loadWs, saveWs, m_benchIntoProject });
  m_benchActions << loadWs << saveWs << m_benchIntoProject << m_recentMenu->menuAction();
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
    // without a device Live says nothing
    if (m_session->controllers().isEmpty() && m_wid->recordingState() == RecordingStore::Live)
      return;
    m_info->setText(text.section('\n', 0, 0));
    m_info->setToolTip(text.section('\n', 1));
  });
  connect(m_wid, &InstanceWidget::useTextLabel, this, &MainWindow::setUseTextLabel);
  connect(m_wid, &InstanceWidget::iconSet, this, [](const QString &set)
          { Designs::setIconSet(Designs::iconSetFromName(set)); });
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
  for (QMdiSubWindow *w : { m_meterWin, m_graphWin, m_readingsWin, m_poincareWin, m_histogramWin, m_heatmapWin })
    if (!w->isHidden())
      m_grown.insert(w);
  if (m_grown.contains(m_readingsWin) || m_grown.contains(m_poincareWin) || m_grown.contains(m_histogramWin) || m_grown.contains(m_heatmapWin))
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
    for (QMdiSubWindow *w : { m_graphWin, m_readingsWin, m_poincareWin, m_histogramWin, m_heatmapWin })
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
  const QList<QMdiSubWindow *> graphWins = graphWindows();
  const bool showsFile = std::any_of(graphWins.cbegin(), graphWins.cend(), [this](QMdiSubWindow *w) { return graphViewOf(w)->showsFile(); });
  const bool empty = m_session->controllers().isEmpty() && m_pendingOpen.isEmpty() && !showsFile;
  const bool noDevices = m_wid->devices()->list().isEmpty();
  m_emptyConnect->setVisible(!noDevices);
  // nothing to see behind the start page; checked stays what comes back
  for (QAction *a : { m_displayAction, m_meterAction, m_poincareAction, m_xyAction, m_histogramAction, m_heatmapAction, action_Graph, m_readingsAction })
    a->setEnabled(!empty);
  // [E 09.11.] a project or a bench makes no sense without a device
  if (m_benchActions.constFirst()->isEnabled() == noDevices)
  {
    for (QAction *a : m_benchActions)
      a->setEnabled(!noDevices);
    updateProjectSidebar();
    updateRecentWorkspaces();
  }
  m_projectAction->setToolTip(noDevices ? tr("Project and bench: add a device first") : projectTip());
  // the project's window stays at hand beside the start page
  m_basketBeside = empty && !noDevices && !m_basketWin->isHidden();
  if (empty)
  {
    // the last device closed: its messages go with it
    if (m_emptyStart->isHidden())
      for (QLabel *l : { m_error, m_info })
      {
        l->clear();
        l->setToolTip(QString());
      }
    m_emptyStart->raise();
  }
  m_emptyStart->setVisible(empty);
  placeEmptyStart();
}

QString MainWindow::projectTip() const
{
  return tr("Show or hide the project's window; without a project: a new one. The arrow: new, open, the last ones");
}

void MainWindow::placeEmptyStart()
{
  const bool beside = m_basketBeside && !m_emptyStart->isHidden();
  // the others stay where they are, under the start page
  m_arranger->setHeld(beside);
  QRect r = m_mdi->rect();
  if (beside)
  {
    // the project's window over a column at the right, like the readings table
    const QWidget *vp = m_mdi->viewport();
    const int column = qMin(vp->width() / 2, qMax(320, m_basketWin->minimumSizeHint().width()));
    m_basketWin->setGeometry(vp->width() - column, 0, column, vp->height());
    m_basketWin->raise();
    r.setWidth(vp->x() + vp->width() - column);
  }
  m_emptyStart->setGeometry(r);
}

void MainWindow::addDevice(int start, const ValueRef &input)
{
  AddDeviceDlg dlg(m_wid->devices(), this);
  dlg.setPorts(Transport::availablePorts() + m_wid->settings()->customPorts());
  dlg.setSigrokExe(m_wid->settings()->getString("Port settings/sigrok_exe", "sigrok-cli"));
  dlg.setPlacesInUse(placesInUse());
  // a calculated value's settings, filled in for a new one
  dlg.setNodeEditors([this, input](AddDeviceDlg::Connection kind)
  {
    if (kind == AddDeviceDlg::Simulated)
      return makeNodeEditor(GeneratorNode::kType, QString(), nullptr, tr("Simulated"));
    if (kind == AddDeviceDlg::Integral)
      return makeNodeEditor(IntegralNode::kType, QString(), nullptr, QString(), input);
    return makeNodeEditor(FormulaNode::kType, QString(), nullptr, tr("Calculated"));
  });
  // straight to a calculated value's settings ("Integrate..." of a value)
  if (start >= 0)
    dlg.chooseConnection(static_cast<AddDeviceDlg::Connection>(start));
  if (dlg.exec() != QDialog::Accepted)
    return;
  const bool node = AddDeviceDlg::isNode(dlg.connection());
  QVariantMap keys = dlg.keys();
  if (!node)
    keys.insert("Port settings/device", SerialDevice::stableDevice(keys.value("Port settings/device").toString()));
  const int before = int(m_wid->devices()->list().size());
  // a find that is one of My devices changes that entry: no second one
  QString id = dlg.knownDevice();
  if (id.isEmpty())
    id = m_wid->devices()->add(node ? m_wid->devices()->uniqueName(dlg.name()) : dlg.name(), keys);
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

QList<FormulaNodeDlg::Choice> MainWindow::formulaChoices(const QString &id) const
{
  QList<FormulaNodeDlg::Choice> out;
  for (const MyDevice &device : m_wid->devices()->list())
  {
    if (device.id == id)
      continue;
    FormulaNodeDlg::Choice c;
    c.source = "device:" + device.id;
    c.name = device.name;
    const MeterController *ctl = m_session->controller(c.source);
    c.connected = ctl != nullptr;
    // not connected: its values by number, units unknown; a calculated
    // one: its outputs as set. DMM/number-of-values is the frames per
    // reading, not the values: the model's count (MeterSetup)
    std::unique_ptr<LogicNode> node = nodeOf(device.keys);
    // a node by its kind, as on the tiles of Add device
    if (node)
      c.icon = node->type() == QLatin1String(IntegralNode::kType)    ? QStringLiteral("qtdmm-integration")
               : node->type() == QLatin1String(GeneratorNode::kType) ? QStringLiteral("qtdmm-simulated")
                                                                     : QStringLiteral("qtdmm-calculated");
    const QList<LogicNode::Output> outputs = node ? node->outputs() : QList<LogicNode::Output>();
    const int count = ctl ? ctl->valueCount() : node ? int(outputs.size())
                                                     : qBound(1, MeterSetup::fromKeys(device.keys).valueCount, kMaxValues);
    for (int i = 0; i < count && i < kMaxValues; ++i)
    {
      QString value = ctl ? ctl->valueName(i) : QString();
      if (value.isEmpty() && node)
        value = outputs[i].name;
      c.values << (value.isEmpty() ? tr("value %1").arg(i + 1) : value);
      c.units << (ctl ? ctl->lastReading(i).baseUnit : node ? SiPrefix::split(outputs[i].unit).baseUnit : QString());
    }
    out << c;
  }
  return out;
}

std::unique_ptr<LogicNode> MainWindow::nodeOf(const QVariantMap &keys, QString *why) const
{
  if (keys.contains("Node/json"))
    return LogicNode::fromJsonText(keys.value("Node/json").toString());
  if (LegacyNodes::isLegacy(keys))
    return LegacyNodes::fromKeys(keys, [this](const QString &v) { return m_session->sourceOfVariable(v); }, why);
  return nullptr;
}

NodeEditor *MainWindow::makeNodeEditor(const QString &type, const QString &id, const LogicNode *stored,
                                       const QString &name, const ValueRef &input)
{
  if (type == QLatin1String(IntegralNode::kType))
  {
    auto *editor = new IntegralNodeEditor;
    editor->setSources(formulaChoices(id));
    // one open: where it is now, and setting it to 0
    if (!id.isEmpty())
      if (MeterController *ctl = m_session->controller("device:" + id); NodeRunner::of(ctl))
        editor->setState([this, id]() -> std::optional<Reading>
                         {
                           const MeterController *c = m_session->controller("device:" + id);
                           return c && c->lastReading().msecs > 0 ? std::optional(c->lastReading()) : std::nullopt;
                         },
                         [this, id]
                         {
                           if (NodeRunner *runner = NodeRunner::of(m_session->controller("device:" + id)))
                             runner->reset();
                         });
    const auto *integral = dynamic_cast<const IntegralNode *>(stored);
    IntegralNode fresh;
    if (!input.source.isEmpty())
      fresh.setInputs({ input });
    editor->load(name, integral ? *integral : fresh);
    if (!id.isEmpty())
      editor->setLoopCheck(loopCheck(id));
    return editor;
  }

  if (type == QLatin1String(GeneratorNode::kType))
  {
    auto *editor = new GeneratorNodeEditor;
    const auto *generator = dynamic_cast<const GeneratorNode *>(stored);
    editor->load(name, generator ? *generator : GeneratorNode());
    return editor;
  }

  auto *editor = new FormulaNodeEditor;
  editor->setChoices(formulaChoices(id));
  editor->setCurrent([this](const ValueRef &ref) -> std::optional<Reading>
  {
    const MeterController *ctl = m_session->controller(ref.source);
    if (!ctl)
      return std::nullopt;
    const Reading r = ctl->lastReading(ref.id);
    return r.msecs > 0 ? std::optional(r) : std::nullopt;
  });
  const auto *formula = dynamic_cast<const FormulaNode *>(stored);
  editor->load(name, formula ? *formula : FormulaNode());
  // the inputs of the calculated values of My devices, for loops
  if (!id.isEmpty())
    editor->setLoopCheck(loopCheck(id));
  return editor;
}

std::function<bool(const QList<ValueRef> &)> MainWindow::loopCheck(const QString &id) const
{
  // the inputs of the calculated values of My devices
  const QString source = "device:" + id;
  return [this, source](const QList<ValueRef> &inputs)
  {
    return LogicNode::makesLoop(source, inputs, [this](const QString &s) -> QList<ValueRef>
    {
      const std::optional<MyDevice> d = m_wid->devices()->find(s.mid(int(qstrlen("device:"))));
      const std::unique_ptr<LogicNode> n = d ? nodeOf(d->keys) : nullptr;
      return n ? n->inputs() : QList<ValueRef>();
    });
  };
}

std::optional<QVariantMap> MainWindow::editNode(const QString &id, const QVariantMap &keys, QString *name)
{
  QString why;
  std::unique_ptr<LogicNode> stored = nodeOf(keys, &why);
  // a calculated value of before naming a device no longer there: set up anew
  if (!stored && LegacyNodes::isLegacy(keys))
    QMessageBox::warning(this, *name, tr("%1\nSet up its formula anew.").arg(why));
  const QString type = stored ? stored->type() : QString::fromLatin1(FormulaNode::kType);
  const QString title = type == QLatin1String(IntegralNode::kType) ? tr("Integral")
                        : type == QLatin1String(GeneratorNode::kType) ? tr("Simulated values")
                                                                       : tr("Calculated value");
  NodeDlg dlg(makeNodeEditor(type, id, stored.get(), *name), title, this);
  dlg.resize(type == QLatin1String(GeneratorNode::kType) ? QSize(760, 360) : QSize(640, 480));
  if (dlg.exec() != QDialog::Accepted)
    return std::nullopt;
  *name = dlg.editor()->name();
  return QVariantMap { { "DMM/model", dlg.editor()->model() }, { "Node/json", dlg.editor()->logicNode()->toJsonText() } };
}

void MainWindow::addNode(const QString &type, const ValueRef &input)
{
  addDevice(type == QLatin1String(GeneratorNode::kType) ? AddDeviceDlg::Simulated
            : type == QLatin1String(IntegralNode::kType) ? AddDeviceDlg::Integral
                                                          : AddDeviceDlg::Calculated,
            input);
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
  const QString project = m_basket ? m_basket->folder() : QString();
  if (!project.isEmpty())
    where << BasketWidget::projectName(project);
  // a bench without a name of its own by its file, as the sidebar has it;
  // with a project only one of it (in its folder), until saved there [V 09.11.]
  const bool benchInProject = project.isEmpty()
                              || (!m_workspaceFile.isEmpty()
                                  && BasketWidget::projectOf(m_workspaceFile, project) == project);
  if ((!m_workspaceName.isEmpty() || !m_workspaceFile.isEmpty()) && benchInProject)
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
  connect(action_Stop, &QAction::triggered, m_wid, &InstanceWidget::stopSLOT);
  connect(action_Clear, &QAction::triggered, m_wid, &InstanceWidget::clearSLOT);
  connect(action_Print, &QAction::triggered, this, [this]
  {
    // the graph used last, else the first one
    if (QPointer<GraphView> graphView = pictureGraph() ? pictureGraph() : graphViewOf(m_graphWin))
      m_wid->printWith([graphView](QPrinter *printer, const QString &title, const QString &comment)
      {
        if (graphView)
          graphView->graph()->print(printer, title, comment);
      });
  });
  connect(action_Import, &QAction::triggered, this, qOverload<>(&MainWindow::importRecording));
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
  m_updateAction = new QAction(tr("Check for &updates..."), this);
  m_updateAction->setObjectName("action_CheckUpdates");
  m_updateAction->setVisible(UpdateCheck::enabled());
  m_updateAction->setWhatsThis(tr("<html><head/><body><p><span style=\" font-weight:600;\">Check for updates</span></p>"
                                  "<p>Looks whether a newer QtDMM is out and offers it.</p></body></html>"));
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
  connect(m_zoomIn, &QAction::triggered, this, [this]
  {
    if (const GraphView *view = currentGraph())
      view->graph()->zoomBy(1.25);
  });
  m_zoomOut = new QAction(tr("Zoom &out"), this);
  m_zoomOut->setObjectName("action_ZoomOut");
  m_zoomOut->setShortcut(QKeySequence::ZoomOut);
  connect(m_zoomOut, &QAction::triggered, this, [this]
  {
    if (const GraphView *view = currentGraph())
      view->graph()->zoomBy(1 / 1.25);
  });
  m_zoomFit = new QAction(tr("Show &whole recording"), this);
  m_zoomFit->setObjectName("action_ZoomFit");
  m_zoomFit->setShortcut(QKeySequence("Ctrl+0"));
  connect(m_zoomFit, &QAction::triggered, this, [this]
  {
    if (const GraphView *view = currentGraph())
      view->graph()->zoomFit();
  });
  // a note in the graph at the newest reading, also while the graph has no focus
  m_noteAction = new QAction(tr("&Note in the graph..."), this);
  m_noteAction->setObjectName("action_Note");
  m_noteAction->setShortcut(QKeySequence("Ctrl+Shift+N"));
  m_noteAction->setWhatsThis(tr("<html><head/><body><p><span style=\" font-weight:600;\">Note in the graph</span></p>"
                                "<p>Sets a note at the newest reading: a flag with its title under the graph, "
                                "its text on a click. In the graph's context menu also where you clicked; "
                                "N in the graph does the same.</p></body></html>"));
  connect(m_noteAction, &QAction::triggered, this, [this]
  {
    if (GraphView *view = currentGraph())
      view->graph()->askNote();
  });
  m_copyImage = new QAction(tr("Cop&y graph image"), this);
  m_copyImage->setObjectName("action_CopyImage");
  m_copyImage->setShortcut(QKeySequence("Ctrl+Shift+C"));
  m_copyImage->setWhatsThis(tr("<html><head/><body><p><span style=\" font-weight:600;\">Copy graph image</span></p>"
                               "<p>Puts a picture of the graph on the clipboard, ready to paste into a "
                               "report or a chat.</p></body></html>"));
  connect(m_copyImage, &QAction::triggered, this, [this]
  {
    if (GraphView *view = currentGraph())
      view->graph()->copyImage();
  });

  // Space (Record) toggles the recorder; Start and Stop have neither a
  // button nor a key of their own any more
  addActions({action_Configure, action_ConfigureDMM, action_Direct_help, action_Help, action_Quit,
              m_displayAction, m_meterAction, m_readingsAction, m_poincareAction, m_histogramAction, m_heatmapAction, m_basketAction, m_titleBars,
              m_fullScreen, m_toolbars, m_zoomIn, m_zoomOut, m_zoomFit, m_copyImage, m_noteAction,
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
  for (QAction *a : { action_Live, action_Clear, m_zoomIn, m_zoomOut, m_zoomFit, m_copyImage, m_noteAction,
                      m_graphExport, action_Import, action_Print })
    m_shortcuts->add(a, recording);
  for (QAction *a : { m_sidebarAction, m_displayAction, m_meterAction, m_newMeterAction, action_Graph,
                      m_readingsAction, m_poincareAction, m_newPoincareAction, m_histogramAction, m_newHistogramAction,
                      m_heatmapAction, m_newHeatmapAction,
                      m_xyAction, m_newXYAction, m_newGraphAction, m_basketAction, m_titleBars, m_toolbars,
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
  const auto state = RecordingStore::State(m_wid->recordingState());
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
    m_menu->addAction(m_histogramAction);
    m_menu->addAction(m_heatmapAction);
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
    m_menu->addAction(m_updateAction);
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
  if (watched == m_mdi && event->type() == QEvent::Resize && !m_emptyStart->isHidden())
    placeEmptyStart();
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
    for (QAction *a : { m_displayAction, m_meterAction, m_readingsAction, m_poincareAction, m_histogramAction, m_heatmapAction, action_Graph,
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
  // the front panels share the LCD key: on while one of them shows. Hidden
  // one by one (their window menu), it stayed on, and its first click only
  // switched it off - the panels seemed gone for good
  if ((event->type() == QEvent::Show || event->type() == QEvent::Hide) && !event->spontaneous() && !m_restoring)
    if (auto *win = qobject_cast<QMdiSubWindow *>(watched); win && panelWindows().contains(win))
    {
      const QList<QMdiSubWindow *> panels = panelWindows();
      const bool anyShown = std::any_of(panels.cbegin(), panels.cend(), [](const QMdiSubWindow *w) { return !w->isHidden(); });
      if (m_displayAction->isChecked() != anyShown)
      {
        QSignalBlocker block(m_displayAction);
        m_displayAction->setChecked(anyShown);
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
  HistogramView *histogram = histogramViewOf(win);
  HeatmapView *heatmap = heatmapViewOf(win);
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
  if ((meter || plot || histogram || heatmap) && !m_session->controllers().isEmpty())
    connect(menu.addAction(tr("S&ources...")), &QAction::triggered, this, [this, meter, plot, histogram, heatmap]
    {
      editSources(meter ? ValueMenu::View::Meter : heatmap ? ValueMenu::View::Heatmap
                            : histogram ? ValueMenu::View::Histogram : ValueMenu::View::Poincare,
                  meter, plot, nullptr, histogram, heatmap);
    });
  auto addBasicPlotActions = [this, &menu](QWidget *widget, bool held, std::function<void(bool)> setHold,
                                            std::function<void()> reset, const QString &fileName)
  {
    QAction *hold = menu.addAction(tr("&Hold picture"));
    hold->setCheckable(true);
    hold->setChecked(held);
    connect(hold, &QAction::toggled, widget, [setHold](bool on) { setHold(on); });
    connect(menu.addAction(tr("&Reset")), &QAction::triggered, widget, [reset] { reset(); });
    connect(menu.addAction(tr("&Save image...")), &QAction::triggered, this, [this, widget, fileName]
    {
      const QString png = tr("PNG image (*.png)");
      const ProjectFolder::Proposal where(fileName, ProjectFolder::Images);
      QString path = QFileDialog::getSaveFileName(this, tr("Save image"), where.path(), png);
      if (path.isEmpty()) return;
      if (QFileInfo(path).suffix().isEmpty()) path += ".png";
      if (!widget->grab().save(path))
        QMessageBox::warning(this, tr("Save image"), tr("Could not save the image."));
    });
  };
  GraphView *graphView = graphViewOf(win);
  if (graphView)
    setPictureGraph(graphView);   // the window's picture actions are for it now
  // on a mark's flag: its note (words of the context GraphWidget)
  const int graphMark = graphView ? graphView->graph()->markAtGlobal(globalPos) : -1;
  if (graphMark >= 0)
  {
    GraphWidget *graph = graphView->graph();
    auto word = [](const char *text) { return QCoreApplication::translate("GraphWidget", text); };
    if (const QString photo = graph->markPhoto(graphMark); !photo.isEmpty())
      connect(menu.addAction(tr("Show &photo")), &QAction::triggered, this, [this, photo] { showPhoto(photo); });
    if (graph->markIsNote(graphMark) && graph->marksEditable())
      connect(menu.addAction(word("&Edit note...")), &QAction::triggered, graph, [graph, graphMark] { graph->editMark(graphMark); });
    if (graph->marksEditable())
      connect(menu.addAction(word("&Remove mark")), &QAction::triggered, graph, [graph, graphMark] { graph->removeMark(graphMark); });
  }
  else if (graphView)
  {
    // the core menu (tmp/redesign/graph_kontextmenue.md): first what the
    // click point means, then recording and picture, the switches and the
    // looks in "Display", the pictures and files in "Output", the window last
    GraphWidget *graph = graphView->graph();
    GraphModel *model = graphView->model();
    auto word = [](const char *text) { return QCoreApplication::translate("GraphWidget", text); };
    // at the click point: a note, the time cursors (a click sets A, Shift+click B)
    const qint64 at = graph->timeAt(globalPos);
    if (graph->store() && graph->marksEditable() && at >= 0)
    {
      QAction *note = menu.addAction(word("Note here..."));
      note->setShortcut(QKeySequence(Qt::Key_N));   // shown as a hint: N alone notes at the newest reading
      connect(note, &QAction::triggered, graph, [graph, at] { graph->askNote(at); });
    }
    if (at >= 0)
      for (int which : { 0, 1 })
        connect(menu.addAction(which == 0 ? word("Cursor A here") : word("Cursor B here")), &QAction::triggered, graph,
                [graph, which, at] { graph->setTimeCursor(which, at); });
    // only when they do something
    if (graph->zoomed())
      connect(menu.addAction(tr("&Undo zoom")), &QAction::triggered, graph, [graph] { graph->endZoom(true); });
    if (graph->timeCursor(0) >= 0 || graph->timeCursor(1) >= 0)
      connect(menu.addAction(tr("Remove &time cursors")), &QAction::triggered, graph, &GraphWidget::clearTimeCursors);
    menu.addSeparator();
    // recording, as the button does: only the devices whose values it shows
    if (graph->store() && graph->store()->isRunning())
      connect(menu.addAction(word("Stop recording")), &QAction::triggered, action_Stop, &QAction::trigger);
    else if (!graphView->showsFile() && action_Start->isEnabled())
      connect(menu.addAction(word("Start recording")), &QAction::triggered, this, [this, graphView]
      {
        QStringList sources;
        for (const ValueRef &v : graphView->model()->values())
          sources << v.source;
        m_session->setRecordingScope(sources);
        action_Start->trigger();
        if (!m_session->isRecording())
          m_session->clearRecordingScope();   // cancelled
      });
    QAction *hold = menu.addAction(tr("&Hold picture"));
    hold->setCheckable(true);
    hold->setChecked(graph->hold());
    connect(hold, &QAction::toggled, graph, [graph](bool on) { graph->setHold(on); });
    // as Clear in the toolbar: the recording goes (asked first when not
    // saved), a recording or a file viewed: Live again
    connect(menu.addAction(tr("&Reset")), &QAction::triggered, action_Clear, &QAction::trigger);
    menu.addSeparator();
    {
      // what it shows: the switches, then its looks (own texts: the letters
      // of the GraphWidget words are taken in this menu)
      QMenu *display = menu.addMenu(tr("Displa&y"));
      auto toggle = [&](const QString &text, bool on, void (GraphModel::*set)(bool))
      {
        QAction *a = display->addAction(text);
        a->setCheckable(true);
        a->setChecked(on);
        connect(a, &QAction::toggled, model, set);
      };
      toggle(tr("Overvie&w"), model->minimapVisible(), &GraphModel::setMinimapVisible);
      toggle(tr("Min/max &band"), model->envelopeVisible(), &GraphModel::setEnvelopeVisible);
      toggle(tr("&Meter states"), model->statesVisible(), &GraphModel::setStatesVisible);
      toggle(tr("Stat&istics"), model->statsVisible(), &GraphModel::setStatsVisible);
      display->addSeparator();
      // this graph's own colours, or those of the settings
      QMenu *variants = display->addMenu(tr("Colour &variant"));
      auto *group = new QActionGroup(variants);
      auto entry = [&](const QString &text, const QString &name)
      {
        QAction *a = variants->addAction(text);
        a->setCheckable(true);
        a->setChecked(model->variant() == name);
        group->addAction(a);
        connect(a, &QAction::triggered, graphView, [model, name] { model->setVariant(name); });
      };
      entry(tr("As in the &settings"), QString());
      variants->addSeparator();
      for (GraphLook::Variant v : GraphLook::variants())
        entry(GraphLook::title(v), GraphLook::name(v));
      toggle(tr("&Patterned lines"), model->patternedLines(), &GraphModel::setPatternedLines);
      if (!graphView->showsFile() && !m_session->controllers().isEmpty() && !model->values().isEmpty())
        fillGraphAxisMenu(display->addMenu(tr("A&xis")), graphView);
      if (!graphView->showsFile())
        fillGraphReferenceMenu(display->addMenu(tr("Re&ference line")), graphView, QCursor::pos());
      // readings not drawn, the recording keeps them; the entries are the
      // words of the context GraphWidget, the menu's letter its own
      using namespace GraphOutliers;
      QMenu *outliers = display->addMenu(tr("Outli&ers"));
      const Rule rule = model->outliers();
      const QString unit = SiPrefix::displayText(rule.unit);
      auto mode = [&](const QString &text, Mode m)
      {
        QAction *a = outliers->addAction(text);
        a->setCheckable(true);
        a->setChecked(rule.mode == m);
        connect(a, &QAction::triggered, graph, [graph, m] { graph->askOutliers(m); });
      };
      mode(word("&Show all"), Shown);
      mode(rule.mode == Sigma ? word("&More than %1 σ from their neighbours...").arg(rule.a)
                              : word("&More than n σ from their neighbours..."), Sigma);
      mode(rule.mode == Jump ? word("&Spikes larger than %1 %2...").arg(rule.a).arg(unit) : word("&Spikes larger than..."), Jump);
      mode(rule.mode == Limits ? word("Outside %1 .. %2 %3...").arg(rule.a).arg(rule.b).arg(unit) : word("Outside &limits..."), Limits);
    }
    {
      // pictures and the recording; the window's actions without their letters
      QMenu *output = menu.addMenu(tr("Out&put"));
      connect(output->addAction(word("Copy image")), &QAction::triggered, graph, &GraphWidget::copyImage);
      connect(output->addAction(word("Export image...")), &QAction::triggered, this, [this, graphView] { exportGraphImage(graphView); });
      for (QAction *shared : { action_Print, m_graphExport, m_graphToBasket, m_labPlotLive })
      {
        QAction *a = output->addAction(shared->icon(), QString(shared->text()).remove('&'));
        a->setEnabled(shared->isEnabled());
        connect(a, &QAction::triggered, shared, &QAction::trigger);
      }
    }
    menu.addSeparator();
    if (!graphView->showsFile() && !m_session->controllers().isEmpty())
      connect(menu.addAction(tr("S&ources...")), &QAction::triggered, this, [this, graphView] { editGraphSources(graphView); });
    connect(menu.addAction(tr("&Load recording...")), &QAction::triggered, this, [this, graphView] { loadIntoGraph(graphView); });
    if (graphView->showsFile())
      connect(menu.addAction(tr("Back to the &device")), &QAction::triggered, graphView, &GraphView::closeFile);
    // another one, first with the same value
    const QList<ValueRef> values = model->values();
    connect(menu.addAction(tr("&New graph")), &QAction::triggered, this, [this, values] { newGraph(values.value(0)); });
    // the Graph page of the settings: colours, line, points, width
    connect(menu.addAction(word("Configure...")), &QAction::triggered, m_wid, &InstanceWidget::configGraphSLOT);
    // the window last
    menu.removeAction(hide);
    menu.addSeparator();
    menu.addAction(hide);
  }
  if (XYView *xy = xyViewOf(win))
  {
    if (!m_session->controllers().isEmpty())
      connect(menu.addAction(tr("S&ources...")), &QAction::triggered, this, [this, xy] { editXYSources(xy); });
    connect(menu.addAction(m_newXYAction->text()), &QAction::triggered, this, [this, xy]
    {
      newView(Added::XY, xy->xValue());
    });
    menu.addSeparator();
    addBasicPlotActions(xy->plot(), xy->plot()->hold(), [plot = xy->plot()](bool on) { plot->setHold(on); },
                        [plot = xy->plot()] { plot->clear(); }, QStringLiteral("xy.png"));
  }
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
  if (histogram)
  {
    connect(menu.addAction(m_newHistogramAction->text()), &QAction::triggered, this, [this, histogram]
    {
      newView(Added::Histogram, histogram->value());
    });
    menu.addSeparator();
    addBasicPlotActions(histogram->plot(), histogram->plot()->hold(),
                        [plot = histogram->plot()](bool on) { plot->setHold(on); },
                        [plot = histogram->plot()] { plot->clear(); }, QStringLiteral("histogram.png"));
  }
  if (heatmap)
  {
    QMenu *colorScheme = menu.addMenu(tr("&Color scheme"));
    auto *group = new QActionGroup(colorScheme);
    group->setExclusive(true);
    for (ColorMap::Scheme scheme : ColorMap::all())
    {
      QAction *action = colorScheme->addAction(ColorMap::name(scheme));
      action->setCheckable(true);
      action->setChecked(scheme == heatmap->plot()->colorScheme());
      group->addAction(action);
      connect(action, &QAction::triggered, this, [heatmap, scheme]
      {
        heatmap->plot()->setColorScheme(scheme);
      });
    }

    connect(menu.addAction(m_newHeatmapAction->text()), &QAction::triggered, this, [this, heatmap]
    {
      newView(Added::Heatmap, heatmap->value(0));
    });
    menu.addSeparator();
    addBasicPlotActions(heatmap->plot(), heatmap->plot()->hold(),
                        [plot = heatmap->plot()](bool on) { plot->setHold(on); },
                        [plot = heatmap->plot()] { plot->clear(); }, QStringLiteral("heatmap.png"));
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

bool MainWindow::editSources(ValueMenu::View view, MeterView *meter, PoincareView *plot, GraphView *graph,
                             HistogramView *histogram, HeatmapView *heatmap)
{
  if (view == ValueMenu::View::Graph)
  {
    if (!graph)
      graph = currentGraph();
    return editGraphSources(graph);
  }
  if (!meter)
    meter = meterViewOf(m_meterWin);
  if (!plot)
    plot = poincareViewOf(m_poincareWin);
  if (!histogram)
    histogram = histogramViewOf(m_histogramWin);
  if (!heatmap)
    heatmap = heatmapViewOf(m_heatmapWin);
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
  const bool several = view == ValueMenu::View::Table || view == ValueMenu::View::Heatmap;
  QList<ValueRef> chosen;
  QString name;
  switch (view)
  {
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
    case ValueMenu::View::Histogram:
      name = histogram->plot()->windowTitle();
      chosen << histogram->value();
      break;
    case ValueMenu::View::Heatmap:
      name = heatmap->plot()->windowTitle();
      chosen << heatmap->value(0) << heatmap->value(1) << heatmap->value(2);
      break;
    default:
      name = plot->plot()->windowTitle();
      chosen << plot->value();
      break;
  }
  QList<ValueRef> picked;
  ValueRef pickedZ;
  if (view == ValueMenu::View::Heatmap)
  {
    SourcesDlg dlg(name, meters, chosen.value(0), chosen.value(1), chosen.value(2), this);
    if (dlg.exec() != QDialog::Accepted)
      return false;
    picked << dlg.xValue() << dlg.yValue();
    pickedZ = dlg.zValue();
  }
  else
  {
    SourcesDlg dlg(name, several, meters, this);
    dlg.setChosen(chosen);
    if (dlg.exec() != QDialog::Accepted)
      return false;
    picked = dlg.chosen();
  }
  if (!several && picked.isEmpty())
    return false;
  switch (view)
  {
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
    case ValueMenu::View::Histogram:
      histogram->setValue(picked.first());
      if (histogram == histogramViewOf(m_histogramWin))
        m_histogramAction->setChecked(true);
      break;
    case ValueMenu::View::Heatmap:
      heatmap->setValues(picked[0], picked[1], pickedZ);
      if (heatmap == heatmapViewOf(m_heatmapWin))
        m_heatmapAction->setChecked(true);
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

bool MainWindow::editXYSources(XYView *view)
{
  if (!view)
    return false;
  QList<SourcesDlg::Meter> meters;
  for (const MeterController *ctl : m_session->controllers())
  {
    SourcesDlg::Meter meter;
    meter.source = ctl->source();
    meter.name = ctl->deviceName();
    for (int id = 0; id < ctl->valueCount(); ++id)
      meter.values << otherValueRow(ctl, id).name;
    meters << meter;
  }
  SourcesDlg dlg(view->plot()->windowTitle(), meters, view->xValue(), view->yValue(), this);
  if (dlg.exec() != QDialog::Accepted)
    return false;
  view->setValues(dlg.xValue(), dlg.yValue());
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
  else if (HistogramView *histogram = histogramViewOf(win))
    histogram->setValue(value);
  else if (HeatmapView *heatmap = heatmapViewOf(win))
    heatmap->setValues(value, heatmap->value(1), heatmap->value(2));
  else if (GraphView *graphView = graphViewOf(win))
  {
    // one more curve; a graph showing a file has none of a device
    if (graphView->showsFile())
    {
      statusBar()->showMessage(tr("A graph showing a loaded recording takes no values of a device: Back to the device in its menu"), 6000);
      return;
    }
    QList<ValueRef> values = graphView->model()->values();
    if (!values.contains(value))
      graphView->model()->setValues(values << value);
  }
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
    QAction *histogram = menu.addAction(m_newHistogramAction->icon(), tr("New &histogram"));
    QAction *heatmap = menu.addAction(m_newHeatmapAction->icon(), tr("New &heatmap"));
    QAction *graph = menu.addAction(action_Graph->icon(), tr("In the &graph"));
    QAction *newGraph = menu.addAction(action_Graph->icon(), tr("New g&raph"));
    QAction *chosen = menu.exec(m_mdi->viewport()->mapToGlobal(pos));
    if (chosen == meter)
      newView(Added::Meter, value);
    else if (chosen == plot)
      newView(Added::Poincare, value);
    else if (chosen == histogram)
      newView(Added::Histogram, value);
    else if (chosen == heatmap)
      newView(Added::Heatmap, value, true);
    else if (chosen == newGraph)
      newView(Added::Graph, value);
    else if (chosen == graph)
    {
      // the first graph; one more curve
      GraphView *first = graphViewOf(m_graphWin);
      if (first->showsFile())
        first->closeFile();
      QList<ValueRef> values = first->model()->values();
      if (!values.contains(value))
        first->model()->setValues(values << value);
      showGraphWindow(m_graphWin);
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
  QStringList inGraphs;
  for (QMdiSubWindow *win : graphWindows())
    if (const QColor color = win->isHidden() ? QColor() : graphViewOf(win)->graph()->drawnColor(value); color.isValid())
    {
      if (!row.color.isValid())
        row.color = color;   // the first graph's
      inGraphs << win->windowTitle();
    }
  // the views showing it, for the marks of its row
  if (QMdiSubWindow *win = m_panelWins.value(ctl->source()); win && !win->isHidden())
    if (const FrontPanel *panel = panelOf(win))
      row.onPanel = panel->value().id == id || panel->secondaries().contains(id);
  row.views << inGraphs;
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
  for (QMdiSubWindow *win : histogramWindows())
    if (const HistogramView *histogram = histogramViewOf(win); histogram && !win->isHidden() && histogram->value() == value)
    {
      ++row.histograms;
      row.views << win->windowTitle();
    }
  for (QMdiSubWindow *win : heatmapWindows())
    if (const HeatmapView *heatmap = heatmapViewOf(win); heatmap && !win->isHidden()
        && (heatmap->value(0) == value || heatmap->value(1) == value || heatmap->value(2) == value))
      row.views << win->windowTitle();
  return row;
}

void MainWindow::updateOpenDevices()
{
  // pressed while a device of the bench is connected (also by SCPI, its menu)
  bool any = false;
  for (const MeterController *ctl : m_session->controllers())
    any = any || ctl->isWanted();
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
    row.connected = ctl->isWanted();
    // the dot green while readings come in; why none come is on the front panel
    const Reading &r = ctl->lastReading();
    row.active = row.connected && ctl->shortState().isEmpty() && r.msecs > 0 && now - r.msecs < 3000;
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
  for (QMdiSubWindow *win : graphWindows())
    if (!win->isHidden() && graphViewOf(win)->graph()->drawnColor(value).isValid())
      wins << win;
  for (QMdiSubWindow *win : meterWindows())
    if (const MeterView *meter = meterViewOf(win); meter && !win->isHidden() && meter->value() == value)
      wins << win;
  for (QMdiSubWindow *win : poincareWindows())
    if (const PoincareView *plot = poincareViewOf(win); plot && !win->isHidden() && plot->value() == value)
      wins << win;
  for (QMdiSubWindow *win : histogramWindows())
    if (const HistogramView *histogram = histogramViewOf(win); histogram && !win->isHidden() && histogram->value() == value)
      wins << win;
  for (QMdiSubWindow *win : heatmapWindows())
    if (const HeatmapView *heatmap = heatmapViewOf(win); heatmap && !win->isHidden()
        && (heatmap->value(0) == value || heatmap->value(1) == value || heatmap->value(2) == value))
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
  target.newHistogram = [this](const ValueRef &v) { newView(Added::Histogram, v); };
  target.newHeatmap = [this](const ValueRef &v) { newView(Added::Heatmap, v); };
  target.newGraph = [this](const ValueRef &v) { newView(Added::Graph, v); };
  target.integrate = [this](const ValueRef &v) { addNode(IntegralNode::kType, v); };
  target.graphs = [this]
  {
    QList<std::pair<QString, GraphModel *>> graphs;
    for (const QMdiSubWindow *win : graphWindows())
      graphs << std::make_pair(win->windowTitle(), graphViewOf(win)->model());
    return graphs;
  };
  target.showGraph = [this](const GraphModel *model)
  {
    for (QMdiSubWindow *win : graphWindows())
      if (graphViewOf(win)->model() == model)
        showGraphWindow(win);
  };
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
                    : view == ValueMenu::View::Histogram ? m_histogramAction
                    : view == ValueMenu::View::Heatmap ? m_heatmapAction : action_Graph;
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
  // a calculated or simulated value (also one of before): its node, no meter settings
  if (device->isNode() || LegacyNodes::isLegacy(device->keys))
  {
    QString name = device->name;
    const std::optional<QVariantMap> keys = editNode(id, device->keys, &name);
    if (!keys)
      return;
    if (name != device->name)
      m_wid->devices()->rename(id, m_wid->devices()->uniqueName(name));
    if (*keys != device->keys)
      applyEntryKeys(id, *keys);
    return;
  }
  DeviceSettingsDlg dlg(device->name, this);
  dlg.settings()->setPorts(Transport::availablePorts() + m_wid->settings()->customPorts());
  dlg.settings()->setSigrokExe(m_wid->settings()->getString("Port settings/sigrok_exe", "sigrok-cli"));
  dlg.load(device->keys);
  dlg.setStatus([this, id] { return deviceStatus(id); });
  dlg.setFrameSource([this, id]
  {
    const MeterController *ctl = m_session->controller("device:" + id);
    return ctl ? ctl->dmm() : nullptr;
  });
  // the Data tab marks the range of the reading on the display
  dlg.setReading([this, id]() -> std::optional<Reading>
  {
    const MeterController *ctl = m_session->controller("device:" + id);
    if (!ctl)
      return std::nullopt;
    return ctl->lastReading();
  });
  // the names of a connected microcontroller's values, beside their units
  if (const MeterController *ctl = m_session->controller("device:" + id))
  {
    QStringList names;
    for (int i = 0; i < kMaxValues; ++i)
      names << ctl->valueName(i);
    dlg.settings()->setValueNames(names);
  }
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
  applyEntryKeys(id, keys);
}

void MainWindow::applyEntryKeys(const QString &id, const QVariantMap &keys)
{
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
    // a calculated value keeps its state (an integral its sum) when its
    // inputs stay: only its settings changed
    QJsonObject nodeState;
    QList<ValueRef> nodeInputs;
    QString nodeType;
    if (const NodeRunner *runner = NodeRunner::of(ctl))
    {
      nodeState = runner->node()->state();
      nodeInputs = runner->node()->inputs();
      nodeType = runner->node()->type();
    }
    m_session->close(id);
    openBeside(id);
    if (NodeRunner *runner = NodeRunner::of(m_session->controller("device:" + id));
        runner && !nodeState.isEmpty() && runner->node()->type() == nodeType)
    {
      QStringList before, after;
      for (const ValueRef &r : std::as_const(nodeInputs))
        before << r.toString();
      for (const ValueRef &r : runner->node()->inputs())
        after << r.toString();
      if (before == after)
        runner->node()->setState(nodeState);
    }
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
  switch (ctl->linkState())
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
  if (last.battery == Battery::Low)
    html += para(tr("The meter's battery is low."));
  else if (last.battery == Battery::Mid)
    html += para(tr("The meter's battery is half full."));
  else if (last.battery == Battery::Full)
    html += para(tr("The meter's battery is full."));
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
  if (const MeterController *ctl = m_session->open(id, &error, waitForDevice); ctl && ctl->dmm() && !ctl->dmm()->isOpen())
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
  // the rack of 27.1's first logic nodes is gone: their front panels took over
  for (const char *key : { "Windows/rack-order", "Windows/rack-geometry", "Windows/rack-open" })
    cfg->remove(key);
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
  // the graphs' settings below (applyGraphState())
  for (const QString &number : get("Windows/graphs", QString()).toString().split(' ', Qt::SkipEmptyParts))
    addViewWindow(Added::Graph, ValueRef(), number.toInt())->setVisible(get("Windows/graph-" + number + "-shown", true).toBool());
  for (const QString &number : get("Windows/poincares", QString()).toString().split(' ', Qt::SkipEmptyParts))
    addViewWindow(Added::Poincare, valueOf(get("Windows/poincare-" + number + "-value", QString()).toString()),
                  number.toInt())->show();
  for (const QString &number : get("Windows/histograms", QString()).toString().split(' ', Qt::SkipEmptyParts))
    addViewWindow(Added::Histogram, valueOf(get("Windows/histogram-" + number + "-value", QString()).toString()),
                  number.toInt())->show();
  for (const QString &number : get("Windows/xys", QString()).toString().split(' ', Qt::SkipEmptyParts))
  {
    QMdiSubWindow *win = addViewWindow(Added::XY, ValueRef(), number.toInt());
    XYView *view = xyViewOf(win);
    view->setValues(valueOf(get("Windows/xy-" + number + "-0", QString()).toString()),
                    valueOf(get("Windows/xy-" + number + "-1", QString()).toString()));
    win->show();
  }
  for (const QString &number : get("Windows/heatmaps", QString()).toString().split(' ', Qt::SkipEmptyParts))
  {
    QMdiSubWindow *win = addViewWindow(Added::Heatmap, ValueRef(), number.toInt());
    HeatmapView *view = heatmapViewOf(win);
    view->setValues(valueOf(get("Windows/heatmap-" + number + "-0", QString()).toString()),
                    valueOf(get("Windows/heatmap-" + number + "-1", QString()).toString()),
                    valueOf(get("Windows/heatmap-" + number + "-2", QString()).toString()));
    view->plot()->setColorScheme(ColorMap::fromKey(get("Windows/heatmap-" + number + "-scheme", QString()).toString()));
    win->show();
  }
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
    // the front panels too: arrangeNow() below moves those already open
    // (a workspace loaded with its meters connected)
    auto windows = [this]
    {
      return meterWindows() + poincareWindows() + xyWindows() + histogramWindows() + heatmapWindows() + graphWindows()
             + QList<QMdiSubWindow *>{ m_readingsWin } + panelWindows();
    };
    QMap<QString, QString> geometry;
    for (QMdiSubWindow *w : windows())
      geometry[w->objectName()] = get("Windows/geometry-" + w->objectName(), QString()).toString();
    for (const QString &id : std::as_const(m_pendingOpen))
      geometry["panel-" + id] = get("Windows/geometry-panel-" + id, QString()).toString();
    // once the window is shown and the area has its size: the automatic
    // layout for a start, then the stored positions on top of it
    QTimer::singleShot(0, this, [this, geometry, windows]
    {
      m_arranger->arrangeNow();
      for (QMdiSubWindow *w : windows())
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
  m_xyAction->setChecked(get("Windows/xy", false).toBool());
  m_histogramAction->setChecked(get("Windows/histogram", false).toBool());
  m_heatmapAction->setChecked(get("Windows/heatmap", false).toBool());
  m_basketAction->setChecked(get("Windows/basket", false).toBool());
  // "1@device:ab12cd34" (ValueRef)
  meterViewOf(m_meterWin)->setValue(valueOf(get("Windows/meter-value", 0).toString()));
  poincareViewOf(m_poincareWin)->setValue(valueOf(get("Windows/poincare-value", 0).toString()));
  xyViewOf(m_xyWin)->setValues(valueOf(get("Windows/xy-x", QString()).toString()),
                               valueOf(get("Windows/xy-y", QString()).toString()));
  histogramViewOf(m_histogramWin)->setValue(valueOf(get("Windows/histogram-value", QString()).toString()));
  heatmapViewOf(m_heatmapWin)->setValues(valueOf(get("Windows/heatmap-x", QString()).toString()),
                                         valueOf(get("Windows/heatmap-y", QString()).toString()),
                                         valueOf(get("Windows/heatmap-z", QString()).toString()));
  heatmapViewOf(m_heatmapWin)->plot()->setColorScheme(ColorMap::fromKey(get("Windows/heatmap-scheme", QString()).toString()));
  for (QMdiSubWindow *win : graphWindows())
    applyGraphState(graphViewOf(win), get, valueOf);
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
  for (QAction *a : { m_meterAction, action_Graph, m_readingsAction, m_poincareAction, m_xyAction, m_histogramAction, m_heatmapAction, m_basketAction })
    qobject_cast<QWidget *>(a->property("window").value<QObject *>())->setVisible(a->isChecked());
  if (!m_displayAction->isChecked())
    for (QMdiSubWindow *win : std::as_const(m_panelWins))
      win->hide();
  m_restoring = false;
  syncArrangeActions();
  m_arranger->arrange();
}

void MainWindow::applyGraphState(GraphView *view, const WorkspaceGet &get,
                                 const std::function<ValueRef(const QString &)> &valueOf)
{
  GraphModel *model = view->model();
  const QString key = "Windows/" + view->key();
  // what is missing keeps its default; without a window of its own the settings'
  model->readSettings([&get](const QString &k, const QVariant &def) { return get(k, def); }, key);
  if (get(key + "-window", 0).toInt() <= 0)
    model->setWindowSeconds(m_wid->settingsDialog()->graphPage()->windowSeconds());
  // the values and their axes through valueOf(): before 26.4 the window's
  // meter by its id alone ("1", "1:right"), the main value apart
  QList<ValueRef> values;
  const QString list = get(key + "-values", QStringLiteral("-")).toString();
  if (list != QLatin1String("-"))
    for (const QString &text : list.split(' ', Qt::SkipEmptyParts))
      values << valueOf(text);
  else if (view == graphViewOf(m_graphWin))
  {
    if (get("Windows/graph-main-shown", true).toBool())
      values << valueOf("0");
    for (const QString &text : get("Windows/graph-shown-values", QString()).toString().split(' ', Qt::SkipEmptyParts))
      values << valueOf(text);
  }
  // a file from before 26.4 names no meter: its values are those of a meter
  // connected. Without any they are none: the first meter connected takes
  // the empty graph
  auto own = [this, &list](ValueRef v)
  {
    if (list == QLatin1String("-") && v.source.isEmpty() && !m_session->controllers().isEmpty())
      v.source = m_session->controllers().first()->source();
    return v;
  };
  QHash<ValueRef, int> axes;
  for (const QString &entry : get(key + "-value-axes", QString()).toString().split(' ', Qt::SkipEmptyParts))
  {
    const qsizetype colon = entry.lastIndexOf(':');
    const QString side = colon < 0 ? QString() : entry.mid(colon + 1);
    const int axis = side == "left" ? 0 : side == "right" ? 1 : side == "third" ? 2
                   : side.startsWith("axis") ? side.mid(4).toInt() - 1 : GraphModel::kAxisAuto;
    if (const ValueRef v = own(valueOf(entry.left(colon))); colon > 0 && !v.source.isEmpty() && axis >= 0)
      axes.insert(v, axis);
  }
  for (ValueRef &v : values)
    v = own(v);
  values.removeIf([](const ValueRef &v) { return v.source.isEmpty(); });
  for (const ValueRef &v : model->valueAxes().keys())
    model->setValueAxis(v, GraphModel::kAxisAuto);
  for (auto it = axes.cbegin(); it != axes.cend(); ++it)
    model->setValueAxis(it.key(), it.value());
  model->setValues(values);
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
    for (QMdiSubWindow *w : meterWindows() + poincareWindows() + xyWindows() + histogramWindows() + heatmapWindows() + graphWindows() + QList<QMdiSubWindow *>{ m_readingsWin }
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
  set("Windows/xy", m_xyAction->isChecked());
  set("Windows/histogram", m_histogramAction->isChecked());
  set("Windows/heatmap", m_heatmapAction->isChecked());
  set("Windows/basket", m_basketAction->isChecked());
  set("MainWindow/show-graph", action_Graph->isChecked());
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
  set("Windows/xy-x", xyViewOf(m_xyWin)->xValue().toString());
  set("Windows/xy-y", xyViewOf(m_xyWin)->yValue().toString());
  set("Windows/histogram-value", histogramViewOf(m_histogramWin)->value().toString());
  set("Windows/heatmap-x", heatmapViewOf(m_heatmapWin)->value(0).toString());
  set("Windows/heatmap-y", heatmapViewOf(m_heatmapWin)->value(1).toString());
  set("Windows/heatmap-z", heatmapViewOf(m_heatmapWin)->value(2).toString());
  set("Windows/heatmap-scheme", ColorMap::key(heatmapViewOf(m_heatmapWin)->plot()->colorScheme()));
  QStringList plots;
  for (QMdiSubWindow *w : poincareWindows().mid(1))
  {
    const QString number = w->objectName().section('-', 1);
    plots << number;
    set("Windows/poincare-" + number + "-value", poincareViewOf(w)->value().toString());
  }
  set("Windows/poincares", plots.join(' '));
  QStringList xys;
  for (QMdiSubWindow *w : xyWindows().mid(1))
  {
    const QString number = w->objectName().section('-', 1);
    xys << number;
    XYView *view = xyViewOf(w);
    set("Windows/xy-" + number + "-0", view->xValue().toString());
    set("Windows/xy-" + number + "-1", view->yValue().toString());
  }
  set("Windows/xys", xys.join(' '));
  QStringList histograms;
  for (QMdiSubWindow *w : histogramWindows().mid(1))
  {
    const QString number = w->objectName().section('-', 1);
    histograms << number;
    set("Windows/histogram-" + number + "-value", histogramViewOf(w)->value().toString());
  }
  set("Windows/histograms", histograms.join(' '));
  QStringList heatmaps;
  for (QMdiSubWindow *w : heatmapWindows().mid(1))
  {
    const QString number = w->objectName().section('-', 1);
    heatmaps << number;
    HeatmapView *view = heatmapViewOf(w);
    for (int axis = 0; axis < 3; ++axis)
      set("Windows/heatmap-" + number + "-" + QString::number(axis), view->value(axis).toString());
    set("Windows/heatmap-" + number + "-scheme", ColorMap::key(view->plot()->colorScheme()));
  }
  set("Windows/heatmaps", heatmaps.join(' '));
  QStringList graphs;
  for (QMdiSubWindow *w : graphWindows())
  {
    graphViewOf(w)->model()->writeSettings([&set](const QString &k, const QVariant &v) { set(k, v); },
                                            "Windows/" + w->objectName());
    if (w != m_graphWin)
    {
      graphs << w->objectName().section('-', 1);
      set("Windows/" + w->objectName() + "-shown", !w->isHidden());
    }
  }
  set("Windows/graphs", graphs.join(' '));
  QStringList tableHidden;
  for (const ValueRef &v : m_readings->log()->hiddenValues())
    tableHidden << v.toString();
  tableHidden.sort();
  set("Windows/table-hidden-values", tableHidden.join(' '));
  set("Windows/table-devices", m_tableMeters.join(' '));
}

// A workspace file (WorkspaceFile): the window's keys, the devices connected,
// a name, a description and a picture.
QString MainWindow::basketName(ProjectFolder::Kind kind, const QString &suffix) const
{
  // "recordings/2026-11-08 0714 Sim A.csv": sorts by time, the folder says what it is
  const QString device = m_session->lead() ? m_session->lead()->deviceName() : QString();
  QString name = QDateTime::currentDateTime().toString("yyyy-MM-dd HHmm") + (device.isEmpty() ? "" : " " + device);
  name.replace(QRegularExpression(R"([/\\:*?"<>|])"), "_");
  return BasketWidget::freeName(ProjectFolder::dirFor(m_basket->folder(), kind), name + "." + suffix);
}

void MainWindow::graphToBasket()
{
  // the graph used last, else the first one
  graphToBasketOf(pictureGraph() ? pictureGraph() : graphViewOf(m_graphWin));
}

void MainWindow::photoSetup()
{
  const QString project = m_basket->folder();
  if (project.isEmpty())
    return;
  if (!m_photoDlg)
  {
    m_photoDlg = new PhotoDlg(project, this);
    m_photoDlg->setAttribute(Qt::WA_DeleteOnClose);
    connect(m_photoDlg, &PhotoDlg::error, m_wid, &InstanceWidget::error);
    // each photo: a mark in the table at its time, and a note in the graph
    // where one is shown
    connect(m_photoDlg, &PhotoDlg::photoAdded, this, [this](const QString &path, const QDateTime &when)
    {
      const QString title = tr("Photo");
      m_readings->log()->markAt(when, QColor::fromRgba(RecordingStore::kNoteColor), title);
      if (GraphView *view = currentGraph())
        view->graph()->addNote(-1, title, QFileInfo(path).fileName(), PhotoStore::relative(m_basket->folder(), path));
    });
    // a photo deleted in the dialog: its marks go along, in every graph
    connect(m_photoDlg, &PhotoDlg::photoRemoved, this, [this](const QString &path, const QDateTime &when)
    {
      m_readings->log()->unmarkAt(when, tr("Photo"));
      const QString photo = PhotoStore::relative(m_basket->folder(), path);
      for (QMdiSubWindow *w : m_mdi->subWindowList())
        if (GraphView *view = graphViewOf(w))
          for (int mark = view->graph()->markCount() - 1; mark >= 0; --mark)
            if (view->graph()->markPhoto(mark) == photo)
              view->graph()->removeMark(mark);
    });
  }
  m_photoDlg->show();
  m_photoDlg->raise();
  m_photoDlg->activateWindow();
}

void MainWindow::showPhoto(const QString &photo)
{
  const QString path = PhotoStore::absolute(m_basket->folder(), photo);
  if (!PhotoView::showPhoto(path, this))
    m_wid->error(tr("The photo %1 cannot be shown: not found in the open project.").arg(QDir::toNativeSeparators(photo)));
}

GraphView *MainWindow::currentGraph() const
{
  // the graph whose window is active, else the one used last, else the first
  if (GraphView *view = graphViewOf(m_mdi->activeSubWindow()))
    return view;
  return pictureGraph() ? pictureGraph() : graphViewOf(m_graphWin);
}

GraphView *MainWindow::pictureGraph() const
{
  return m_pictureGraph && m_pictureGraph->graph()->isVisibleTo(m_mdi) ? m_pictureGraph.data() : nullptr;
}

bool MainWindow::writeGraphRecording(GraphView *view, const QString &path, bool everyReading)
{
  if (!view->showsFile())
    return m_wid->writeRecordingTo(path, everyReading, view->model()->values());
  // a file loaded into it: that recording, all its values
  RecordingStore *store = view->graph()->store();
  QString error = QCoreApplication::translate("InstanceWidget", "nothing recorded");
  QList<int> extras;
  for (int i = 0; store && i < store->extraCount(); ++i)
    extras << i;
  if (store && store->count() > 0 && store->write(path, &error, everyReading, extras))
    return true;
  Q_EMIT m_wid->error(tr("Export failed: %1").arg(error));
  return false;
}

void MainWindow::graphToBasketOf(GraphView *view)
{
  const QString csv = basketName(ProjectFolder::Recordings, "csv");
  if (!writeGraphRecording(view, csv, false))
    return;
  const QString png = BasketWidget::freeName(ProjectFolder::dirFor(m_basket->folder(), ProjectFolder::Images),
                                             QFileInfo(csv).completeBaseName() + ".png");
  if (!view->graph()->exportImageFile(png))
    Q_EMIT m_wid->error(tr("Project: cannot write %1").arg(QDir::toNativeSeparators(png)));
  else
    Q_EMIT m_wid->error(tr("Into the project: %1").arg(QFileInfo(csv).completeBaseName()));
}

// The dialog of a graph's picture, with its last place
void MainWindow::exportGraphImage(GraphView *view)
{
  auto word = [](const char *text) { return QCoreApplication::translate("GraphWidget", text); };
  Settings *cfg = m_wid->settings();
  const QFileInfo last(cfg->getString("QtDMM/LastImagePath"));
  const QStringList suffixes = { "svg", "pdf", "png", "jpg" };
  const QString suffix = suffixes.contains(last.suffix().toLower()) ? last.suffix().toLower() : "svg";
  const QString name = last.baseName().isEmpty() ? QString("untitled") : last.absolutePath() + "/" + last.baseName();
  const QString svgFilter = word("Scalable vector graphics (*.svg)"), pdfFilter = word("PDF (*.pdf)"),
                pngFilter = word("PNG image (*.png)"), jpgFilter = word("JPEG image (*.jpg)");
  QString filter = suffix == "pdf" ? pdfFilter : suffix == "png" ? pngFilter : suffix == "jpg" ? jpgFilter : svgFilter;
  const ProjectFolder::Proposal where(name + "." + suffix, ProjectFolder::Images);
  QString path = QFileDialog::getSaveFileName(this, word("Export image"), where.path(),
                                              svgFilter + ";;" + pdfFilter + ";;" + pngFilter + ";;" + jpgFilter, &filter);
  if (path.isEmpty())
    return;
  // a name typed without a suffix follows the chosen filter
  if (!suffixes.contains(QFileInfo(path).suffix().toLower()))
    path += "." + QString(filter == pdfFilter ? "pdf" : filter == pngFilter ? "png" : filter == jpgFilter ? "jpg" : "svg");
  cfg->setString("QtDMM/LastImagePath", QDir().absoluteFilePath(path));
  if (!view->graph()->exportImageFile(path))
    Q_EMIT m_wid->error(word("Could not write %1").arg(QDir::toNativeSeparators(path)));
}

void MainWindow::exportDialog(int view)
{
  exportDialogFor(view, pictureGraph() ? pictureGraph() : graphViewOf(m_graphWin));
}

void MainWindow::exportDialogFor(int view, GraphView *graphView)
{
  ExportDlg dlg(static_cast<ExportDlg::What>(view), m_basket->folder(), m_wid->settings(), this);
  if (dlg.exec() != QDialog::Accepted)
    return;
  QString path = dlg.file();
  if (dlg.toBasket())
    path = basketName(dlg.what() == ExportDlg::Picture ? ProjectFolder::Images
                      : dlg.format() == QLatin1String(RecordingFile::kLabPlotProjectSuffix) ? ProjectFolder::Analysis
                                                        : ProjectFolder::Recordings, dlg.format());
  bool ok = false;
  QString error;
  switch (dlg.what())
  {
    case ExportDlg::Recording:
      // says why itself
      ok = graphView && (dlg.graphValuesOnly() || graphView->showsFile()) ? writeGraphRecording(graphView, path, dlg.everyReading())
                                                                    : m_wid->writeRecordingTo(path, dlg.everyReading());
      break;
    case ExportDlg::Table:
      ok = m_readings->log()->writeAny(path, &error);
      break;
    case ExportDlg::Picture:
      ok = graphView && graphView->graph()->exportImageFile(path);
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
  QString dir = ProjectFolder::dirFor(m_basket->folder(), ProjectFolder::Analysis);
  const bool inProject = !dir.isEmpty();
  if (!inProject)
  {
    dir = QDir(QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation)).filePath("QtDMM");
    QDir().mkpath(dir);
  }
  // "analysis/2026-11-09 1030 Sim A.csv" and ".lml" beside it, both new;
  // outside a project "... Sim A live.csv"
  QString name = QDateTime::currentDateTime().toString("yyyy-MM-dd HHmm") + " " + lead->deviceName();
  if (!inProject)
    name += " " + tr("live");
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
  connect(m_live, &LabPlotLive::stopped, this, [this, csv = QDir(dir).filePath(base + ".csv")]
  {
    if (m_live->writeFailed())
      Q_EMIT m_wid->error(tr("Live in LabPlot: cannot write %1, stopped").arg(QDir::toNativeSeparators(csv)));
    else
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
  const QString csv = basketName(ProjectFolder::Recordings, "csv");
  QString error;
  if (!m_readings->log()->write(csv, &error))
    Q_EMIT m_wid->error(tr("Project: cannot write %1: %2").arg(QDir::toNativeSeparators(csv), error));
  else
    Q_EMIT m_wid->error(tr("Into the project: %1").arg(QFileInfo(csv).fileName()));
}

void MainWindow::saveWorkspace(bool asNew)
{
  Settings *cfg = m_wid->settings();
  // the picture: the windows as they stand
  const QPixmap shot = m_mdi->grab().scaledToWidth(480, Qt::SmoothTransformation);
  // a new one starts without the name and file of the one in use
  const QString currentName = asNew ? QString() : m_workspaceName;
  const QString currentFile = asNew ? QString() : m_workspaceFile;
  WorkspaceDlg dlg(currentName, asNew ? QString() : m_workspaceDescription, shot, this);
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
  const QString proposal = !currentFile.isEmpty() && (dlg.name() == currentName || base.isEmpty())
                             ? currentFile
                             : QDir(dir).filePath((base.isEmpty() ? QString("untitled") : base) + "." + kWorkspaceSuffix);
  // a bench of the project stays where it is; one loaded from elsewhere goes
  // into the project all the same, among its benches
  const QString project = m_basket->folder();
  const bool ofProject = proposal == currentFile && !project.isEmpty()
                         && BasketWidget::projectOf(currentFile, project) == project;
  const ProjectFolder::Proposal where(proposal, ofProject ? ProjectFolder::Root : ProjectFolder::Benches);
  QString file = QFileDialog::getSaveFileName(this, tr("Save bench"), ofProject ? currentFile : where.path(),
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
  // the project's benches, the project, else where the last one came from
  const QString benches = QDir(m_basket->folder()).filePath(ProjectFolder::folderName(ProjectFolder::Benches));
  const QString dir = m_basket->folder().isEmpty() ? cfg->getString("Windows/workspace-dir",
                                                                    QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation))
                      : QFileInfo(benches).isDir() ? benches
                                                   : m_basket->folder();
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
  // the note first, the path in the tooltip (second line) [V 09.11.]
  QString text = tr("Bench loaded");
  // the bench belongs to the project in whose folder it lies: loading it
  // opens that one, or closes the open one when it lies outside; no
  // question, the status bar says so
  const QString before = m_basket->folder();
  const QString project = BasketWidget::projectOf(file, before);
  if (project != before)
  {
    m_basket->setFolder(project);
    text += "; "
            + (project.isEmpty()
                 ? tr("project %1 closed: the bench is not in it").arg(BasketWidget::projectName(before))
                 : tr("project %1 opened: the bench is in it").arg(BasketWidget::projectName(project)));
  }
  if (!restored.isEmpty())
    text += "; " + tr("back in My devices: %1").arg(restored.join(", "));
  if (programsLeft)
    text += "; " + tr("the programs of their alarms were left out, set them again in Alarms...");
  if (!keyless.isEmpty())
    text += "; " + tr("%1 without its key: enter it in the device settings").arg(keyless.join(", "));
  Q_EMIT m_wid->error(text + '\n' + QDir::toNativeSeparators(file));
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
  updateProjectSidebar();
}

void MainWindow::benchIntoNewProject()
{
  const QString before = m_basket->folder();
  m_basket->newAction()->trigger();
  const QString project = m_basket->folder();
  if (project.isEmpty() || project == before)
    return;   // no new project made
  // the bench into it: its name and file proposed, now in the project's folder
  saveWorkspace();
}

void MainWindow::updateProjectSidebar()
{
  const QString project = m_basket ? m_basket->folder() : QString();
  // without a device no project or bench either [E 09.11.]
  if (project.isEmpty() || !QFileInfo(project).isDir() || m_wid->devices()->list().isEmpty())
  {
    m_sidebar->setProject(QString(), {}, QString());
    return;
  }
  // its benches: the bench files in its folder and the folders under it
  QList<DeviceSidebar::Workspace> benches;
  QDirIterator it(project, { QString("*.") + kWorkspaceSuffix }, QDir::Files, QDirIterator::Subdirectories);
  while (it.hasNext())
  {
    const QString file = it.next();
    const std::optional<WorkspaceFile> ws = WorkspaceFile::read(file);
    benches.append({ file, ws ? ws->name : QString(), ws ? workspaceTooltip(file, *ws) : QString(), false });
  }
  std::sort(benches.begin(), benches.end(), [](const DeviceSidebar::Workspace &a, const DeviceSidebar::Workspace &b)
  {
    const QString na = a.name.isEmpty() ? QFileInfo(a.file).completeBaseName() : a.name;
    const QString nb = b.name.isEmpty() ? QFileInfo(b.file).completeBaseName() : b.name;
    return na.localeAwareCompare(nb) < 0;
  });
  m_sidebar->setProject(BasketWidget::projectName(project), benches, m_workspaceFile);
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
  const bool noDevices = m_wid->devices()->list().isEmpty();
  m_sidebar->setWorkspaces(noDevices ? QList<DeviceSidebar::Workspace>() : rows);
  m_recentMenu->clear();
  for (int i = 0; i < rows.size(); ++i)
  {
    const QString &file = files[i];
    // 1 ... 9 as the letters, then none
    QString name = rows[i].name.isEmpty() ? QFileInfo(file).completeBaseName() : rows[i].name;
    // and its project, if it lies in one
    if (const QString project = BasketWidget::projectOf(file, m_basket->folder()); !project.isEmpty())
      name += QStringLiteral(" \u2013 ") + BasketWidget::projectName(project);
    name.replace('&', "&&");
    QAction *a = m_recentMenu->addAction(i < 9 ? QString("&%1 %2").arg(i + 1).arg(name) : name);
    a->setToolTip(rows[i].tooltip.isEmpty() ? QDir::toNativeSeparators(file) : rows[i].tooltip);
    a->setEnabled(QFileInfo::exists(file));
    connect(a, &QAction::triggered, this, [this, file] { loadWorkspaceFile(file); });
  }
  m_recentMenu->setToolTipsVisible(true);
  m_recentMenu->menuAction()->setEnabled(!files.isEmpty() && !noDevices);
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
  if (graphViewOf(win))
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
  for (const GraphView *view : std::as_const(m_graphViews))
    view->graph()->setThemeColors(g.background, g.grid, g.labels, g.data);
  for (QMdiSubWindow *win : poincareWindows())
    applyPoincareColors(static_cast<PoincarePlot *>(win->widget()));
  for (QMdiSubWindow *win : xyWindows())
    applyPlotColors(static_cast<GraphCommon *>(win->widget()));
  for (QMdiSubWindow *win : histogramWindows())
    applyPlotColors(static_cast<GraphCommon *>(win->widget()));
  for (QMdiSubWindow *win : heatmapWindows())
    if (HeatmapView *view = heatmapViewOf(win))
      applyPlotColors(view->plot());
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

// The side plots on QCustomPlot (XY diagram, histogram, heatmap) on the
// design's frame, the graph's background where that is a texture: the points
// would get lost in it
void MainWindow::applyPlotColors(GraphCommon *plot)
{
  const Designs::Design d = Designs::current();
  const QBrush frame = Designs::frameBrush(d);
  const Designs::GraphColors g = Designs::graphColors(d);
  QBrush background = frame.style() != Qt::NoBrush ? frame : QApplication::palette().base();
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
  connect(this->action_Connect, &QAction::toggled, this, [=, this](bool checked)
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
