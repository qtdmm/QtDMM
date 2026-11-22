// Copyright (c) 2026 The QtDMM developers
// SPDX-License-Identifier: GPL-3.0-or-later
#include "ui/updatebar.h"

#include <QCoreApplication>
#include <QDesktopServices>
#include <QDir>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QTimer>
#include <QToolButton>

#include "core/flatpakupdate.h"
#include "core/settings.h"
#include "core/updatedownload.h"

UpdateBar::UpdateBar(Settings *settings, QWidget *parent)
  : QWidget(parent), m_settings(settings), m_check(new UpdateCheck(this)), m_download(new UpdateDownload(this)),
    m_flatpak(new FlatpakUpdate(this)),
    m_kind(UpdateCheck::installKind())
{
  setAutoFillBackground(true);
  setBackgroundRole(QPalette::AlternateBase);
  auto *layout = new QHBoxLayout(this);
  layout->setContentsMargins(10, 4, 6, 4);
  m_text = new QLabel(this);
  m_text->setWordWrap(true);
  layout->addWidget(m_text, 1);
  m_buttons = new QHBoxLayout;
  layout->addLayout(m_buttons);
  auto *close = new QToolButton(this);
  close->setIcon(QIcon::fromTheme("window-close"));
  close->setAutoRaise(true);
  close->setToolTip(tr("Close"));
  connect(close, &QToolButton::clicked, this, &QWidget::hide);
  layout->addWidget(close);
  hide();

  connect(m_check, &UpdateCheck::checked, this, &UpdateBar::checked);
  connect(m_check, &UpdateCheck::failed, this, [this](const QString &error) {
    qInfo("update check: %s", qPrintable(error));
    if (m_manual)
      showMessage(tr("Could not look for a new version: %1").arg(error), {});
  });
}

bool UpdateBar::beta() const
{
  return m_settings->getBool("Update/beta") || UpdateCheck::isPreRelease(QCoreApplication::applicationVersion());
}

void UpdateBar::startupCheck()
{
  // what the last update on macOS left of the version before
  if (m_kind == UpdateCheck::Install::MacBundle)
    UpdateCheck::removeOldBundle(UpdateCheck::bundlePath());
  if (!UpdateCheck::enabled())
    return;
  if (m_settings->getString("Update/check").isEmpty())
  {
    ask();
    return;
  }
  const QString today = QDate::currentDate().toString(Qt::ISODate);
  if (!m_settings->getBool("Update/check") || m_settings->getString("Update/last") == today)
    return;
  // saved at once: the reads see the file only
  m_settings->setString("Update/last", today);
  m_settings->save();
  m_manual = false;
  m_check->check();
}

void UpdateBar::checkNow()
{
  m_manual = true;
  m_check->check();
}

void UpdateBar::ask()
{
  auto answer = [this](bool yes) {
    m_settings->setBool("Update/check", yes);
    m_settings->save();
    hide();
    if (yes)
      startupCheck();
  };
  showMessage(tr("Should QtDMM look for a new version when it starts? Once a day it fetches the list of "
                 "releases from qtdmm.github.io; nothing else is sent. Settings, General can change it."),
              { { tr("Yes"), [answer] { answer(true); } }, { tr("No"), [answer] { answer(false); } } });
}

void UpdateBar::checked(const QByteArray &feed)
{
  const QString current = QCoreApplication::applicationVersion();
  const auto release =
    UpdateCheck::offer(feed, current, beta(), m_manual ? QString() : m_settings->getString("Update/skip"));
  if (release)
    offer(*release);
  else if (m_manual)
    showMessage(tr("QtDMM %1 is the newest version.").arg(current), {});
}

void UpdateBar::offer(const UpdateRelease &release)
{
  const UpdateCheck::Install kind = m_kind;
  QString text = tr("QtDMM %1 is out (this is %2).").arg(release.version, QCoreApplication::applicationVersion());
  QList<Button> buttons;
  const UpdateFile file = release.files.value(UpdateCheck::fileKind(kind));
  if (kind == UpdateCheck::Install::Flatpak && FlatpakUpdate::supported())
    buttons.append({ tr("Install"), [this, release] { updateFlatpak(release); } });
  else if (kind == UpdateCheck::Install::Flatpak)
    text += ' ' + tr("Your software centre or \"flatpak update\" installs it.");
  else if ((kind == UpdateCheck::Install::WindowsInstaller || kind == UpdateCheck::Install::AppImage
            || kind == UpdateCheck::Install::MacBundle)
           && !file.url.isEmpty())
    buttons.append({ tr("Install"), [this, release, file] { download(release, file); } });
  else if (!file.url.isEmpty())
    buttons.append({ tr("Download"), [url = file.url] { QDesktopServices::openUrl(url); } });
  buttons.append({ tr("What's new"), [url = release.notes] { QDesktopServices::openUrl(url); } });
  buttons.append({ tr("Skip this version"), [this, version = release.version] {
                    m_settings->setString("Update/skip", version);
                    m_settings->save();
                    hide();
                  } });
  showMessage(text, buttons);
}

