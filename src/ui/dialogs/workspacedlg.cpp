// Copyright (c) 2026 The QtDMM developers
// SPDX-License-Identifier: GPL-3.0-or-later
#include "ui/dialogs/workspacedlg.h"

#include <QDialogButtonBox>
#include <QFormLayout>
#include <QLabel>
#include <QLineEdit>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QVBoxLayout>

WorkspaceDlg::WorkspaceDlg(const QString &name, const QString &description, const QPixmap &picture, QWidget *parent)
  : QDialog(parent)
{
  setWindowTitle(tr("Save bench"));
  auto *layout = new QVBoxLayout(this);
  auto *form = new QFormLayout;
  m_name = new QLineEdit(name, this);
  m_name->setObjectName("ui_workspaceName");
  m_description = new QPlainTextEdit(description, this);
  m_description->setObjectName("ui_workspaceDescription");
  m_description->setPlaceholderText(tr("What it measures, how it is wired up ..."));
  m_description->setTabChangesFocus(true);
  m_description->setFixedHeight(fontMetrics().height() * 5);
  form->addRow(tr("&Name:"), m_name);
  form->addRow(tr("&Description:"), m_description);
  layout->addLayout(form);
  // the picture the file keeps, for the list of workspaces
  auto *preview = new QLabel(this);
  preview->setObjectName("ui_workspacePicture");
  preview->setPixmap(picture);
  preview->setAlignment(Qt::AlignCenter);
  layout->addWidget(preview);

  auto *buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
  buttons->button(QDialogButtonBox::Ok)->setText(tr("&Save..."));
  buttons->button(QDialogButtonBox::Ok)->setDefault(true);
  connect(buttons, &QDialogButtonBox::accepted, this, &QDialog::accept);
  connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
  layout->addWidget(buttons);
  m_name->setFocus();
  m_name->selectAll();
}

QString WorkspaceDlg::name() const
{
  return m_name->text().trimmed();
}

QString WorkspaceDlg::description() const
{
  return m_description->toPlainText().trimmed();
}
