// Copyright (c) 2026 The QtDMM developers
// SPDX-License-Identifier: GPL-3.0-or-later
#include "core/sourcebuild.h"

#include <QCoreApplication>
#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QProcess>
#include <QStandardPaths>
#include <QThread>

#include <cstdio>

// set for this file only by CMakeLists.txt (not in packages: no paths of
// the build machine in their program)
#ifndef QTDMM_SOURCE_DIR
#define QTDMM_SOURCE_DIR ""
#endif
#ifndef QTDMM_BUILD_DIR
#define QTDMM_BUILD_DIR ""
#endif
#ifndef QTDMM_CMAKE_COMMAND
#define QTDMM_CMAKE_COMMAND "cmake"
#endif
#ifndef QTDMM_CPACK_COMMAND
#define QTDMM_CPACK_COMMAND "cpack"
#endif

namespace
{
QString same(const QString &path)
{
  const QFileInfo info(path);
  return info.exists() ? info.canonicalFilePath() : QDir::cleanPath(path);
}

QString program(const char *name)
{
  return QStandardPaths::findExecutable(name);
}
}   // namespace

SourceBuild::SourceBuild(QObject *parent)
  : QObject(parent), m_exe(QCoreApplication::applicationFilePath()), m_elevator(program("pkexec"))
{
  setDirs(builtSourceDir(), builtBuildDir());
}

QString SourceBuild::builtSourceDir()
{
  return QString::fromUtf8(QTDMM_SOURCE_DIR);
}

QString SourceBuild::builtBuildDir()
{
  return QString::fromUtf8(QTDMM_BUILD_DIR);
}

bool SourceBuild::available(const QString &source, const QString &build)
{
#ifdef Q_OS_WIN
  Q_UNUSED(source)
  Q_UNUSED(build)
  return false;
#else
  return !source.isEmpty() && !build.isEmpty() && QFileInfo::exists(source + "/.git")
         && QFileInfo::exists(build + "/CMakeCache.txt") && !program("git").isEmpty();
#endif
}

SourceBuild::Install SourceBuild::installMode(const QString &exe, const QString &source, const QString &build,
                                              bool packaged)
{
  const QString dir = same(QFileInfo(exe).absolutePath());
  // macOS: …/build/qtdmm.app/Contents/MacOS
  if (dir == same(build) || dir.startsWith(same(build) + "/"))
    return Install::BuildDir;
#ifdef Q_OS_MACOS
  Q_UNUSED(source)
  Q_UNUSED(packaged)
  return Install::None;
#else
  if (dir == same(source + "/bin"))
    return Install::BinCopy;
  return packaged ? Install::Deb : Install::CMakeInstall;
#endif
}

bool SourceBuild::debPackaged(const QString &path)
{
  if (program("dpkg").isEmpty())
    return false;
  QProcess dpkg;
  dpkg.start("dpkg", { "-S", path });
  return dpkg.waitForFinished(5000) && dpkg.exitStatus() == QProcess::NormalExit && dpkg.exitCode() == 0;
}

void SourceBuild::setDirs(const QString &source, const QString &build)
{
  m_source = source;
  m_build = build;
  m_buildCommand = { QString::fromUtf8(QTDMM_CMAKE_COMMAND), "--build", build, "--parallel",
                     QString::number(QThread::idealThreadCount()) };
  m_mode = source.isEmpty() ? Install::None : installMode(m_exe, source, build, debPackaged(m_exe));
}

QString SourceBuild::cpackCommand() const
{
  return QString::fromUtf8(QTDMM_CPACK_COMMAND);
}

void SourceBuild::start()
{
  if (running())
    return;
  const QString git = program("git");
  m_steps.clear();
  m_restart.clear();
  m_started = QDateTime::currentMSecsSinceEpoch();

  m_steps.append({ tr("Looking for changes not committed"),
                   [this, git] { return Command{ git, { "status", "--porcelain", "--untracked-files=no" }, m_source }; },
                   [this](const QByteArray &out) {
                     return out.trimmed().isEmpty()
                              ? QString()
                              : tr("The sources in %1 have changes not committed; commit or stash them first.")
                                  .arg(QDir::toNativeSeparators(m_source));
                   },
                   {} });
  m_steps.append({ tr("Looking for the branch's upstream"),
                   [this, git] {
                     return Command{ git, { "rev-parse", "--abbrev-ref", "--symbolic-full-name", "@{u}" }, m_source };
                   },
                   {}, {} });
  m_steps.append({ tr("git pull --ff-only"),
                   [this, git] { return Command{ git, { "pull", "--ff-only" }, m_source }; }, {}, {} });
  m_steps.append({ tr("Building"),
                   [this] { return Command{ m_buildCommand.value(0), m_buildCommand.mid(1), m_build }; }, {}, {} });

  const QString cmake = QString::fromUtf8(QTDMM_CMAKE_COMMAND);
  switch (m_mode)
  {
  case Install::BuildDir:
    m_restart = m_exe;
    m_done = tr("Built. The new version starts with Restart now.");
    break;
  case Install::BinCopy:
    m_steps.append({ tr("Copying it to %1").arg(QDir::toNativeSeparators(m_source + "/bin")), {}, {},
                     [this] { return copyToBin(); } });
    m_restart = m_exe;
    m_done = tr("Built and copied. The new version starts with Restart now.");
    break;
  case Install::CMakeInstall:
    if (m_elevator.isEmpty())
      m_done = tr("Built. To install it, run: sudo %1 --install %2").arg(cmake, m_build);
    else
    {
      m_steps.append({ tr("Installing (cmake --install)"),
                       [this, cmake] { return Command{ m_elevator, { cmake, "--install", m_build }, m_build }; }, {},
                       {} });
      m_restart = m_exe;
      m_done = tr("Built and installed. The new version starts with Restart now.");
    }
    break;
  case Install::Deb:
    m_steps.append({ tr("Making the .deb package"),
                     [this] { return Command{ cpackCommand(), { "-G", "DEB", "--config", "CPackConfig.cmake" }, m_build }; },
                     {}, {} });
    if (m_elevator.isEmpty())
      m_done = tr("Built, the package is in %1. To install it, run: sudo apt-get install ./<package>.deb")
                 .arg(QDir::toNativeSeparators(m_source + "/packages"));
    else
    {
      // pkexec runs a program by its full path, in a clean environment
      const QString aptGet = program("apt-get").isEmpty() ? QString("/usr/bin/apt-get") : program("apt-get");
      m_steps.append({ tr("Installing the package (apt-get)"),
                       [this, aptGet] {
                         const QString deb = newestDeb();
                         return Command{ deb.isEmpty() ? QString() : m_elevator,
                                         { aptGet, "install", "-y", deb }, m_build };
                       },
                       {}, [] { return tr("cpack made no .deb package."); } });
      m_restart = m_exe;
      m_done = tr("Built and installed as a package. The new version starts with Restart now.");
    }
    break;
  case Install::None:
    m_done = tr("Built in %1.").arg(QDir::toNativeSeparators(m_build));
    break;
  }
  m_next = 0;
  runNext();
}

