//======================================================================
// File:		mainwid.cpp
// Author:	Matthias Toussaint
// Created:	Tue Apr 10 17:29:01 CEST 2001
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
#include <QPrinter>
#include <iostream>
#include <cmath>

#include "ui/dialogs/recordlengthdlg.h"
#include "ui/instancewidget.h"
#include "device/transports/serial.h"
#include "core/devicelibrary.h"
#include "device/dmmdecoder.h"
#include "device/protocols.h"
#include "ui/controlbar.h"
#include "ui/views/graphwidget.h"
#include "ui/settings/settingsdialog.h"
#include "device/meterconnection.h"
#include "service/metercontroller.h"
#include "ui/views/lcdwidget.h"
#include "ui/views/analogmeter.h"
#include "recording/readingsmodel.h"
#include "core/alarm.h"
#include "core/siprefix.h"
#include "core/settings.h"
#include "service/sharedstatemanager.h"



InstanceWidget::InstanceWidget(QString instance_id, QString config_path, QWidget *parent) :  QFrame(parent),
  m_display(nullptr),
  m_meter(nullptr)
{
  setupUi(this);
  setWindowIcon(QPixmap(":/Symbols/icon.xpm"));

  // the meter session: connection, min/max, alarms, SCPI
  m_ctl = new MeterController(this);
  m_ctl->setInstanceId(instance_id.isEmpty() ? QString("default") : instance_id);

  m_instanceId = instance_id;
  m_configPath = config_path;
  m_settings  = new Settings(instance_id, config_path, this);
  // the integral is over time since 26.2: its scale once per settings file
  GraphWidget::migrateIntegralScale(m_settings);
  // "My devices", one file for all instances next to their settings. What
  // an entry keeps depends on how its model connects; the model table is
  // not in QtCore
  DeviceLibrary::setModelTransport([](const QString &model) -> QString
  {
    for (const DmmDecoder::DMMInfo &cfg : DmmDecoder::getDeviceConfigurations())
      if (DmmDecoder::sameModel(model, cfg.name))
      {
        // baud 0: the Bluetooth entry of a protocol that also has a cable
        const ProtocolInfo *info = protocolInfo(cfg.protocol);
        if (cfg.baud != 0 || !info || QLatin1String(info->transport) != QLatin1String("Bluetooth LE"))
          return QString();
        return cfg.protocol == FrameFormat::VictronBLE ? QStringLiteral("ble") : QStringLiteral("blegatt");
      }
    return QString();
  });
  m_devices = new DeviceLibrary(m_settings->configDir(), this);
  m_configDlg = new SettingsDialog(m_settings, this);
  m_configDlg->hide();
  m_configDlg->readPrinter(&m_printer);

  m_printDlg = new qtdmm::PrintDlg(this);
  m_printDlg->hide();

  connect(this, SIGNAL(sendState(const QString &)), parent, SLOT(sendStateSLOT(const QString &)));
  connect(m_ctl, &MeterController::error, this, &InstanceWidget::error);
  connect(m_ctl, &MeterController::info, this, &InstanceWidget::info);
  // the recorder belongs to the controller; the graph shows it
  ui_graph->setStore(m_ctl->recorder());
  connect(m_ctl, &MeterController::unitChanged, ui_graph, &GraphWidget::setUnit);
  connect(ui_graph, SIGNAL(info(const QString &)), this, SIGNAL(info(const QString &)));
  connect(ui_graph, SIGNAL(error(const QString &)), this, SIGNAL(error(const QString &)));
  connect(ui_graph, SIGNAL(running(bool)), this, SLOT(runningSLOT(bool)));
  connect(ui_graph, &GraphWidget::stateChanged, this, &InstanceWidget::recorderState);
  connect(ui_graph, &GraphWidget::liveRequested, this, &InstanceWidget::liveSLOT);
  // the graph runs from the start: Live, until a recording
  m_ctl->recorder()->live();
  // OK and Apply take the settings over; the meter stays as it is - the
  // dialog has no meter page, a meter is changed through takeOver()
  connect(m_configDlg, &SettingsDialog::accepted, this, [this]() { applySLOT(); });
  connect(m_configDlg, &SettingsDialog::applied, this, [this]() { applySLOT(); });
  connect(m_configDlg, SIGNAL(zoomed()), this, SLOT(zoomedSLOT()));
  connect(ui_graph, &GraphWidget::windowRequested, m_configDlg, &SettingsDialog::setWindowSecondsSLOT);
  connect(ui_graph, SIGNAL(sampleTime(int)), m_configDlg, SLOT(setSampleTimeSLOT(int)));
  // this graph's own colours from its context menu (empty = the default
  // from the settings page, applied in readConfig())
  connect(ui_graph, &GraphWidget::colorVariantChanged, this, [this](int v)
  {
    m_settings->setString("Windows/graph-variant",
                          v < 0 ? QString() : GraphWidget::variantName(static_cast<GraphWidget::ColorVariant>(v)));
  });
  connect(ui_graph, &GraphWidget::configure, this, [this] { openConfig(SettingsDialog::Graph); });
  connect(ui_graph, &GraphWidget::clearRequested, this, &InstanceWidget::clearSLOT);
  connect(ui_graph, SIGNAL(exportData()), this, SLOT(exportSLOT()));
  connect(ui_graph, SIGNAL(importData()), this, SLOT(importSLOT()));

  connect(ui_graph, SIGNAL(connectDMM(bool)), this, SIGNAL(connectDMM(bool)));

  connect(ui_graph, SIGNAL(zoomOut(double)), m_configDlg, SLOT(zoomOutSLOT(double)));
  connect(ui_graph, SIGNAL(zoomIn(double)), m_configDlg, SLOT(zoomInSLOT(double)));
  connect(ui_graph, SIGNAL(thresholdChanged(GraphWidget::CursorMode, double)),
          m_configDlg, SLOT(thresholdChangedSLOT(GraphWidget::CursorMode, double)));

  ui_graph->setSettings(m_settings);

  m_settings->save();
  Q_EMIT sendState("UPDATE_INSTANCES_"+QString::number(QDateTime::currentMSecsSinceEpoch()));

  // alarms: the controller judges every reading and runs the program; beep,
  // popup and raising the window are UI (the banner is MainWindow's)
  connect(m_ctl, &MeterController::alarmRaised, this, &InstanceWidget::alarmRaised);
  connect(m_ctl, &MeterController::recordingRequested, this, [this](bool start)
  {
    if (start)
      startSLOT();
    else
      stopSLOT();
  });
  connect(m_ctl, &MeterController::markRequested, this, [this](const QColor &color, const QString &name, bool graph, bool)
  {
    if (graph)
      ui_graph->addMark(color, Alarm::title(name));
  });
  m_ctl->setAlarms(m_configDlg->alarms());

  // SCPI server: the meter as a network instrument (applyScpi() starts it)
  connect(m_ctl, &MeterController::connectRequested, this, [this](bool on)
  {
    Q_EMIT setConnect(on);
    Q_EMIT connectDMM(on);
    connectSLOT(on);
  });
  // an alarm's program has the port for a while: the recording runs on
  connect(m_ctl, &MeterController::portReleased, this, [this](bool released)
  {
    Q_EMIT setConnect(!released);
    Q_EMIT connectDMM(!released);
    connectPort(!released);
  });
  connect(m_ctl, &MeterController::scpiStatusChanged, this, [this](const QString &status, const QString &detail)
  {
    Q_EMIT scpiStatus(status);
    m_configDlg->setScpiStatus(detail);
  });
  // HCOPy:SDUMp:DATA? - the main window as the "instrument screen"
  m_ctl->setScreenshotSource([this](const QByteArray &format) -> QByteArray
  {
    QWidget *top = window() ? window() : this;
    const QPixmap shot = top->grab();
    QByteArray bytes;
    QBuffer buffer(&bytes);
    buffer.open(QIODevice::WriteOnly);
    if (!shot.save(&buffer, format.constData()))
      return {};
    return bytes;
  });
}

