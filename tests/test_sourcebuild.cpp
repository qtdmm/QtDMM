// Copyright (c) 2026 The QtDMM developers
// SPDX-License-Identifier: GPL-3.0-or-later
// Update and build (SourceBuild) on a repository of its own in a temporary
// folder: an "origin" one commit ahead of its clone, a stand-in build
// directory and a build command that only touches a file. git runs without
// the user's configuration. What it stops at (changes not committed, no
// upstream, a failed build, Cancel), the pull, how the program is replaced
// (build directory, compile.sh's bin/ copy, cmake --install without pkexec).
// Installing as root is not run here. Argument: the cmake program.
#include "core/sourcebuild.h"

#include "testutil.h"

#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QProcess>
#include <QTemporaryDir>

namespace
{
QString git(const QString &dir, const QStringList &args)
{
  QProcess p;
  p.setWorkingDirectory(dir);
  p.start("git", QStringList{ "-c", "user.name=test", "-c", "user.email=test@example.invalid", "-c",
                              "commit.gpgsign=false" }
                   + args);
  p.waitForFinished(10000);
  if (p.exitCode() != 0)
    check(false, "git " + args.join(' ') + ": " + QString::fromLocal8Bit(p.readAllStandardError()));
  return QString::fromLocal8Bit(p.readAllStandardOutput()).trimmed();
}

void write(const QString &path, const QByteArray &data)
{
  QFile f(path);
  check(f.open(QIODevice::WriteOnly) && f.write(data) == data.size(), "write " + path);
}

QByteArray read(const QString &path)
{
  QFile f(path);
  return f.open(QIODevice::ReadOnly) ? f.readAll() : QByteArray();
}

struct Result
{
  bool done = false;
  bool ok = false;
  QString message;
  QStringList steps;
};

Result run(SourceBuild &build, const std::function<void()> &meanwhile = {})
{
  Result r;
  QObject ctx;
  QObject::connect(&build, &SourceBuild::step, &ctx, [&r](const QString &what) { r.steps << what; });
  QObject::connect(&build, &SourceBuild::finished, &ctx, [&r](bool ok, const QString &message) {
    r.done = true;
    r.ok = ok;
    r.message = message;
  });
  build.start();
  if (meanwhile)
    meanwhile();
  check(waitFor([&r] { return r.done; }, 30000), "the run ends");
  return r;
}
}   // namespace

