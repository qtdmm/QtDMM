//======================================================================
// File:		main.cpp
// Author: Matthias Toussaint
// Created: Tue Apr 10 17:36:39 CEST 2001
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
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with QtDMM. If not, see <http://www.gnu.org/licenses/>.
//----------------------------------------------------------------------
// Copyright (c) 2001 Matthias Toussaint
// SPDX-License-Identifier: GPL-3.0-or-later
//======================================================================
#include <QLockFile>
#include <QMessageBox>
#include <QtGui>
#include <iostream>

#include "ui/mainwindow.h"
#include "ui/mnemoniccheck.h"
#include "ui/dialogs/formulanodedlg.h"
#include "ui/dialogs/generatornodedlg.h"
#include "ui/dialogs/integralnodedlg.h"
#include "ui/dialogs/adddevicedlg.h"
#include "core/applog.h"
#include "core/settings.h"
#include "ui/views/qcpgraphcommon.h"
#include "service/metercontroller.h"

#ifdef Q_OS_WIN
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <stdio.h>

static void attachParentConsole()
{
  if (::AttachConsole(ATTACH_PARENT_PROCESS))
  {
    FILE *f = nullptr;
    freopen_s(&f, "CONOUT$", "w", stdout);
    freopen_s(&f, "CONOUT$", "w", stderr);
    qputenv("QT_FORCE_STDERR_LOGGING", "1");
  }
}
#endif

void qtdmmMessageOutput(QtMsgType type, const QMessageLogContext &context, const QString &msg)
{
  const char *prefix = "";
  switch (type)
  {
    case QtDebugMsg: prefix = context.category ? context.category : "Debug"; break;
    case QtInfoMsg: prefix = "Info"; break;
    case QtWarningMsg: prefix = "Warning"; break;
    case QtCriticalMsg: prefix = "Critical"; break;
    case QtFatalMsg: prefix = "Fatal"; break;
  }
  fprintf(stderr, "%s: %s\n", prefix, msg.toLocal8Bit().constData());
  fflush(stderr);
  const AppLog::Level level = type == QtDebugMsg ? AppLog::Debug : type == QtInfoMsg ? AppLog::Info
                              : type == QtWarningMsg ? AppLog::Warning : AppLog::Error;
  const QLatin1String category(context.category ? context.category : "default");
  AppLog::instance()->add(level, category == QLatin1String("default") ? QStringLiteral("app") : QString(category), msg);
  if (type == QtFatalMsg)
    abort();
}

void initTranslation(QApplication *app, QTranslator *QtTranslation, QTranslator *AppTranslation)
{
  const QLocale locale = QLocale::system();
  const bool english = locale.language() == QLocale::English || locale.language() == QLocale::C;
  const QString qtDir = QLibraryInfo::path(QLibraryInfo::TranslationsPath);
  if (QtTranslation->load(locale, "qt", "_", qtDir) || QtTranslation->load(locale, "qtbase", "_", qtDir))
    app->installTranslator(QtTranslation);
  else if (!english)
    qWarning() << "Could not load Qt translation for" << locale.name();
  const QStringList dirs = {":/Translations", QCoreApplication::applicationDirPath() + "/../share/qtdmm/translations"};
  bool loaded = false;
  for (const QString &dir : dirs)
  {
    if (AppTranslation->load(locale, "qtdmm", "_", dir))
    {
      loaded = true;
      break;
    }
  }
  if (loaded)
    app->installTranslator(AppTranslation);
  else if (!english)
    qWarning() << "Could not load App translation for" << locale.name();
}