void InstanceWidget::setConsoleLogging(bool on)
{
  m_ctl->dmm()->setConsoleLogging(on);
}

void InstanceWidget::setDisplay(LcdWidget *display)
{
  m_display = display;
  // the LCD's colours: backlight blue unless chosen otherwise; a config
  // from before the variants with a tint of its own lands on Custom
  QString lcd = m_settings->getString("Display/lcd");
  if (lcd.isEmpty())
    lcd = m_settings->getColor("Display/display-background", LcdWidget::classicFace()) == LcdWidget::classicFace()
            ? "blue" : "custom";
  display->setLcdVariant(LcdWidget::lcdVariantFromName(lcd));
  connect(m_ctl, &MeterController::reading, display, &LcdWidget::showReading);
  connect(m_ctl, &MeterController::staleChanged, display, &LcdWidget::setStale);
  connect(m_ctl, &MeterController::minimumChanged, display, &LcdWidget::showMinimum);
  connect(m_ctl, &MeterController::maximumChanged, display, &LcdWidget::showMaximum);
  connect(m_ctl, &MeterController::minMaxReset, display, &LcdWidget::clearMinMax);
}

void InstanceWidget::setMeter(AnalogMeter *meter)
{
  m_meter = meter;
  connect(m_ctl, &MeterController::reading, meter, &AnalogMeter::showReading);
  connect(m_ctl, &MeterController::staleChanged, meter, &AnalogMeter::setStale);
  connect(m_ctl, &MeterController::minimumChanged, meter, &AnalogMeter::showMinimum);
  connect(m_ctl, &MeterController::maximumChanged, meter, &AnalogMeter::showMaximum);
  connect(m_ctl, &MeterController::minMaxReset, meter, &AnalogMeter::clearMinMax);
}

