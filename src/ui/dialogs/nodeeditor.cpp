// Copyright (c) 2026 The QtDMM developers
// SPDX-License-Identifier: GPL-3.0-or-later
#include "ui/dialogs/nodeeditor.h"

#include <QDialogButtonBox>
#include <QPushButton>
#include <QVBoxLayout>

#include "logic/logicnode.h"

void NodeEditor::setValid(bool valid)
{
  if (valid == m_valid)
    return;
  m_valid = valid;
  Q_EMIT validChanged(valid);
}

NodeDlg::NodeDlg(NodeEditor *editor, const QString &title, QWidget *parent) :
  QDialog(parent), m_editor(editor)
{
  setWindowTitle(title);
  auto *top = new QVBoxLayout(this);
  editor->setParent(this);
  top->addWidget(editor, 1);
  auto *buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
  top->addWidget(buttons);
  m_ok = buttons->button(QDialogButtonBox::Ok);
  m_ok->setEnabled(editor->valid());
  connect(editor, &NodeEditor::validChanged, m_ok, &QPushButton::setEnabled);
  connect(buttons, &QDialogButtonBox::accepted, this, &QDialog::accept);
  connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
}
