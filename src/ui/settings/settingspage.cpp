//======================================================================
// File:		prefwidget.cpp
// Author:	Matthias Toussaint
// Created:	Sat Oct 19 14:32:35 CEST 2002
//----------------------------------------------------------------------
// This file is part of QtDMM.
//
// QtDMM is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
//
// QtDMM is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with QtDMM.  If not, see <http://www.gnu.org/licenses/>.
//----------------------------------------------------------------------
// Copyright (c) 2002 Matthias Toussaint
// SPDX-License-Identifier: GPL-3.0-or-later
//======================================================================

#include "ui/settings/settingspage.h"
#include "ui/designs.h"

SettingsPage::SettingsPage(QWidget *parent) : QWidget(parent)
{
}


QIcon SettingsPage::icon() const
{
  return Designs::plainIcon(m_iconName);
}

QFormLayout *SettingsPage::createForm(QWidget *parent)
{
  auto *form = new QFormLayout(parent ? parent : this);
  form->setLabelAlignment(Qt::AlignRight | Qt::AlignVCenter);
  form->setFormAlignment(Qt::AlignLeft | Qt::AlignTop);
  form->setFieldGrowthPolicy(QFormLayout::FieldsStayAtSizeHint);
  form->setRowWrapPolicy(QFormLayout::DontWrapRows);
  return form;
}

QLabel *SettingsPage::addSection(QFormLayout *form, const QString &title)
{
  auto *label = new QLabel(title, form->parentWidget());
  addSection(form, label);
  return label;
}

void SettingsPage::addSection(QFormLayout *form, QWidget *title)
{
  // the space above does what the frame did
  if (form->rowCount() > 0)
  {
    auto *gap = new QWidget(form->parentWidget());
    gap->setFixedHeight(title->fontMetrics().height() / 2);
    form->addRow(gap);
  }
  QFont f = title->font();
  f.setBold(true);
  title->setFont(f);
  title->setProperty("section", true);
  form->addRow(title);
}

void SettingsPage::alignLabels(QFormLayout *form)
{
  QList<QWidget *> labels;
  int width = 0;
  for (int r = 0; r < form->rowCount(); ++r)
    if (QLayoutItem *item = form->itemAt(r, QFormLayout::LabelRole))
      if (QWidget *label = item->widget())
      {
        labels << label;
        width = qMax(width, label->sizeHint().width());
      }
  for (QWidget *label : labels)
  {
    label->setMinimumWidth(width);
    if (auto *l = qobject_cast<QLabel *>(label))
      l->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
  }
}

void SettingsPage::showRow(QFormLayout *form, QWidget *field, bool show)
{
  field->setVisible(show);
  if (QWidget *label = form->labelForField(field))
    label->setVisible(show);
}

QWidget *SettingsPage::row(std::initializer_list<QWidget *> widgets, bool stretch)
{
  auto *box = new QWidget;
  auto *layout = new QHBoxLayout(box);
  layout->setContentsMargins(0, 0, 0, 0);
  for (QWidget *w : widgets)
  {
    if (!w->parentWidget() || w->parentWidget() != box)
      w->setParent(box);
    layout->addWidget(w);
  }
  if (stretch)
    layout->addStretch(1);
  // a label's Alt key goes to the first field
  if (widgets.size() > 0)
    box->setFocusProxy(*widgets.begin());
  return box;
}