void InstanceWidget::setReadingLog(ReadingsModel *log)
{
  // the table shows the recorder's readings series: the controller feeds it
  // whether or not a table exists
  log->setStore(m_ctl->recorder());
  connect(m_ctl, &MeterController::markRequested, log, [log](const QColor &color, const QString &name, bool, bool table)
  {
    if (table)
      log->markLast(color, name);
  });
}

bool InstanceWidget::closeWin()
{
  m_ctl->connectMeter(false);
  m_configDlg->setWinRect(parentRect());
  m_configDlg->on_ui_buttonBox_accepted();

  Q_EMIT setConnect(false);

  return keepUnsavedData(tr("If you quit now it will be lost."), tr("Quit without saving"));
}

bool InstanceWidget::keepUnsavedData(const QString &text, const QString &discard)
{
  if (!ui_graph->dirty() || !m_configDlg->alertUnsavedData())
    return true;

  QMessageBox question;
  question.setWindowTitle(tr("QtDMM: Unsaved data"));
  question.setText(tr("<font size=+2><b>Unsaved data</b></font><p>"
                      "You still have unsaved measured data in memory. %1"
                      "<p>Do you want to export your unsaved data first?").arg(text));
  question.setIcon(QMessageBox::Information);
  question.setIconPixmap(QPixmap(":/Symbols/icon.xpm"));

  // Set standard buttons
  question.setStandardButtons(QMessageBox::Yes | QMessageBox::No | QMessageBox::Cancel);
  question.setDefaultButton(QMessageBox::Yes);
  question.setEscapeButton(QMessageBox::Cancel);

  // Set custom button texts
  QAbstractButton *yesButton = question.button(QMessageBox::Yes);
  if (yesButton)
    yesButton->setText(tr("Export data first"));

  QAbstractButton *noButton = question.button(QMessageBox::No);
  if (noButton)
    noButton->setText(discard);

  switch (question.exec())
  {
    case QMessageBox::Yes:
      return ui_graph->exportDataSLOT();

    case QMessageBox::No:
      break;

    case QMessageBox::Cancel:
      return false;
  }

  return true;
}

QRect InstanceWidget::winRect() const
{
  return m_configDlg->winRect();
}

bool InstanceWidget::saveWindowPosition() const
{
  return m_configDlg->saveWindowPosition();
}

bool InstanceWidget::saveWindowSize() const
{
  return m_configDlg->saveWindowSize();
}

QRect InstanceWidget::parentRect() const
{
  // the main window: InstanceWidget itself sits inside an MDI window now
  QRect fRect = window()->frameGeometry();
  QRect rect  = window()->rect();

  return QRect(fRect.x(), fRect.y(), rect.width(), rect.height());
}

void InstanceWidget::setStateManager(SharedStateManager *mgr)
{
  m_ctl->setStateManager(mgr);
}

