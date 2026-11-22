// Copyright (c) 2026 The QtDMM developers
// SPDX-License-Identifier: GPL-3.0-or-later
#include "ui/dialogs/notedlg.h"

#include <QDialogButtonBox>
#include <QFormLayout>
#include <QLabel>
#include <QLineEdit>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QVBoxLayout>

NoteDlg::NoteDlg(const QString &when, QWidget *parent) :
  QDialog(parent)
{
  setObjectName("ui_noteDlg");
  setWindowTitle(tr("Note"));
  auto *top = new QVBoxLayout(this);
  top->addWidget(new QLabel(when, this));
  auto *form = new QFormLayout;
  top->addLayout(form);
  m_title = new QLineEdit(this);
  m_title->setObjectName("ui_noteTitle");
  m_title->setPlaceholderText(tr("on the flag, e.g. Load on"));
  form->addRow(tr("&Title:"), m_title);
  m_text = new QPlainTextEdit(this);
  m_text->setObjectName("ui_noteText");
  m_text->setTabChangesFocus(true);
  m_text->setPlaceholderText(tr("optional: what happened, what was changed"));
  form->addRow(tr("Te&xt:"), m_text);
  auto *buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
  top->addWidget(buttons);
  QPushButton *ok = buttons->button(QDialogButtonBox::Ok);
  ok->setEnabled(false);
  connect(m_title, &QLineEdit::textChanged, ok, [ok](const QString &t) { ok->setEnabled(!t.trimmed().isEmpty()); });
  connect(buttons, &QDialogButtonBox::accepted, this, &QDialog::accept);
  connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
  resize(420, 240);
}

void NoteDlg::setTitle(const QString &title)
{
  m_title->setText(title);
  m_title->selectAll();
}

QString NoteDlg::title() const
{
  return m_title->text().simplified();
}

void NoteDlg::setText(const QString &text)
{
  m_text->setPlainText(text);
}

QString NoteDlg::text() const
{
  return m_text->toPlainText().trimmed();
}
