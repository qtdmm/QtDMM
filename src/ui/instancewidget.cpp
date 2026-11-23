//======================================================================
// File:		mainwid.cpp
// Author:	Matthias Toussaint
// Created:	Tue Apr 10 17:29:01 CEST 2001
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
#include <QPrinter>
#include <iostream>
#include <cmath>
#include <optional>

#include "core/applog.h"
#include "core/projectfolder.h"
#include "ui/dialogs/recordlengthdlg.h"
#include "ui/alarmsound.h"
#include "ui/instancewidget.h"
#include <QPointer>
#include "device/transports/serial.h"
#include "core/devicelibrary.h"
#include "device/dmmdecoder.h"
#include "device/protocols.h"
#include "ui/controlbar.h"
#include "ui/frontpanel.h"
#include "ui/meterview.h"
#include "ui/poincareview.h"
#include "ui/settings/settingsdialog.h"
#include "ui/devicesettings.h"
#include "ui/settings/alarmprefs.h"
#include "ui/settings/generalprefs.h"
#include "ui/settings/guiprefs.h"
#include "ui/settings/keyboardprefs.h"
#include "ui/settings/recorderprefs.h"
#include "ui/settings/scpiprefs.h"
#include "ui/settings/webviewprefs.h"
#include "device/meterconnection.h"
#include "service/metercontroller.h"
#include "ui/views/lcdwidget.h"
#include "ui/views/analogmeter.h"
#include "ui/views/poincareplot.h"
#include "recording/recordingfile.h"
#include "recording/recordingstore.h"
#include "recording/readingsmodel.h"
#include "core/alarm.h"
#include "core/siprefix.h"
#include "core/settings.h"
#include "service/devicesession.h"
#include "service/servicehost.h"
#include "service/valuefeed.h"