void InstanceWidget::resetSLOT()
{
  m_ctl->resetMinMax();
}

void InstanceWidget::connectSLOT(bool on)
{
  connectPort(on);
  if (!on)
    ui_graph->stopSLOT();
}

void InstanceWidget::connectPort(bool on)
{
  // the recording stays: the next start clears it (a reconnect after the
  // settings or an alarm program must not lose it)
  if (on && !m_ctl->connectMeter(true))
    Q_EMIT setConnect(false);   // the port could not be opened: button back to "off"
  else if (!on)
    m_ctl->connectMeter(false);

  m_configDlg->connectSLOT(on);
  ui_graph->connectSLOT(on);
}

void InstanceWidget::quitSLOT()
{
  if (closeWin()) qApp->quit();
}

void InstanceWidget::helpSLOT()
{
  QWhatsThis::enterWhatsThisMode();
}

void InstanceWidget::configSLOT()
{
  // opened plainly it starts on General, not on the page left last time;
  // already open, it stays where it is
  if (!m_configDlg->isVisible())
    m_configDlg->showPage(SettingsDialog::General);
  openConfig();
}

void InstanceWidget::openConfig(int page)
{
  // the meter goes on reading, and a recording on recording: the dialog
  // has no meter page any more (it disconnected for that, and stopped a
  // recording)
  if (page >= 0)
    m_configDlg->showPage(SettingsDialog::PageType(page));
  m_configDlg->show();
  m_configDlg->raise();
}

QString InstanceWidget::currentDevice() const
{
  const QString id = m_settings->getString("DMM/my-device");
  return !id.isEmpty() && m_devices->find(id) ? id : QString();
}

bool InstanceWidget::switchDevice(const QString &id, bool ask)
{
  const std::optional<MyDevice> device = m_devices->find(id);
  if (!device || (ask && !confirmSwitch(device->name)))
    return false;
  takeOver(device->keys, device->name, id);
  return true;
}

bool InstanceWidget::confirmSwitch(const QString &name)
{
  // the readings of the meter before are another function: export or drop them
  return keepUnsavedData(tr("Switching to %1 clears it.").arg(name), tr("Switch without saving"));
}

QString InstanceWidget::openInNewWindow(const QString &id)
{
  const std::optional<MyDevice> device = m_devices->find(id);
  if (!device)
    return QString();
  const QString instance = DeviceLibrary::instanceId(device->name, m_settings->getConfigInstances());
  // this instance's settings, the device's meter
  m_settings->copyConfig(instance);
  Settings created(instance, m_settings->configDir());
  created.setValues(device->keys);
  created.setString("DMM/my-device", device->id);
  created.setBool("DMM/configured", true);   // connects at its start
  created.save();

  QStringList args { "--config-id", instance };
  if (!m_configPath.isEmpty())
    args << "--config-dir" << m_configPath;
  QProcess::startDetached(QCoreApplication::applicationFilePath(), args);
  return instance;
}

void InstanceWidget::takeOver(const QVariantMap &keys, const QString &name, const QString &id)
{
  // another meter is another function: one recording does not mix them
  if (m_ctl->recorder()->isRunning())
  {
    ui_graph->stopSLOT();
    Q_EMIT error(tr("Recording stopped: switched to %1").arg(name));
  }
  Q_EMIT setConnect(false);
  Q_EMIT connectDMM(false);
  connectSLOT(false);

  m_settings->setValues(keys);
  m_settings->setString("DMM/my-device", id);
  m_settings->setBool("DMM/configured", true);
  m_settings->save();
  m_configDlg->reloadMeter();
  applySLOT();
  // another meter: its own minimum and maximum, even at the same port
  m_ctl->resetMinMax();
  ui_graph->liveSLOT();

  Q_EMIT setConnect(true);
  Q_EMIT connectDMM(true);
  connectSLOT(true);
}

