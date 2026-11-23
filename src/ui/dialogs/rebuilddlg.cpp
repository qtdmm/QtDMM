// Copyright (c) 2026 The QtDMM developers
// SPDX-License-Identifier: GPL-3.0-or-later
#include "ui/dialogs/rebuilddlg.h"

#include "core/sourcebuild.h"

#include <QDialogButtonBox>
#include <QDir>
#include <QFontDatabase>
#include <QLabel>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QVBoxLayout>

namespace
{
QString installText(const SourceBuild &b)
{
  switch (b.mode())
  {
  case SourceBuild::Install::BuildDir:
    return RebuildDlg::tr("QtDMM runs from the build directory: Restart now starts the new build.");
  case SourceBuild::Install::BinCopy:
    return RebuildDlg::tr("Then the program is copied to %1 as compile.sh does.")
      .arg(QDir::toNativeSeparators(b.sourceDir() + "/bin"));
  case SourceBuild::Install::CMakeInstall:
    return RebuildDlg::tr("Then it is installed with \"cmake --install\" as before; that asks for the "
                          "administrator password.");
  case SourceBuild::Install::Deb:
    return RebuildDlg::tr("QtDMM is installed as a .deb package: a new package is made and installed with apt; "
                          "that asks for the administrator password.");
  case SourceBuild::Install::None:
    break;
  }
  return RebuildDlg::tr("This program is not in the build directory: it is built only.");
}
}   // namespace

RebuildDlg::RebuildDlg(SourceBuild *build, QWidget *parent) : QDialog(parent), m_build(build)
{
  m_build->setParent(this);
  setWindowTitle(tr("Update and build"));
  auto *layout = new QVBoxLayout(this);
  m_intro = new QLabel(tr("Fetches the new commits of the branch into %1 (git pull, fast-forward only), "
                          "then builds in %2 with its configuration.")
                           .arg(QDir::toNativeSeparators(build->sourceDir()),
                                QDir::toNativeSeparators(build->buildDir()))
                         + ' ' + installText(*build) + ' '
                         + tr("Nothing happens while the sources have changes not committed."),
                       this);
  m_intro->setWordWrap(true);
  m_intro->setTextInteractionFlags(Qt::TextSelectableByMouse);
  layout->addWidget(m_intro);
  m_status = new QLabel(this);
  m_status->setWordWrap(true);
  m_status->setTextInteractionFlags(Qt::TextSelectableByMouse);
  layout->addWidget(m_status);
  m_log = new QPlainTextEdit(this);
  m_log->setReadOnly(true);
  m_log->setMaximumBlockCount(10000);
  m_log->setFont(QFontDatabase::systemFont(QFontDatabase::FixedFont));
  m_log->setMinimumSize(640, 280);
  layout->addWidget(m_log, 1);

  auto *buttons = new QDialogButtonBox(this);
  m_start = buttons->addButton(tr("Start"), QDialogButtonBox::AcceptRole);
  m_restart = buttons->addButton(tr("Restart now"), QDialogButtonBox::ActionRole);
  m_close = buttons->addButton(QDialogButtonBox::Cancel);
  m_restart->hide();
  m_start->setDefault(true);
  layout->addWidget(buttons);
  connect(m_start, &QPushButton::clicked, this, &RebuildDlg::start);
  connect(m_close, &QPushButton::clicked, this, &RebuildDlg::reject);
  connect(m_restart, &QPushButton::clicked, this, [this] {
    accept();
    Q_EMIT restartRequested(m_build->restartPath());
  });

  connect(m_build, &SourceBuild::step, this, [this](const QString &what) {
    m_status->setText(what + QStringLiteral(" …"));
    m_log->appendPlainText(QStringLiteral("== ") + what);
  });
  connect(m_build, &SourceBuild::output, this, [this](const QString &text) {
    m_log->moveCursor(QTextCursor::End);
    m_log->insertPlainText(text);
    m_log->ensureCursorVisible();
  });
  connect(m_build, &SourceBuild::finished, this, [this](bool ok, const QString &message) {
    m_status->setText(message);
    m_log->appendPlainText(QStringLiteral("== ") + message);
    m_close->setText(tr("Close"));
    m_start->setText(tr("Try again"));
    m_start->setVisible(!ok);
    m_restart->setVisible(ok && !m_build->restartPath().isEmpty());
    if (m_restart->isVisible())
      m_restart->setDefault(true);
  });
}

QString RebuildDlg::intro() const
{
  return m_intro->text();
}

QString RebuildDlg::status() const
{
  return m_status->text();
}

QString RebuildDlg::log() const
{
  return m_log->toPlainText();
}

void RebuildDlg::start()
{
  m_start->hide();
  m_restart->hide();
  m_close->setText(tr("Cancel"));
  m_build->start();
}

void RebuildDlg::reject()
{
  if (m_build->running())
  {
    m_build->cancel();
    return;   // stays open: the log says why it stopped
  }
  QDialog::reject();
}
