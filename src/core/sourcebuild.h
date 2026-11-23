// Copyright (c) 2026 The QtDMM developers
// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <QObject>
#include <QStringList>

#include <functional>

class QProcess;

/// Brings a build from the sources up to date: git pull in the source
/// directory it was built from, the build in its build directory, then the
/// program the user runs replaced the way it got there (installMode()).
/// The steps run one after the other as processes, their output in
/// output(); it ends in finished().
///
/// It stops before building when the sources have changes not committed,
/// the branch has no upstream or the pull is no fast-forward: nothing is
/// stashed, merged or rebased. Installing into the system goes through
/// pkexec (it asks for the password); without pkexec it ends after the
/// build and says the command to run.
class SourceBuild : public QObject
{
  Q_OBJECT
public:
  /// How the running program got where it is, which says how to replace it.
  enum class Install
  {
    BuildDir,       ///< it runs from the build directory: the build is all
    BinCopy,        ///< compile.sh's copy in <sources>/bin: copied again
    CMakeInstall,   ///< cmake --install (compile.sh install): again, as root
    Deb,            ///< a .deb of it (compile.sh pack): a new one, installed with apt
    None            ///< elsewhere: built only
  };

  explicit SourceBuild(QObject *parent = nullptr);

  /// The source and build directory this program was built in; empty in
  /// builds without the update check (packages).
  static QString builtSourceDir();
  static QString builtBuildDir();
  /// Whether this can run: a git checkout in @p source, a configured build
  /// directory @p build, git found; not on Windows (the running program
  /// cannot be overwritten there).
  static bool available(const QString &source = builtSourceDir(), const QString &build = builtBuildDir());
  /// How the program @p exe got there: the build directory, <source>/bin,
  /// else installed (@p packaged: dpkg owns it). macOS only builds.
  static Install installMode(const QString &exe, const QString &source, const QString &build, bool packaged);
  /// Whether dpkg owns the file @p path.
  static bool debPackaged(const QString &path);

  /// Tests: other directories, program, configure and build command, way to install, and
  /// what runs a command as root (empty: none there).
  void setDirs(const QString &source, const QString &build);
  void setExecutable(const QString &exe) { m_exe = exe; }
  void setConfigureCommand(const QStringList &command) { m_configureCommand = command; }
  void setBuildCommand(const QStringList &command) { m_buildCommand = command; }
  void setInstallMode(Install mode) { m_mode = mode; }
  void setElevator(const QString &program) { m_elevator = program; }

  Install mode() const { return m_mode; }
  QString sourceDir() const { return m_source; }
  QString buildDir() const { return m_build; }

  void start();
  void cancel();
  bool running() const { return m_next >= 0; }
  /// After a successful run: what to start for the new version; empty when
  /// it was built only.
  QString restartPath() const { return m_restart; }

Q_SIGNALS:
  /// A step begins (one line for the dialog).
  void step(const QString &what);
  /// Output of the step running.
  void output(const QString &text);
  /// The end: @p ok and what to tell (an error, or what was done).
  void finished(bool ok, const QString &message);

private:
  struct Command
  {
    QString program;   ///< empty: done in run (copy)
    QStringList args;
    QString dir;
  };
  struct Step
  {
    QString what;
    std::function<Command()> command;          ///< built when the step starts
    std::function<QString(const QByteArray &)> check;   ///< after it: an error, or empty
    std::function<QString()> run;              ///< a step without a process: an error, or empty
  };

  void runNext();
  void stepDone(int exitCode);
  void fail(const QString &message);
  QString copyToBin() const;
  QString newestDeb() const;
  QString cpackCommand() const;

  QString m_source;
  QString m_build;
  QString m_exe;
  QStringList m_configureCommand;
  QStringList m_buildCommand;
  Install m_mode;
  QString m_elevator;
  QList<Step> m_steps;
  int m_next = -1;
  QProcess *m_process = nullptr;
  QByteArray m_stepOutput;
  QString m_restart;
  QString m_done;   ///< what finished() says on success
  qint64 m_started = 0;   ///< ms since epoch the run began (the .deb made by it)
};