void InstanceWidget::syncDevice()
{
  // another instance may have changed a formula here (renaming): the stale
  // cache must not go back into the entry
  m_settings->sync();
  QVariantMap keys = m_settings->meterKeys();
  keys.insert("Port settings/device", SerialDevice::stableDevice(keys.value("Port settings/device").toString()));
  const QString model = keys.value("DMM/model").toString();
  const QString stored = m_settings->getString("DMM/my-device");
  std::optional<MyDevice> device = m_devices->find(stored);
  if (!device || !DmmDecoder::sameModel(device->model(), model))
  {
    // another meter: one of My devices when it is at its place
    device = m_devices->find(m_devices->findByPlace(keys));
    if (device && !DmmDecoder::sameModel(device->model(), model))
      device.reset();
  }
  const QString id = device ? device->id : QString();
  if (id != stored)
  {
    m_settings->setString("DMM/my-device", id);
    m_settings->save();
  }
  // the same meter: a port or a key changed here changes its entry
  if (device && DeviceLibrary::entryKeys(keys) != device->keys)
    m_devices->update(id, keys);
}

void InstanceWidget::configRecorderSLOT()
{
  openConfig(SettingsDialog::Recorder);
}

void InstanceWidget::applySLOT()
{
  readConfig();
  m_ctl->setAlarms(m_configDlg->alarms());
  ui_graph->setAlertUnsaved(m_configDlg->alertUnsavedData());
  m_ctl->setModel(m_configDlg->dmmName());
  ScpiConfig scpi;
  scpi.enabled = m_configDlg->scpiEnabled();
  scpi.allInterfaces = m_configDlg->scpiAllInterfaces();
  scpi.port = quint16(m_configDlg->scpiPort());
  scpi.mdns = m_configDlg->scpiMdns();
  m_ctl->applyScpi(scpi);
  syncDevice();
  Q_EMIT configChanged();
}

void InstanceWidget::zoomedSLOT()
{
  ui_graph->setGraphSize(m_configDlg->windowSeconds());
}

void InstanceWidget::exportSLOT()
{
  ui_graph->exportDataSLOT();
}

void InstanceWidget::importSLOT()
{
  if (!keepUnsavedData(tr("Loading a file replaces it."), tr("Load without saving")))
    return;
  ui_graph->importDataSLOT();
}

void InstanceWidget::printSLOT()
{
  m_printDlg->setPrinter(&m_printer);

  if (m_printDlg->exec())
  {
    m_configDlg->writePrinter(&m_printer);
    ui_graph->print(&m_printer, m_printDlg->title(), m_printDlg->comment());
  }
}

void InstanceWidget::clearSLOT()
{
  if (!keepUnsavedData(tr("Clear deletes it."), tr("Clear without saving")))
    return;
  // an empty view is nothing to look at: Live again
  if (m_ctl->recorder()->state() == RecordingStore::View)
    ui_graph->liveSLOT();
  else
    ui_graph->clearSLOT();
}

void InstanceWidget::liveSLOT()
{
  if (m_ctl->recorder()->state() != RecordingStore::View)
  {
    Q_EMIT recorderState(int(m_ctl->recorder()->state()));   // the button shows the state again
    return;
  }
  if (keepUnsavedData(tr("Live clears it."), tr("Live without saving")))
    ui_graph->liveSLOT();
  else
    Q_EMIT recorderState(int(m_ctl->recorder()->state()));
}

bool InstanceWidget::confirmRecording()
{
  if (m_ctl->recorder()->state() != RecordingStore::View)
    return true;
  return keepUnsavedData(tr("A new recording clears it."), tr("Record without saving"));
}

bool InstanceWidget::askRecordingLength()
{
  // a start the settings set up waits in Live; this one starts now
  QString hint;
  if (m_configDlg->sampleMode() == GraphWidget::Time)
    hint = tr("It starts now. A recording at %1 starts by itself from Live.").arg(m_configDlg->startTime().toString());
  else if (m_configDlg->sampleMode() == GraphWidget::Raising || m_configDlg->sampleMode() == GraphWidget::Falling)
    hint = tr("It starts now. A recording at the threshold starts by itself from Live.");
  RecordLengthDlg dlg(m_configDlg->recordingLengthValue(), m_configDlg->recordingLengthUnit(), hint, this);
  if (dlg.exec() != QDialog::Accepted)
    return false;
  m_configDlg->setRecordingLength(dlg.value(), dlg.unit());
  ui_graph->setSampleLength(m_configDlg->sampleLength());
  return true;
}

void InstanceWidget::startSLOT()
{
  ui_graph->startSLOT();
}