int main(int argc, char **argv)
{
#ifdef Q_OS_WIN
  attachParentConsole();
#endif
  qInstallMessageHandler(qtdmmMessageOutput);
  QApplication app(argc, argv);
  QTranslator QtTranslation;
  QTranslator AppTranslation;
  QCommandLineParser parser;

  app.setApplicationName(APP_NAME);
  app.setApplicationVersion(APP_VERSION);
  AppLog::info("app", QString("QtDMM %1, Qt %2, %3").arg(APP_VERSION, qVersion(), QSysInfo::prettyProductName()));
  app.setOrganizationName(APP_ORGANIZATION);
  app.setDesktopFileName("io.github.qtdmm.qtdmm");

  initTranslation(&app, &QtTranslation, &AppTranslation);
  parser.addOption({"debug", QObject::tr("protocol debugging information")});
  parser.addOption({"debug-device", QObject::tr("only the frames of <id> of My devices (repeatable)"), "id"});
  parser.addOption({"config-dir", QObject::tr("sets directory where config files are located"), "config-dir"});
  parser.addOption({"config-id", QObject::tr("sets <config-id>"), "config-id"});
  parser.addOption({"connect", QObject::tr("connects <id> of My devices at the start (repeatable)"), "id"});
  parser.addOption({"also", QObject::tr("the same as --connect (its name before 26.4)"), "id"});
  parser.addOption({"vxi11-rpc", QObject::tr("VXI-11 portmapper for this run: auto, system (rpcbind) or own (port 111)"), "mode"});
  parser.addOption({"vxi11-portmapper-port", QObject::tr("for tests and development: VXI-11 with an own portmapper on this port instead of 111 (0: any free one) for this run; VISA and lxi-tools ask port 111"), "port"});
  QCommandLineOption checkMnemonics("check-mnemonics");
  checkMnemonics.setFlags(QCommandLineOption::HiddenFromHelp);
  parser.addOption(checkMnemonics);
  parser.addHelpOption();
  parser.addVersionOption();
  parser.process(app);

  if (parser.isSet("vxi11-rpc"))
  {
    bool ok = false;
    const Vxi11Server::RpcbindMode mode = Vxi11Server::modeFromName(parser.value("vxi11-rpc"), &ok);
    if (!ok)
    {
      std::cerr << qPrintable(QObject::tr("--vxi11-rpc: auto, system or own, not \"%1\"").arg(parser.value("vxi11-rpc"))) << std::endl;
      return 2;
    }
    ServiceHost::setVxi11RpcOverride(mode);
  }
  if (parser.isSet("vxi11-portmapper-port"))
  {
    bool ok = false;
    const uint port = parser.value("vxi11-portmapper-port").toUInt(&ok);
    bool system = false;
    if (parser.isSet("vxi11-rpc"))
      system = Vxi11Server::modeFromName(parser.value("vxi11-rpc")) == Vxi11Server::RpcbindMode::System;
    if (!ok || port > 65535 || system)
    {
      std::cerr << qPrintable(system ? QObject::tr("--vxi11-portmapper-port needs the own portmapper, not --vxi11-rpc system")
                                     : QObject::tr("--vxi11-portmapper-port: a port from 0 to 65535, not \"%1\"").arg(parser.value("vxi11-portmapper-port"))) << std::endl;
      return 2;
    }
    ServiceHost::setVxi11PortmapperPort(quint16(port));
  }

  if (parser.isSet(checkMnemonics))
  {
    Settings cfg(parser.value("config-id"), parser.value("config-dir"));
    if (!cfg.fileExists())
    {
      cfg.setInt("QtDMM/version", 0);
      cfg.setInt("QtDMM/revision", 84);
      cfg.save();
    }
  }

  const QString configFile = Settings(parser.value("config-id"), parser.value("config-dir")).fileName();
  QDir().mkpath(QFileInfo(configFile).absolutePath());
  QLockFile lock(configFile + ".running");
  lock.setStaleLockTime(0);
  if (!lock.tryLock(500))
  {
    qint64 pid = 0;
    QString host, name;
    lock.getLockInfo(&pid, &host, &name);
    const QString text = QObject::tr("QtDMM is running already with these settings (process %1).").arg(pid);
    std::cerr << qPrintable(text) << std::endl;
    if (!parser.isSet(checkMnemonics))
      QMessageBox::critical(nullptr, APP_NAME, text);
    return 1;
  }

  Settings graphSettings(parser.value("config-id"), parser.value("config-dir"));
  QcpGraphCommon::setOpenGlEnabledDefault(graphSettings.getBool("Graph/openGl", false));

  MainWindow mainWin(parser);
  mainWin.show();
  mainWin.move(100, 100);

  if (parser.isSet(checkMnemonics))
  {
    auto *addDevice = new AddDeviceDlg(nullptr, &mainWin);
    addDevice->setDiscoverers([](AddDeviceDlg::Connection) { return QList<Discoverer *>(); });
    addDevice->chooseConnection(AddDeviceDlg::Cable);
    auto *formula = new FormulaNodeDlg(&mainWin);
    formula->load(QStringLiteral("Power"), FormulaNode());
    auto *generator = new GeneratorNodeDlg(&mainWin);
    generator->load(QStringLiteral("Test"), GeneratorNode());
    auto *integral = new IntegralNodeDlg(&mainWin);
    integral->setState([] { return std::optional<Reading>(); }, [] {});
    integral->load(QStringLiteral("Energy"), IntegralNode());
    QTimer::singleShot(0, &mainWin, [&mainWin]
    {
      const QStringList conflicts = MnemonicCheck::run(&mainWin);
      for (const QString &c : conflicts)
        std::cout << qPrintable(c) << std::endl;
      std::cout << conflicts.size() << " conflict(s), language " << qPrintable(QLocale::system().name()) << std::endl;
      QCoreApplication::exit(conflicts.isEmpty() ? 0 : 1);
    });
  }

  const int result = app.exec();
  qInstallMessageHandler(nullptr);
  return result;
}