InstanceWidget::InstanceWidget(QString instanceId, QString configPath, DeviceSession *session, QWidget *parent) :  QFrame(parent)
{
  setWindowIcon(QPixmap(":/Symbols/icon.xpm"));

  m_session = session;
  // the views' values: of a meter of the session
  connect(session, &DeviceSession::error, this, &InstanceWidget::error);
  connect(session, &DeviceSession::info, this, &InstanceWidget::info);
  m_instanceId = instanceId;
  m_configPath = configPath;
  m_settings  = new Settings(instanceId, configPath, this);
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
  // the Alarms page: the alarms of every open meter, each kept by its meter
  m_configDlg->alarmPage()->setDevices([this]
  {
    QList<AlarmPrefs::Device> out;
    for (const MeterController *ctl : m_session->controllers())
    {
      AlarmPrefs::Device d;
      d.name = ctl->deviceName();
      d.alarms = ctl->alarms()->alarms();
      d.source = ctl->source();
      const int count = ctl->valueCount();
      for (int id = 0; id < count; ++id)
      {
        const Reading r = ctl->lastReading(id);
        d.units.insert(id, r.baseUnit);
        if (count > 1)
          d.valueNames << QString("%1: %2").arg(id + 1).arg(!r.name.isEmpty() ? r.name
                                                             : r.baseUnit.isEmpty() ? tr("Value %1").arg(id + 1)
                                                             : RecordingStore::describe(r.port, r.baseUnit));
      }
      out << d;
    }
    return out;
  });
  m_configDlg->alarmPage()->setApply([this](const QString &source, const QList<Alarm> &alarms)
  {
    if (MeterController *ctl = m_session->controller(ValueRef(0, source)))
      ctl->setAlarms(alarms);
  });
  m_configDlg->readPrinter(&m_printer);

  m_printDlg = new qtdmm::PrintDlg(this);
  m_printDlg->hide();

  // --debug for every meter
  connect(m_session, &DeviceSession::opened, this, &InstanceWidget::applyConsoleLogging);
  // OK and Apply take the settings over; the meter stays as it is - the
  // dialog has no meter page, a meter is changed through takeOver()
  connect(m_configDlg, &SettingsDialog::accepted, this, &InstanceWidget::applySLOT);
  connect(m_configDlg, &SettingsDialog::applied, this, &InstanceWidget::applySLOT);

  m_settings->save();

  // alarms and the limit test: each meter's controller judges its readings
  // and runs the program; they are kept with its entry (DeviceSession). The
  // sound, the popup and raising the window are AlarmService's, the banner
  // MainWindow's, the LCD's colour its FrontPanel's
  // a recording asked for by a meter (SCPI INITiate, an alarm's program):
  // as the Record button, with its questions
  m_session->setRecordingRequests([this](bool start)
  {
    if (start)
      startSLOT();
    else
      stopSLOT();
  });
  // SCPI server: the meters as network instruments (applyScpi() starts it)
  connect(m_session->host(), &ServiceHost::statusChanged, this, [this](const QString &detail)
  {
    m_configDlg->scpiPage()->setStatus(detail);
    const QList<ServiceState> services = m_session->host()->services();
    for (const ServiceState &s : services)
      if (s.id == QLatin1String("webview"))
        m_configDlg->webViewPage()->setLink(s.address, s.error);
    Q_EMIT servicesChanged();
  });
  // HCOPy:SDUMp:DATA? - the main window as the "instrument screen"
  m_session->host()->setScreenshotSource([this](const QByteArray &format) -> QByteArray
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

void InstanceWidget::setConsoleLogging(bool on, const QStringList &only)
{
  m_consoleLogging = on;
  m_consoleOnly = only;
  for (MeterController *ctl : m_session->controllers())
    applyConsoleLogging(ctl);
}

void InstanceWidget::applyConsoleLogging(MeterController *ctl) const
{
  if (ctl->dmm())   // a logic node has no frames to log
    ctl->dmm()->setConsoleLogging(m_consoleLogging && (m_consoleOnly.isEmpty() || m_consoleOnly.contains(ctl->deviceId())));
}

MeterView *InstanceWidget::addMeter(AnalogMeter *meter)
{
  auto *view = new MeterView(m_session, meter, this);
  view->setLook(meterLook());
  m_meters << view;
  return view;
}

void InstanceWidget::removeMeter(MeterView *view)
{
  m_meters.removeOne(view);
  delete view;
}

PoincareView *InstanceWidget::addPoincare(PoincarePlot *plot)
{
  auto *view = new PoincareView(m_session, plot, this);
  m_poincares << view;
  return view;
}

void InstanceWidget::removePoincare(PoincareView *view)
{
  m_poincares.removeOne(view);
  delete view;
}

void InstanceWidget::setReadingLog(ReadingsModel *log)
{
  // the table shows the stores' readings series (MainWindow chooses the
  // meters): the controllers feed them whether or not a table exists
  m_readingLog = log;
}

bool InstanceWidget::closeWin()
{
  m_configDlg->setWinRect(parentRect());
  m_configDlg->on_ui_buttonBox_accepted();
  return keepUnsavedData(tr("If you quit now it will be lost."), tr("Quit without saving"));
}

bool InstanceWidget::keepUnsavedData(const QString &reason, const QString &discard)
{
  // the recording is the session's: any meter's not saved
  const QStringList unsaved = m_session->unsavedRecordings();
  if (unsaved.isEmpty() || !m_configDlg->generalPage()->alertUnsavedData())
    return true;
  const QString text = tr("Not saved: %1.").arg(unsaved.join(", ")) + ' ' + reason;

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
      // a recording still running stopped first, so it goes into the file
      if (!unsaved.isEmpty() && m_session->isRecording())
        m_session->stopRecording();
      if (!exportRecording())
        return false;
      // one not in that file (it recorded apart): asked again
      return m_session->unsavedRecordings().isEmpty() || keepUnsavedData(reason, discard);

    case QMessageBox::No:
      break;

    case QMessageBox::Cancel:
      return false;
  }

  return true;
}

QRect InstanceWidget::parentRect() const
{
  // the main window: InstanceWidget itself sits inside an MDI window now
  QRect fRect = window()->frameGeometry();
  QRect rect  = window()->rect();

  return QRect(fRect.x(), fRect.y(), rect.width(), rect.height());
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

void InstanceWidget::updateValueNames()
{
  m_configDlg->recorderPage()->setTriggerChoices(triggerChoices());
}

QList<RecorderPrefs::TriggerChoice> InstanceWidget::triggerChoices() const
{
  // the values of every meter connected, the meter's name in front
  QList<RecorderPrefs::TriggerChoice> out;
  for (const MeterController *ctl : m_session->controllers())
    for (int id = 0; id < ctl->valueCount(); ++id)
    {
      const Reading r = ctl->lastReading(id);
      const QString what = !r.name.isEmpty() ? r.name : r.baseUnit.isEmpty() ? tr("Value %1").arg(id + 1)
                                                                              : RecordingStore::describe(r.port, r.baseUnit);
      out.append({ ValueRef(id, ctl->source()), tr("%1 · %2").arg(ctl->deviceName(), what) });
    }
  return out;
}

void InstanceWidget::openConfig(int page)
{
  updateValueNames();
  // the meter goes on reading, and a recording on recording: the dialog
  // has no meter page any more (it disconnected for that, and stopped a
  // recording)
  // the alarms as the meters have them now (a SCPI client, another meter opened)
  if (!m_configDlg->isVisible())
  {
    m_configDlg->alarmPage()->defaultsSLOT();
    m_configDlg->alarmPage()->setCurrentDevice(m_activeSource);
  }
  if (page >= 0)
    m_configDlg->showPage(SettingsDialog::PageType(page));
  m_configDlg->show();
  m_configDlg->raise();
}

void InstanceWidget::configRecorderSLOT()
{
  openConfig(SettingsDialog::Recorder);
}

void InstanceWidget::configGraphSLOT()
{
  openConfig(SettingsDialog::Graph);
}

void InstanceWidget::configScpiSLOT()
{
  openConfig(SettingsDialog::Scpi);
}

void InstanceWidget::configWebViewSLOT()
{
  openConfig(SettingsDialog::WebView);
}

void InstanceWidget::applySLOT()
{
  readConfig();
  applyScpi();
  applyWebView();
  Q_EMIT configChanged();
}

void InstanceWidget::applyScpi()
{
  ScpiConfig scpi;
  scpi.enabled = m_configDlg->scpiPage()->enabled();
  scpi.allInterfaces = m_configDlg->scpiPage()->allInterfaces();
  scpi.port = quint16(m_configDlg->scpiPage()->port());
  scpi.mdns = m_configDlg->scpiPage()->mdns();
  scpi.hislip = m_configDlg->scpiPage()->hislip();
  scpi.hislipPort = quint16(m_configDlg->scpiPage()->hislipPort());
  scpi.vxi11 = m_configDlg->scpiPage()->vxi11();
  scpi.vxi11Rpc = Vxi11Server::modeFromName(m_configDlg->scpiPage()->vxi11Rpc());
  // mDNS names the model of the first meter
  const QList<MeterController *> all = m_session->controllers();
  m_session->host()->apply(scpi, m_instanceId.isEmpty() ? QString("default") : m_instanceId,
                           all.isEmpty() ? QString() : all.first()->model());
}

void InstanceWidget::applyWebView()
{
  const WebViewPrefs *page = m_configDlg->webViewPage();
  WebViewConfig web;
  web.enabled = page->enabled();
  web.allInterfaces = page->allInterfaces();
  web.port = quint16(page->port());
  web.interval = page->interval();
  web.token = page->token();
  m_session->host()->applyWebView(web);
}

bool InstanceWidget::writeRecordingTo(const QString &file, bool everyReading, const QList<ValueRef> &values)
{
  // a file viewed: that file; else the session's recording (Live too),
  // every meter that recorded with all its values, as QTDMM:RECord:SAVE
  // writes it
  if (fileShown() && m_fileView.write)
    return m_fileView.write(file, everyReading);   // it says why
  QString err = tr("nothing recorded");
  QPointer<MeterController> lead = m_session->lead();
  if (lead && lead->recorder()->count() > 0 && lead->writeRecording(file, everyReading, &err, values))
    return true;
  AppLog::error("app", "Export failed: " + err);
  Q_EMIT error(tr("Export failed: %1").arg(err));
  return false;
}

QString InstanceWidget::askExportFile(bool *raw)
{
  // the texts were the graph's, translated there
  auto word = [](const char *text) { return QCoreApplication::translate("GraphWidget", text); };
  const QFileInfo fileInfo(m_settings->getString("QtDMM/LastUsesPath"));
  const QStringList validSuffixes = { "csv", "xlsx", "ods", RecordingFile::kSuffix };
  const QString fnSuffix = validSuffixes.contains(fileInfo.suffix()) ? fileInfo.suffix() : "csv";
  QString fn = fileInfo.baseName().isEmpty() ? "untitled." + fnSuffix : fileInfo.absolutePath() + "/untitled." + fnSuffix;
  const QString csvFilter = word("CSV (*.csv)"), xlsxFilter = word("Excel (*.xlsx)"), odsFilter = word("OpenDocument (*.ods)");
  // the CSV with what it does not keep (marks), in a ZIP
  const QString qtdmmFilter = word("QtDMM recording (*.%1)").arg(RecordingFile::kSuffix);
  // every reading at its time instead of the sample time's grid
  const QString rawFilter = word("CSV, every reading (*.csv)");
  QString filter = fnSuffix == "xlsx" ? xlsxFilter : fnSuffix == "ods" ? odsFilter
                 : fnSuffix == RecordingFile::kSuffix ? qtdmmFilter : csvFilter;
  ProjectFolder::Proposal where(fn, ProjectFolder::Recordings);
  fn = QFileDialog::getSaveFileName(this, word("Export data"), where.path(),
                                    csvFilter + ";;" + xlsxFilter + ";;" + odsFilter + ";;" + rawFilter + ";;" + qtdmmFilter,
                                    &filter);
  if (fn.isNull())
    return QString();
  // the chosen filter decides when no suffix was typed
  if (QFileInfo(fn).suffix().isEmpty())
    fn += filter == xlsxFilter ? ".xlsx" : filter == odsFilter ? ".ods"
          : filter == qtdmmFilter ? QString(".") + RecordingFile::kSuffix : ".csv";
  *raw = filter == rawFilter;
  where.keep(fn);   // written by the caller
  m_settings->setString("QtDMM/LastUsesPath", QDir().absoluteFilePath(fn));
  return fn;
}

bool InstanceWidget::exportRecording()
{
  QPointer<MeterController> lead = m_session->lead();
  if (!lead)
    return true;   // nothing of a meter to keep
  bool raw = false;
  const QString file = askExportFile(&raw);
  if (file.isEmpty())
    return false;
  // the dialog runs events: SCPI may have closed it meanwhile
  if (!lead)
  {
    AppLog::error("app", "Export failed: the device was closed meanwhile");
    Q_EMIT error(tr("Export failed: the device was closed meanwhile"));
    return false;
  }
  QString err;
  if (!lead->writeRecording(file, raw, &err))
  {
    AppLog::error("app", "Export failed: " + err);
    Q_EMIT error(tr("Export failed: %1").arg(err));
    return false;
  }
  return true;
}

void InstanceWidget::printWith(const std::function<void(QPrinter *, const QString &, const QString &)> &print)
{
  // printing into a PDF with a project open: among its documents
  std::optional<ProjectFolder::Proposal> where;
  if (m_printer.outputFormat() == QPrinter::PdfFormat && !ProjectFolder::current().isEmpty())
  {
    where.emplace(m_printer.outputFileName().isEmpty() ? QStringLiteral("graph.pdf") : m_printer.outputFileName(),
                  ProjectFolder::Documents);
    m_printer.setOutputFileName(where->path());
  }
  m_printDlg->setPrinter(&m_printer);

  if (m_printDlg->exec())
  {
    m_configDlg->writePrinter(&m_printer);
    print(&m_printer, m_printDlg->title(), m_printDlg->comment());
  }
}

void InstanceWidget::clearSLOT()
{
  if (!keepUnsavedData(tr("Clear deletes it."), tr("Clear without saving")))
    return;
  // an empty view is nothing to look at: Live again
  if (recordingState() == RecordingStore::View)
    m_session->goLive();
  else
    m_session->clearRecording();
  leaveFile();
}

void InstanceWidget::leaveFile() const
{
  if (fileShown() && m_fileView.leave)
    m_fileView.leave();
}

void InstanceWidget::liveSLOT()
{
  // a file without a device stays in view beside a meter that is live
  if (recordingState() != RecordingStore::View && !fileShown())
  {
    Q_EMIT recorderState(recordingState());   // the button shows the state again
    return;
  }
  if (keepUnsavedData(tr("Live clears it."), tr("Live without saving")))
  {
    m_session->goLive();
    leaveFile();
  }
  else
    Q_EMIT recorderState(recordingState());
}

int InstanceWidget::recordingState() const
{
  const MeterController *lead = m_session->lead();
  if (lead)
    return int(lead->recorder()->state());
  return int(fileShown() ? RecordingStore::View : RecordingStore::Live);
}

bool InstanceWidget::confirmRecording()
{
  if (recordingState() != RecordingStore::View)
    return true;
  return keepUnsavedData(tr("A new recording clears it."), tr("Record without saving"));
}

bool InstanceWidget::askRecordingLength()
{
  // a start the settings set up waits in Live; this one starts now
  QString hint;
  if (m_configDlg->recorderPage()->sampleMode() == RecordingStore::Time)
    hint = tr("It starts now. A recording at %1 starts by itself from Live.").arg(m_configDlg->recorderPage()->startTime().toString());
  else if (m_configDlg->recorderPage()->sampleMode() == RecordingStore::Raising || m_configDlg->recorderPage()->sampleMode() == RecordingStore::Falling)
    hint = tr("It starts now. A recording at the threshold starts by itself from Live.");
  RecordLengthDlg dlg(m_configDlg->recorderPage()->lengthValue(), m_configDlg->recorderPage()->lengthUnit(), hint, this);
  if (dlg.exec() != QDialog::Accepted)
    return false;
  m_configDlg->setRecordingLength(dlg.value(), dlg.unit());
  applyRecordingSetup();
  return true;
}

void InstanceWidget::applyRecordingSetup()
{
  // the session's: every meter's store, the one of the trigger value leads
  const RecorderPrefs *recorder = m_configDlg->recorderPage();
  RecordingStore::Setup setup;
  setup.sampleTime = recorder->sampleStep();
  setup.sampleLength = recorder->sampleLength();
  setup.preTrigger = recorder->preTrigger();
  setup.mode = recorder->sampleMode();
  setup.startTime = recorder->startTime();
  setup.falling = recorder->fallingThreshold();
  setup.raising = recorder->raisingThreshold();
  setup.trigger = recorder->triggerValue();
  m_session->setRecordingSetup(setup);
}

void InstanceWidget::startSLOT()
{
  m_session->startRecording();
  leaveFile();
}

void InstanceWidget::stopSLOT()
{
  m_session->stopRecording();
}

void InstanceWidget::readConfig()
{
  const RecorderPrefs *recorder = m_configDlg->recorderPage();
  const GuiPrefs *gui = m_configDlg->guiPage();

  applyRecordingSetup();

  // a style chosen on the page Appearance: for all analog meters again
  const MeterView::Look look = meterLook();
  const bool restyle = m_meterStyle >= 0 && look.style != m_meterStyle;
  m_meterStyle = look.style;
  for (MeterView *view : std::as_const(m_meters))
  {
    if (restyle)
      view->setStyle(-1);
    view->setLook(look);
  }

  if (recorder->sampleMode() == RecordingStore::Time)
    Q_EMIT info(tr("Automatic start at %1").arg(recorder->startTime().toString()));
  else if (recorder->sampleMode() == RecordingStore::Raising)
    Q_EMIT info(tr("Raising threshold %1").arg(recorder->raisingThreshold()));
  else if (recorder->sampleMode() == RecordingStore::Falling)
    Q_EMIT info(tr("Falling threshold %1").arg(recorder->fallingThreshold()));
  Q_EMIT useTextLabel(gui->useTextLabel());
  Q_EMIT iconSet(gui->iconSet());
  Q_EMIT showDisplay(gui->showDisplay());
}

void InstanceWidget::openAlarms(const QString &source)
{
  openConfig(SettingsDialog::Alarms);
  m_configDlg->alarmPage()->setCurrentDevice(source);
}

MeterView::Look InstanceWidget::meterLook() const
{
  const GuiPrefs *gui = m_configDlg->guiPage();
  MeterView::Look look;
  look.style = gui->meterStyle();
  look.ballistics = gui->meterBallistics();
  look.redZone = gui->meterRedZone();
  look.scaleMode = gui->meterScaleMode() == 1 ? AnalogMeter::Unipolar
                   : gui->meterScaleMode() == 2 ? AnalogMeter::Bipolar : AnalogMeter::Auto;
  look.blink = m_configDlg->alarmPage()->blink();
  return look;
}

QString InstanceWidget::takeOldDevice()
{
  QString id = m_settings->getString("DMM/my-device");
  std::optional<MyDevice> entry = id.isEmpty() ? std::nullopt : m_devices->find(id);
  // a meter set up in the window alone (Manual, or chosen before My
  // devices): an entry of its own, or the one at its place for that model.
  // DMM/configured as before 26.4: "Manual" alone proves nothing, it was
  // written at every exit
  const QString model = m_settings->getString("DMM/model");
  if (!entry && (m_settings->getBool("DMM/configured", false) || (!model.isEmpty() && model != "Manual")))
  {
    QVariantMap keys = m_settings->meterKeys();
    keys.insert("Port settings/device", SerialDevice::stableDevice(keys.value("Port settings/device").toString()));
    id = m_devices->findByPlace(keys);
    entry = m_devices->find(id);
    if (!entry || !DmmDecoder::sameModel(entry->model(), model))
    {
      id = m_devices->add(model.isEmpty() || model == "Manual" ? tr("Meter") : model, keys);
      entry = m_devices->find(id);
    }
  }
  // the window keeps no meter: its keys go, also those of a window without one
  for (const QString &key : m_settings->meterKeys().keys())
    m_settings->remove(key);
  m_settings->remove("DMM/configured");
  if (!entry)
  {
    m_settings->remove("DMM/my-device");
    m_settings->save();
    return QString();
  }
  // the rules of the window before 26.4 go to its device, once
  const QString oldAlarms = m_settings->getString("Alarms/list");
  const QString oldLimit = m_settings->getString("Limits/test");
  if (entry->alarms.isEmpty() && entry->limitTest.isEmpty() && (!oldAlarms.isEmpty() || !oldLimit.isEmpty()))
    m_devices->setRules(id, oldAlarms, oldLimit);
  m_settings->remove("Alarms/list");
  m_settings->remove("Limits/test");
  m_settings->remove("DMM/my-device");
  m_settings->save();
  return id;
}