void InstanceWidget::stopSLOT()
{
  ui_graph->stopSLOT();
}

void InstanceWidget::readConfig()
{
  MeterConnection *dmm = m_ctl->dmm();
  bool reopen = false;

  // the port only when the meter changed: a reopen costs a Bluetooth meter
  // seconds, and the readings and a recording a gap
  QVariantMap meter = m_settings->meterKeys();
  meter.insert("sigrok", m_configDlg->dmmInfo().sigrokExe);
  if (!dmm->isOpen() || meter != m_appliedMeter)
  {
    m_appliedMeter = meter;
    if (dmm->isOpen())
    {
      dmm->close();
      reopen = true;
    }

    dmm->setDmmInfo(m_configDlg->dmmInfo());
    dmm->setDevice(m_configDlg->device());
    dmm->setSpeed(m_configDlg->speed());
    dmm->setFormat(m_configDlg->format());
    dmm->setPortSettings(static_cast<QSerialPort::DataBits>(m_configDlg->bits()), static_cast<QSerialPort::StopBits>(m_configDlg->stopBits()),
                         m_configDlg->parity(), m_configDlg->externalSetup(), m_configDlg->rts(), m_configDlg->dtr() );
  }

  // the sample time first: setGraphSize() counts the window in samples, and
  // the x axis converts them back with the sample time
  ui_graph->setSampleTime(m_configDlg->sampleStep());
  ui_graph->setSampleLength(m_configDlg->sampleLength());
  ui_graph->store()->setPreTrigger(m_configDlg->preTrigger());
  ui_graph->setGraphSize(m_configDlg->windowSeconds());
  ui_graph->setStartTime(m_configDlg->startTime());
  ui_graph->setMode(m_configDlg->sampleMode());

  ui_graph->setCrosshair(m_configDlg->crosshair());

  ui_graph->setThresholds(m_configDlg->fallingThreshold(),
                          m_configDlg->raisingThreshold());

  ui_graph->setScale(m_configDlg->automaticScale(),
                     m_configDlg->includeZero(),
                     m_configDlg->scaleMin(),
                     m_configDlg->scaleMax());

  {
    const QString own = m_settings->getString("Windows/graph-variant");
    ui_graph->setColorVariant(GraphWidget::variantFromName(m_configDlg->graphVariant()),
                              own.isEmpty() ? -1 : int(GraphWidget::variantFromName(own)));
  }
  ui_graph->setColors(m_configDlg->bgColor(),
                      m_configDlg->gridColor(),
                      m_configDlg->dataColor(),
                      m_configDlg->cursorColor(),
                      m_configDlg->startColor(),
                      m_configDlg->intColor(),
                      m_configDlg->intThresholdColor());

  ui_graph->setLineStyle(m_configDlg->lineMode(),
                         m_configDlg->pointMode(),
                         m_configDlg->intLineMode(),
                         m_configDlg->intPointMode());

  // a tint changed on the Appearance page is meant to be seen: Custom
  const QColor tint = m_configDlg->displayBgColor();
  if (m_lcdTint.isValid() && tint != m_lcdTint && m_display->lcdVariant() != LcdWidget::Custom)
    setLcdVariant(LcdWidget::Custom);
  m_lcdTint = tint;
  m_display->setFaceColor(tint);
  m_display->setDisplayMode(m_configDlg->display(), m_configDlg->showMinMax(),
                            m_configDlg->showBar(), m_configDlg->numValues());
  dmm->setNumValues(m_configDlg->numValues());
  m_ctl->setDisplayCounts(m_configDlg->display());

  if (m_meter)
  {
    m_meter->setDisplayCounts(m_configDlg->display());
    applyMeterStyle();
    m_meter->setScaleMode(static_cast<AnalogMeter::ScaleMode>(
      m_configDlg->meterScaleMode() == 1 ? AnalogMeter::Unipolar :
      m_configDlg->meterScaleMode() == 2 ? AnalogMeter::Bipolar : AnalogMeter::Auto));
  }

  ui_graph->setLine(m_configDlg->lineWidth(), m_configDlg->intLineWidth());

  ui_graph->setIntegration(m_configDlg->showIntegration(),
                           m_configDlg->intScale(),
                           m_configDlg->intThreshold(),
                           m_configDlg->intOffset());

  if (m_configDlg->sampleMode() == GraphWidget::Time)
    Q_EMIT info(tr("Automatic start at %1").arg(m_configDlg->startTime().toString()));
  else if (m_configDlg->sampleMode() == GraphWidget::Raising)
    Q_EMIT info(tr("Raising threshold %1").arg(m_configDlg->raisingThreshold()));
  else if (m_configDlg->sampleMode() == GraphWidget::Falling)
    Q_EMIT info(tr("Falling threshold %1").arg(m_configDlg->fallingThreshold()));
  Q_EMIT useTextLabel(m_configDlg->useTextLabel());
  Q_EMIT iconSet(m_configDlg->iconSet());
  Q_EMIT remoteControl(ControlBar::supported(m_configDlg->dmmInfo()));
  Q_EMIT showDisplay(m_configDlg->showDisplay());

  if (reopen)
    dmm->open();
}

