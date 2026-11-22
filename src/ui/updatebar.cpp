// Copyright (c) 2026 The QtDMM developers
// SPDX-License-Identifier: GPL-3.0-or-later
#include "ui/updatebar.h"

#include <QCoreApplication>
#include <QDesktopServices>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QToolButton>

#include "core/settings.h"
#include "core/updatedownload.h"

UpdateBar::UpdateBar(Settings *settings, QWidget *parent)
  : QWidget(parent), m_settings(settings), m_check(new UpdateCheck(this)), m_download(new UpdateDownload(this)),
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
  if (kind == UpdateCheck::Install::Flatpak)
    text += ' ' + tr("Your software centre or \"flatpak update\" installs it.");
  else if (kind == UpdateCheck::Install::WindowsInstaller && !file.url.isEmpty())
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
  connect(m_download, &UpdateDownload::finished, this, [this, version](const QString &path) {
    showMessage(tr("QtDMM %1 is downloaded and checked. QtDMM closes for the installation and starts "
                   "again when it is done.").arg(version),
                { { tr("Install now"), [this, path] { hide(); Q_EMIT installReady(path); } },
                  { tr("Later"), [this] { hide(); } } });
  });
  connect(m_download, &UpdateDownload::failed, this, [this, release, file](const QString &error) {
    showMessage(tr("The download failed: %1").arg(error),
                { { tr("Try again"), [this, release, file] { download(release, file); } },
                  { tr("What's new"), [url = release.notes] { QDesktopServices::openUrl(url); } } });
  });
  showMessage(tr("Downloading QtDMM %1 ... %2 %").arg(version).arg(0),
              { { tr("Cancel"), [this] { m_download->cancel(); hide(); } } });
  m_download->start(file);
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
  }
  m_text->setText(text);
  show();
}