int main(int argc, char **argv)
{
  QCoreApplication app(argc, argv);
#ifdef Q_OS_WIN
  qInfo() << "Update and build is not offered on Windows: skipped.";
  return 0;
#else
  const QString cmake = argc > 1 ? QString(argv[1]) : QString("cmake");
  QTemporaryDir tmp;
  check(tmp.isValid(), "temporary folder");
  // git without the user's configuration (no signing key, no hooks)
  write(tmp.filePath("gitconfig"), "");
  qputenv("GIT_CONFIG_GLOBAL", tmp.filePath("gitconfig").toUtf8());
  qputenv("GIT_CONFIG_NOSYSTEM", "1");

  const QString origin = tmp.filePath("origin"), src = tmp.filePath("src"), build = tmp.filePath("build");
  QDir().mkpath(origin);
  QDir().mkpath(build);
  git(origin, { "init", "-q", "-b", "master" });
  write(origin + "/a.txt", "one\n");
  git(origin, { "add", "a.txt" });
  git(origin, { "commit", "-q", "-m", "one" });
  git(tmp.path(), { "clone", "-q", origin, src });
  write(build + "/CMakeCache.txt", "");
  write(origin + "/a.txt", "two\n");
  git(origin, { "commit", "-q", "-am", "two" });

  // where it is offered, how the program is replaced
  check(SourceBuild::available(src, build) && !SourceBuild::available(src, tmp.filePath("none"))
          && !SourceBuild::available(tmp.filePath("none"), build) && !SourceBuild::available({}, {}),
        "available: a checkout and a configured build directory");
  using I = SourceBuild::Install;
  check(SourceBuild::installMode(build + "/qtdmm", src, build, false) == I::BuildDir, "in the build directory");
#ifndef Q_OS_MACOS
  check(SourceBuild::installMode(src + "/bin/qtdmm", src, build, false) == I::BinCopy, "compile.sh's bin/");
  check(SourceBuild::installMode("/usr/bin/qtdmm", src, build, false) == I::CMakeInstall
          && SourceBuild::installMode("/usr/bin/qtdmm", src, build, true) == I::Deb,
        "installed: cmake --install or a .deb");
#endif

  SourceBuild b;
  b.setExecutable(build + "/qtdmm");
  b.setDirs(src, build);
  b.setConfigureCommand({ cmake, "-E", "touch", build + "/configured" });
  b.setBuildCommand({ cmake, "-E", "touch", build + "/built" });
  b.setInstallMode(I::BuildDir);

  // changes not committed: nothing pulled, nothing built
  write(src + "/a.txt", "mine\n");
  Result r = run(b);
  check(!r.ok && r.message.contains("not committed") && !QFile::exists(build + "/configured")
          && !QFile::exists(build + "/built")
          && read(src + "/a.txt") == "mine\n",
        "changes not committed: stops at once: " + r.message);
  git(src, { "checkout", "-q", "a.txt" });

  // pulled and built, started from the build directory
  r = run(b);
  check(r.ok && git(src, { "rev-parse", "HEAD" }) == git(origin, { "rev-parse", "HEAD" })
          && QFile::exists(build + "/configured") && QFile::exists(build + "/built")
          && b.restartPath() == build + "/qtdmm" && r.steps.size() == 5,
        "pulled and built: " + r.message + " / " + r.steps.join(", "));

  // compile.sh's copy in bin/: copied again, executable as before
  write(build + "/qtdmm", "new");
  write(build + "/qtdmm_de.qm", "de");
  QDir().mkpath(src + "/bin");
  write(src + "/bin/qtdmm", "old");
  QFile::setPermissions(src + "/bin/qtdmm", QFile::permissions(src + "/bin/qtdmm") | QFile::ExeOwner);
  b.setExecutable(src + "/bin/qtdmm");
  b.setInstallMode(I::BinCopy);
  r = run(b);
  check(r.ok && read(src + "/bin/qtdmm") == "new" && read(src + "/bin/qtdmm_de.qm") == "de"
          && (QFile::permissions(src + "/bin/qtdmm") & QFile::ExeOwner) && b.restartPath() == src + "/bin/qtdmm",
        "copied to bin/: " + r.message);

  // installed with cmake --install, no pkexec: built, the command to run said
  b.setInstallMode(I::CMakeInstall);
  b.setElevator({});
  r = run(b);
  check(r.ok && r.message.contains("--install " + build) && b.restartPath().isEmpty(),
        "no pkexec: built, how to install: " + r.message);

  // a failed build, Cancel
  b.setInstallMode(I::BuildDir);
  b.setBuildCommand({ cmake, "-E", "false" });
  r = run(b);
  check(!r.ok && r.message.contains("Building") && b.restartPath().isEmpty(), "a failed build: " + r.message);
  b.setBuildCommand({ cmake, "-E", "sleep", "20" });
  r = run(b, [&b] {
    waitFor([] { return false; }, 1500);
    b.cancel();
  });
  check(!r.ok && r.message.contains("Cancelled") && !b.running(), "Cancel: " + r.message);

  // a branch without upstream
  git(src, { "switch", "-q", "-c", "lonely" });
  b.setBuildCommand({ cmake, "-E", "touch", build + "/built" });
  r = run(b);
  check(!r.ok && r.message.contains("upstream"), "no upstream: " + r.message);

  if (failed == 0)
    qInfo() << "All source build tests passed.";
  return failed == 0 ? 0 : 1;
#endif
}