void InstanceWidget::runningSLOT(bool on)
{
  Q_EMIT running(on);
}

bool InstanceWidget::dmmConfigured() const
{
  // DMM/configured is set when a device is chosen (takeOver(), a new window
  // with a device); the model check keeps configs from before that key
  // working. "Manual" alone proves nothing: applySLOT() writes it at every
  // exit, dialog or not.
  if (m_settings->getBool("DMM/configured", false))
    return true;
  const QString model = m_settings->getString("DMM/model");
  return !model.isEmpty() && model != "Manual";
}

QString InstanceWidget::portName() const
{
  // a Bluetooth port string carries the encryption key: never show it
  const QString device = m_configDlg->device().simplified();
  const QString type = device.section(' ', 0, 0).toLower();
  if (type == "ble" || type == "blegatt")
    return "BLE " + device.section(' ', 1, 1);
  if (type == "calc")
    return tr("calculated");
  if (type == "serial")
    return device.section(' ', 1);   // "SERIAL /dev/ttyUSB0": the port alone
  return device;
}

QString InstanceWidget::dmmTitle() const
{
  if (!dmmConfigured())
    return tr("no meter configured");
  const QString model = m_configDlg->dmmName().trimmed();
  if (!model.isEmpty())
    return model;
  return m_configDlg->device().trimmed();
}

void InstanceWidget::setShowDisplay(bool show)
{
  m_configDlg->setShowDisplay(show);
}

// ---------------------------------------------------------------- alarms

// The controller has reported the alarm and run its program; what is left
// needs the desktop: beep, raise the window, pop up a box.
void InstanceWidget::alarmRaised(const Alarm &alarm, const QString &shown, const QString &text)
{
  if (alarm.beep)
    QApplication::beep();
  if (alarm.raiseWindow && window())
  {
    window()->raise();
    window()->activateWindow();
    QApplication::alert(window());
  }
  if (alarm.popup)
  {
    auto *box = new QMessageBox(QMessageBox::Warning, tr("QtDMM alarm: %1").arg(alarm.name),
                                QString("%1\n%2   %3").arg(text, shown, QDateTime::currentDateTime().toString("HH:mm:ss")),
                                QMessageBox::Ok, this);
    box->setAttribute(Qt::WA_DeleteOnClose);
    box->setModal(false);
    box->show();
  }
}

void InstanceWidget::setMeterStyle(int style)
{
  m_configDlg->setMeterStyle(style);
  m_settings->setInt("Meter/style", style);
  applyMeterStyle();
}

int InstanceWidget::meterStyle() const
{
  return m_configDlg->meterStyle();
}

void InstanceWidget::applyMeterStyle()
{
  if (!m_meter)
    return;
  AnalogMeterStyle style = m_configDlg->meterStyle() == 1 ? AnalogMeterStyle::ivory() : AnalogMeterStyle::dark();
  style.ballistics = m_configDlg->meterBallistics();
  style.redZoneFrom = m_configDlg->meterRedZone() / 100.0;
  m_meter->setStyle(style);
}

void InstanceWidget::setLcdVariant(int variant)
{
  const auto v = static_cast<LcdWidget::LcdVariant>(variant);
  m_display->setLcdVariant(v);
  m_settings->setString("Display/lcd", LcdWidget::lcdVariantName(v));
}
