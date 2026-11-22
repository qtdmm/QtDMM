// Copyright (c) 2026 The QtDMM developers
// SPDX-License-Identifier: GPL-3.0-or-later
// The update check: the version order, what latest.json offers to which
// version (tests/data/update/latest.json, written by tools/make-update-feed.py
// from real releases: stable 26.4, beta 27.1-rc1), fetching it, and the
// banner asking first and offering then; the Windows installer downloaded
// and checked against its SHA256.
#include "core/settings.h"
#include "core/updatecheck.h"
#include "core/flatpakupdate.h"
#include "core/updatedownload.h"
#include "ui/updatebar.h"

#include "testutil.h"

#include <QApplication>
#include <QCryptographicHash>
#include <QDir>
#include <QJsonDocument>
#include <QJsonObject>
#include <QStandardPaths>
#include <QFile>
#include <QLabel>
#include <QPushButton>
#include <QTemporaryDir>

namespace
{
/// The portal's part of a Flatpak update, played: some progress, then done
/// or nothing to pull.
class FakeFlatpak : public FlatpakUpdate
{
public:
  bool newer = true;
  void update() override
  {
    Q_EMIT progress(40);
    if (newer)
      Q_EMIT done();
    else
      Q_EMIT upToDate();
  }
};

void click(const UpdateBar &bar, const QString &text)
{
  for (QPushButton *b : bar.findChildren<QPushButton *>())
    if (!b->isHidden() && b->text() == text)
    {
      b->click();
      return;
    }
  check(false, "no button " + text);
}

QStringList buttons(const UpdateBar &bar)
{
  QStringList texts;
  for (const QPushButton *b : bar.findChildren<QPushButton *>())
    if (!b->isHidden())
      texts << b->text();
  return texts;
}
}   // namespace