void SourceBuild::runNext()
{
  if (m_next >= m_steps.size())
  {
    m_next = -1;
    Q_EMIT finished(true, m_done);
    return;
  }
  const Step &s = m_steps.at(m_next);
  Q_EMIT step(s.what);
  const Command c = s.command ? s.command() : Command{};
  if (c.program.isEmpty())
  {
    // no process: a step of its own, or a command that could not be made
    const QString error = s.run ? s.run() : QString();
    if (!error.isEmpty())
      return fail(error);
    ++m_next;
    runNext();
    return;
  }
  Q_EMIT output("$ " + c.program + ' ' + c.args.join(' ') + '\n');
  m_stepOutput.clear();
  m_process = new QProcess(this);
  m_process->setProcessChannelMode(QProcess::MergedChannels);
  m_process->setWorkingDirectory(c.dir);
  connect(m_process, &QProcess::readyRead, this, [this] {
    const QByteArray data = m_process->readAll();
    m_stepOutput += data;
    Q_EMIT output(QString::fromLocal8Bit(data));
  });
  connect(m_process, &QProcess::finished, this, [this](int exitCode, QProcess::ExitStatus status) {
    stepDone(status == QProcess::NormalExit ? exitCode : -1);
  });
  connect(m_process, &QProcess::errorOccurred, this, [this, c](QProcess::ProcessError error) {
    if (error == QProcess::FailedToStart)
      fail(tr("%1 could not be started.").arg(c.program));
  });
  m_process->start(c.program, c.args);
}

void SourceBuild::stepDone(int exitCode)
{
  if (!running() || !m_process)
    return;
  m_process->deleteLater();
  m_process = nullptr;
  const Step &s = m_steps.at(m_next);
  if (exitCode != 0)
    return fail(tr("%1 failed (exit code %2).").arg(s.what).arg(exitCode));
  const QString error = s.check ? s.check(m_stepOutput) : QString();
  if (!error.isEmpty())
    return fail(error);
  ++m_next;
  runNext();
}

void SourceBuild::fail(const QString &message)
{
  if (!running())
    return;
  if (m_process)
  {
    m_process->disconnect(this);
    m_process->kill();
    m_process->waitForFinished(2000);
    m_process->deleteLater();
    m_process = nullptr;
  }
  m_next = -1;
  m_restart.clear();
  Q_EMIT finished(false, message);
}

void SourceBuild::cancel()
{
  fail(tr("Cancelled."));
}

QString SourceBuild::copyToBin() const
{
  // as compile.sh: the program and its translations; each through a new
  // file renamed over the old one, the running program keeps its own
  const QDir build(m_build);
  const QDir bin(m_source + "/bin");
  QStringList files = build.entryList({ "qtdmm*.qm" }, QDir::Files);
  files.prepend(QFileInfo(m_exe).fileName());
  for (const QString &name : std::as_const(files))
  {
    const QString target = bin.filePath(name);
    const QString part = target + ".new";
    QFile::remove(part);
    if (!QFile::copy(build.filePath(name), part))
      return tr("%1 could not be copied to %2.").arg(name, QDir::toNativeSeparators(bin.path()));
    if (QFile::exists(target))
      QFile::setPermissions(part, QFile::permissions(target));
    if (std::rename(QFile::encodeName(part).constData(), QFile::encodeName(target).constData()) != 0)
    {
      QFile::remove(part);
      return tr("%1 could not be replaced.").arg(QDir::toNativeSeparators(target));
    }
  }
  return {};
}

QString SourceBuild::newestDeb() const
{
  // the one cpack made in this run (CPACK_PACKAGE_DIRECTORY: <sources>/packages)
  const QFileInfoList debs = QDir(m_source + "/packages").entryInfoList({ "*.deb" }, QDir::Files, QDir::Time);
  if (debs.isEmpty() || debs.first().lastModified().toMSecsSinceEpoch() < m_started - 2000)
    return {};
  return debs.first().absoluteFilePath();
}