void UpdateBar::download(const UpdateRelease &release, const UpdateFile &file)
{
  const QString version = release.version;
  disconnect(m_download, nullptr, this, nullptr);
  connect(m_download, &UpdateDownload::progress, this, [this, version](qint64 received, qint64 total) {
    const int percent = total > 0 ? int(received * 100 / total) : 0;
    m_text->setText(tr("Downloading QtDMM %1 ... %2 %").arg(version).arg(percent));
  });
  connect(m_download, &UpdateDownload::finished, this,
          [this, release, file](const QString &path) { downloaded(release, file, path); });
  connect(m_download, &UpdateDownload::failed, this, [this, release, file](const QString &error) {
    showMessage(tr("The download failed: %1").arg(error),
                { { tr("Try again"), [this, release, file] { download(release, file); } },
                  { tr("What's new"), [url = release.notes] { QDesktopServices::openUrl(url); } } });
  });
  showMessage(tr("Downloading QtDMM %1 ... %2 %").arg(version).arg(0),
              { { tr("Cancel"), [this] { m_download->cancel(); hide(); } } });
  m_download->start(file);
}

void UpdateBar::setFlatpakUpdate(FlatpakUpdate *update)
{
  delete m_flatpak;
  m_flatpak = update;
  m_flatpak->setParent(this);
}

void UpdateBar::updateFlatpak(const UpdateRelease &release)
{
  const QString version = release.version;
  disconnect(m_flatpak, nullptr, this, nullptr);
  connect(m_flatpak, &FlatpakUpdate::progress, this, [this, version](int percent) {
    m_text->setText(tr("Updating QtDMM %1 ... %2 %").arg(version).arg(percent));
  });
  connect(m_flatpak, &FlatpakUpdate::done, this, [this, version] {
    showMessage(tr("QtDMM %1 is installed; it starts with the next start of QtDMM.").arg(version),
                { { tr("Restart now"), [this] { hide(); Q_EMIT restartReady(QString()); } },
                  { tr("Later"), [this] { hide(); } } });
  });
  connect(m_flatpak, &FlatpakUpdate::upToDate, this, [this, version] {
    showMessage(tr("QtDMM %1 is not in the Flatpak repository yet; try again later.").arg(version), {});
  });
  connect(m_flatpak, &FlatpakUpdate::failed, this, [this, release](const QString &error) {
    showMessage(tr("The update failed: %1. Your software centre or \"flatpak update\" can install it.").arg(error),
                { { tr("Try again"), [this, release] { updateFlatpak(release); } } });
  });
  showMessage(tr("Updating QtDMM %1 ... %2 %").arg(version).arg(0), {});
  m_flatpak->update();
}

void UpdateBar::downloaded(const UpdateRelease &release, const UpdateFile &file, const QString &path)
{
  if (m_kind == UpdateCheck::Install::MacBundle)
  {
    installDmg(release, path);
    return;
  }
  if (m_kind != UpdateCheck::Install::AppImage)
  {
    showMessage(tr("QtDMM %1 is downloaded and checked. QtDMM closes for the installation and starts "
                   "again when it is done.").arg(release.version),
                { { tr("Install now"), [this, path] { hide(); Q_EMIT installReady(path); } },
                  { tr("Later"), [this] { hide(); } } });
    return;
  }
  // the AppImage: in place at once, the running one goes on with the old file
  const QString target = qEnvironmentVariable("APPIMAGE");
  const QString error = UpdateCheck::replaceAppImage(path, target);
  if (!error.isEmpty())
  {
    showMessage(tr("QtDMM %1 could not replace %2: %3").arg(release.version, QDir::toNativeSeparators(target), error),
                { { tr("Download"), [url = file.url] { QDesktopServices::openUrl(url); } } });
    return;
  }
  showMessage(tr("QtDMM %1 is installed; it starts with the next start of QtDMM.").arg(release.version),
              { { tr("Restart now"), [this, target] { hide(); Q_EMIT restartReady(target); } },
                { tr("Later"), [this] { hide(); } } });
}

void UpdateBar::installDmg(const UpdateRelease &release, const QString &dmg)
{
  // the bundle in place while QtDMM runs; a few seconds, the banner says so first
  showMessage(tr("Installing QtDMM %1 ...").arg(release.version), {});
  QTimer::singleShot(50, this, [this, release, dmg] {
    const QString target = UpdateCheck::bundlePath();
    const QString error = UpdateCheck::installDmg(dmg, target);
    if (!error.isEmpty())
    {
      showMessage(tr("QtDMM %1 could not replace %2: %3").arg(release.version, QDir::toNativeSeparators(target), error),
                  { { tr("Open disk image"), [dmg] { QDesktopServices::openUrl(QUrl::fromLocalFile(dmg)); } } });
      return;
    }
    showMessage(tr("QtDMM %1 is installed; it starts with the next start of QtDMM.").arg(release.version),
                { { tr("Restart now"), [this, target] { hide(); Q_EMIT restartReady(target); } },
                  { tr("Later"), [this] { hide(); } } });
  });
}

void UpdateBar::showMessage(const QString &text, const QList<Button> &buttons)
{
  // later: the button clicked may be one of them
  while (QLayoutItem *item = m_buttons->takeAt(0))
  {
    item->widget()->hide();
    item->widget()->deleteLater();
    delete item;
  }
  for (const Button &b : buttons)
  {
    auto *button = new QPushButton(b.text, this);
    connect(button, &QPushButton::clicked, this, b.action);
    m_buttons->addWidget(button);
    button->show();   // at once, also in a banner already shown
  }
  m_text->setText(text);
  show();
}