int main(int argc, char **argv)
{
  qputenv("QT_QPA_PLATFORM", "offscreen");
  QApplication app(argc, argv);
  const QString data = QString(argv[1]);

  // the order of versions
  auto less = [](const char *a, const char *b) { return UpdateCheck::compareVersions(a, b) < 0; };
  check(less("26.4-dev.14+g1234abc", "26.4-alpha1") && less("26.4-alpha1", "26.4-beta1")
          && less("26.4-beta1", "26.4-rc1") && less("26.4-rc1", "26.4-rc1.3+g1234abc")
          && less("26.4-rc1.3+g1234abc", "26.4-rc2") && less("26.4-rc2", "26.4") && less("26.4", "26.4.1")
          && less("26.4.1", "26.5-dev.1+gabc") && less("26.9", "26.10") && less("26.10", "27.1-rc1"),
        "dev < alpha < beta < rc < release < patch < next");
  check(UpdateCheck::compareVersions("26.4", "26.4") == 0, "equal");
  check(less("0.9.6-git", "26.1") && less("", "0.1"), "no version: before all");
  check(UpdateCheck::isPreRelease("27.1-rc1") && UpdateCheck::isPreRelease("27.1-dev.4+gabc")
          && !UpdateCheck::isPreRelease("26.4") && !UpdateCheck::isPreRelease("26.4.1"),
        "pre-releases");

  // what the feed offers
  QFile file(data + "/latest.json");
  check(file.open(QIODevice::ReadOnly), "latest.json");
  const QByteArray feed = file.readAll();
  auto offered = [&](const QString &current, bool beta, const QString &skipped = {}) {
    const auto r = UpdateCheck::offer(feed, current, beta, skipped);
    return r ? r->version : QString("-");
  };
  check(offered("26.3", false) == "26.4", "26.3: the release");
  check(offered("26.3", true) == "27.1-rc1", "26.3 with beta: the newer candidate");
  check(offered("26.4", false) == "-", "26.4: nothing");
  check(offered("26.4-rc1", true) == "27.1-rc1", "26.4-rc1: the next candidate");
  check(offered("27.1-rc1", true) == "-", "the candidate itself: nothing");
  check(offered("27.1-dev.3+gabc", true) == "27.1-rc1", "a dev build: the candidate");
  check(offered("26.3", false, "26.4") == "-", "a skipped version is not offered");
  check(offered("26.3", true, "27.1-rc1") == "26.4", "a skipped candidate: the release");
  check(!UpdateCheck::offer("{}", "26.3", true) && !UpdateCheck::offer("no json", "26.3", true),
        "no feed: nothing");
  const auto r = UpdateCheck::offer(feed, "26.3", false);
  const UpdateFile appimage = r ? r->files.value("appimage") : UpdateFile();
  check(r && r->date == QDate(2026, 11, 22) && r->notes.toString().endsWith("/releases/tag/26.4"),
        "date and release page");
  check(appimage.url.toString().endsWith("QtDMM-26.4-x86_64.AppImage") && appimage.sha256.size() == 64
          && appimage.size > 1000000,
        "the AppImage: url, SHA256, size");
  check(UpdateCheck::fileKind(UpdateCheck::Install::WindowsInstaller) == "windows-installer"
          && UpdateCheck::fileKind(UpdateCheck::Install::Source).isEmpty(),
        "file kinds");
  qputenv("APPIMAGE", "QtDMM.AppImage");   // only looked at, never opened
  check(UpdateCheck::installKind() == UpdateCheck::Install::AppImage, "$APPIMAGE: an AppImage");
  qunsetenv("APPIMAGE");

  // fetching: the feed from a file
  qputenv("QTDMM_UPDATE_URL", QUrl::fromLocalFile(data + "/latest.json").toString().toUtf8());
  UpdateCheck fetch;
  QByteArray fetched;
  QObject::connect(&fetch, &UpdateCheck::checked, [&](const QByteArray &f) { fetched = f; });
  fetch.check();
  check(waitFor([&] { return !fetched.isEmpty(); }) && fetched == feed, "fetched from QTDMM_UPDATE_URL");

  // the banner: asks first, No leaves it at that
  QTemporaryDir cfg;
  Settings settings("update", cfg.path());
  QCoreApplication::setApplicationVersion("26.3");
  UpdateBar bar(&settings);
  bar.startupCheck();
  check(!bar.isHidden() && buttons(bar) == QStringList({ "Yes", "No" }), "first start: asks");
  bar.findChildren<QPushButton *>().last()->click();
  check(bar.isHidden() && settings.getString("Update/check") == "false", "No: not looked");
  bar.startupCheck();
  check(bar.isHidden(), "asked once");
  // Yes: looks at once, offers the release
  settings.remove("Update/check");
  bar.startupCheck();
  bar.findChildren<QPushButton *>().first()->click();
  const bool shown = waitFor([&] { return buttons(bar).contains("Skip this version"); });
  const QString text = bar.findChild<QLabel *>()->text();
  check(shown && text.contains("26.4") && settings.getString("Update/last") == QDate::currentDate().toString(Qt::ISODate),
        "Yes: 26.4 offered: " + text);
  check(!buttons(bar).contains("Download") || UpdateCheck::installKind() != UpdateCheck::Install::Source,
        "a source build: no download");
  // skipped: not at start, but when asked for
  for (QPushButton *b : bar.findChildren<QPushButton *>())
    if (!b->isHidden() && b->text() == "Skip this version")
      b->click();
  check(bar.isHidden() && settings.getString("Update/skip") == "26.4", "skipped");
  settings.setString("Update/last", "2000-01-01");
  settings.save();
  bar.startupCheck();
  waitFor([] { return false; }, 300);
  check(bar.isHidden(), "the skipped version: no banner at start");
  bar.checkNow();
  check(waitFor([&] { return !bar.isHidden(); }) && bar.findChild<QLabel *>()->text().contains("26.4"),
        "Check for updates: offered all the same");
  // up to date, and a rc gets candidates
  QCoreApplication::setApplicationVersion("27.1-rc1");
  check(bar.beta(), "a candidate gets candidates");
  bar.checkNow();
  check(waitFor([&] { return bar.findChild<QLabel *>()->text().contains("newest"); }), "27.1-rc1: the newest");

  // the Windows installer: Install downloads it into the cache, checks it
  // and hands it on; a wrong SHA256 leaves nothing behind
  QStandardPaths::setTestModeEnabled(true);
  QTemporaryDir web;
  const QByteArray setup = QByteArray("MZ fake installer ").repeated(5000);
  QFile out(web.filePath("QtDMM-26.5-windows-x64.exe"));
  check(out.open(QIODevice::WriteOnly) && out.write(setup) == setup.size(), "the installer on the 'server'");
  out.close();
  auto feedWith = [&](const QString &sha) {
    const QJsonObject file{ { "url", QUrl::fromLocalFile(out.fileName()).toString() }, { "sha256", sha },
                            { "size", double(setup.size()) } };
    const QJsonObject stable{ { "version", "26.5" }, { "date", "2027-01-01" }, { "notes", "https://example.invalid/" },
                              { "files", QJsonObject{ { "windows-installer", file }, { "appimage", file } } } };
    QFile f(web.filePath("latest.json"));
    check(f.open(QIODevice::WriteOnly | QIODevice::Truncate), "the feed on the 'server'");
    f.write(QJsonDocument(QJsonObject{ { "format", 1 }, { "stable", stable }, { "beta", QJsonValue() } }).toJson());
  };
  const QString sha = QString::fromLatin1(QCryptographicHash::hash(setup, QCryptographicHash::Sha256).toHex());
  feedWith(sha);
  qputenv("QTDMM_UPDATE_URL", QUrl::fromLocalFile(web.filePath("latest.json")).toString().toUtf8());
  QCoreApplication::setApplicationVersion("26.4");
  bar.setInstallKind(UpdateCheck::Install::WindowsInstaller);
  QString ready;
  QObject::connect(&bar, &UpdateBar::installReady, [&](const QString &path) { ready = path; });
  bar.checkNow();
  check(waitFor([&] { return buttons(bar).contains("Install"); }) && !buttons(bar).contains("Download"),
        "the installer: Install, no Download");
  click(bar, "Install");
  check(waitFor([&] { return buttons(bar).contains("Install now"); }), "downloaded and checked: " + bar.findChild<QLabel *>()->text());
  click(bar, "Install now");
  QFile got(ready);
  check(bar.isHidden() && QFileInfo(ready).fileName() == "QtDMM-26.5-windows-x64.exe"
          && ready.startsWith(UpdateDownload::defaultFolder()) && got.open(QIODevice::ReadOnly) && got.readAll() == setup,
        "Install now: the checked file handed on: " + ready);
  got.close();
  feedWith(QString(64, 'a'));
  bar.checkNow();
  waitFor([&] { return buttons(bar).contains("Install"); });
  click(bar, "Install");
  check(waitFor([&] { return buttons(bar).contains("Try again"); })
          && bar.findChild<QLabel *>()->text().contains("SHA256")
          && QDir(UpdateDownload::defaultFolder()).entryList(QDir::Files).isEmpty(),
        "a wrong SHA256: failed, nothing left: " + bar.findChild<QLabel *>()->text());
  check(UpdateCheck::installerArguments().contains("/QTDMMRESTART=1"), "the installer starts QtDMM again");

  QString restart;
  QObject::connect(&bar, &UpdateBar::restartReady, [&](const QString &path) { restart = path; });
#ifndef Q_OS_WIN
  // the AppImage: downloaded, checked and put in place of $APPIMAGE at once,
  // executable as before, nothing left beside it; Restart now hands it on.
  // Unix only: execute bits and a read-only folder, no AppImage elsewhere
  feedWith(sha);
  QTemporaryDir apps;
  const QString appImage = apps.filePath("QtDMM-26.4-x86_64.AppImage");
  QFile old(appImage);
  check(old.open(QIODevice::WriteOnly) && old.write("old") == 3, "the running AppImage");
  old.close();
  old.setPermissions(QFile::ReadOwner | QFile::WriteOwner | QFile::ExeOwner | QFile::ReadGroup | QFile::ExeGroup);
  qputenv("APPIMAGE", appImage.toUtf8());
  bar.setInstallKind(UpdateCheck::Install::AppImage);
  bar.checkNow();
  check(waitFor([&] { return buttons(bar).contains("Install"); }), "the AppImage: Install");
  click(bar, "Install");
  check(waitFor([&] { return buttons(bar).contains("Restart now"); }), "replaced: " + bar.findChild<QLabel *>()->text());
  QFile replaced(appImage);
  check(replaced.open(QIODevice::ReadOnly) && replaced.readAll() == setup
          && (replaced.permissions() & QFile::ExeOwner) && (replaced.permissions() & QFile::ExeGroup),
        "the new AppImage in place, executable as the old one");
  replaced.close();
  check(QDir(apps.path()).entryList(QDir::Files | QDir::Hidden) == QStringList{ "QtDMM-26.4-x86_64.AppImage" },
        "nothing beside it: " + QDir(apps.path()).entryList(QDir::Files | QDir::Hidden).join(", "));
  click(bar, "Restart now");
  check(bar.isHidden() && restart == appImage, "Restart now: the same path");
  // a folder QtDMM may not write into: says so, the browser download instead
  // (not as root, who writes there all the same: the FreeBSD CI)
  QFile::setPermissions(apps.path(), QFile::ReadOwner | QFile::ExeOwner);
  if (!QFileInfo(apps.path()).isWritable())
  {
    bar.checkNow();
    waitFor([&] { return buttons(bar).contains("Install"); });
    click(bar, "Install");
    check(waitFor([&] { return buttons(bar).contains("Download"); })
            && bar.findChild<QLabel *>()->text().contains("could not replace"),
          "read-only folder: " + bar.findChild<QLabel *>()->text());
  }
  QFile::setPermissions(apps.path(), QFile::ReadOwner | QFile::WriteOwner | QFile::ExeOwner);
  qunsetenv("APPIMAGE");
#endif

  // macOS: the new bundle takes the old one's name, the old one waits beside
  // it until the next start; a failed swap leaves the old one in place
  QTemporaryDir bundles;
  auto writeFile = [](const QString &path, const QByteArray &data) {
    QDir().mkpath(QFileInfo(path).path());
    QFile f(path);
    return f.open(QIODevice::WriteOnly) && f.write(data) == data.size();
  };
  auto readFile = [](const QString &path) {
    QFile f(path);
    return f.open(QIODevice::ReadOnly) ? f.readAll() : QByteArray();
  };
  const QString bundle = bundles.filePath("QtDMM.app");
  check(writeFile(bundle + "/Contents/MacOS/qtdmm", "old")
          && writeFile(bundles.filePath(".QtDMM.app.update/Contents/MacOS/qtdmm"), "new"),
        "two bundles");
  check(UpdateCheck::replaceBundle(bundles.filePath(".QtDMM.app.update"), bundle).isEmpty()
          && readFile(bundle + "/Contents/MacOS/qtdmm") == "new"
          && readFile(bundles.filePath(".QtDMM.app.old/Contents/MacOS/qtdmm")) == "old",
        "the new bundle in place, the old one beside it");
  UpdateCheck::removeOldBundle(bundle);
  check(QDir(bundles.path()).entryList(QDir::Dirs | QDir::NoDotAndDotDot | QDir::Hidden) == QStringList{ "QtDMM.app" },
        "the old one gone at the next start");
  check(!UpdateCheck::replaceBundle(bundles.filePath(".missing"), bundle).isEmpty()
          && readFile(bundle + "/Contents/MacOS/qtdmm") == "new",
        "a failed swap: the bundle as it was");
  check(UpdateCheck::bundlePath().isEmpty(), "the test is no bundle");
  // the banner: downloaded and checked, a disk image that cannot be put in
  // place (no bundle here, not mountable) opens in the Finder instead
  bar.setInstallKind(UpdateCheck::Install::MacBundle);
  feedWith(sha);
  {
    QJsonObject root = QJsonDocument::fromJson(readFile(web.filePath("latest.json"))).object();
    QJsonObject stable = root["stable"].toObject();
    QJsonObject files = stable["files"].toObject();
    files["macos"] = files["appimage"];
    stable["files"] = files;
    root["stable"] = stable;
    check(writeFile(web.filePath("latest.json"), QJsonDocument(root).toJson()), "the feed with a disk image");
  }
  bar.checkNow();
  check(waitFor([&] { return buttons(bar).contains("Install"); }), "macOS: Install");
  click(bar, "Install");
  check(waitFor([&] { return buttons(bar).contains("Open disk image"); }, 5000),
        "not in place: the disk image to open: " + bar.findChild<QLabel *>()->text());

  // the Flatpak: Install has the portal pull it, Restart now with no path
  // (FlatpakUpdate::restart() starts the latest version)
  if (FlatpakUpdate::supported())
  {
    auto *flatpak = new FakeFlatpak;
    bar.setFlatpakUpdate(flatpak);
    bar.setInstallKind(UpdateCheck::Install::Flatpak);
    restart = "-";
    bar.checkNow();
    check(waitFor([&] { return buttons(bar).contains("Install"); }), "the Flatpak: Install");
    click(bar, "Install");
    check(buttons(bar).contains("Restart now"), "updated: " + bar.findChild<QLabel *>()->text());
    click(bar, "Restart now");
    check(restart.isEmpty(), "Restart now: no path, the Flatpak's latest version");
    flatpak->newer = false;
    bar.checkNow();
    waitFor([&] { return buttons(bar).contains("Install"); });
    click(bar, "Install");
    check(bar.findChild<QLabel *>()->text().contains("not in the Flatpak repository yet"),
          "nothing to pull: " + bar.findChild<QLabel *>()->text());
  }

  if (failed == 0)
    qInfo() << "All update tests passed.";
  return failed == 0 ? 0 : 1;
}
