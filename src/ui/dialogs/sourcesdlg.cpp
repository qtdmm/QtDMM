// Copyright (c) 2026 The QtDMM developers
// SPDX-License-Identifier: GPL-3.0-or-later
#include "ui/dialogs/sourcesdlg.h"

#include <QDialogButtonBox>
#include <QHeaderView>
#include <QLabel>
#include <QTreeWidget>
#include <QVBoxLayout>

namespace
{
constexpr int kSource = Qt::UserRole;
constexpr int kValue = Qt::UserRole + 1;
}

SourcesDlg::SourcesDlg(const QString &view, bool several, const QList<Meter> &meters, QWidget *parent)
  : QDialog(parent)
  , m_tree(new QTreeWidget(this))
  , m_several(several)
{
  setWindowTitle(tr("QtDMM: Sources - %1").arg(view));
  auto *layout = new QVBoxLayout(this);
  auto *hint = new QLabel(several ? tr("The values of the open devices shown here:")
                                  : tr("The value of an open device shown here:"), this);
  layout->addWidget(hint);
  m_tree->setHeaderHidden(true);
  m_tree->setRootIsDecorated(false);
  m_tree->setSelectionMode(QAbstractItemView::NoSelection);
  layout->addWidget(m_tree, 1);
  m_filling = true;
  for (const Meter &meter : meters)
  {
    auto *top = new QTreeWidgetItem(m_tree, { meter.name });
    QFont bold = top->font(0);
    bold.setBold(true);
    top->setFont(0, bold);
    top->setFlags(Qt::ItemIsEnabled);
    for (int id = 0; id < meter.values.size(); ++id)
    {
      auto *item = new QTreeWidgetItem(top, { QString("%1  %2").arg(id + 1).arg(meter.values[id]) });
      item->setData(0, kSource, meter.source);
      item->setData(0, kValue, id);
      item->setFlags(Qt::ItemIsEnabled | Qt::ItemIsUserCheckable);
      item->setCheckState(0, Qt::Unchecked);
    }
    top->setExpanded(true);
  }
  m_filling = false;
  connect(m_tree, &QTreeWidget::itemChanged, this, [this](QTreeWidgetItem *item) { ticked(item); });
  auto *buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
  connect(buttons, &QDialogButtonBox::accepted, this, &QDialog::accept);
  connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
  layout->addWidget(buttons);
  resize(360, 320);
}

void SourcesDlg::setChosen(const QList<ValueRef> &values)
{
  m_filling = true;
  for (int m = 0; m < m_tree->topLevelItemCount(); ++m)
  {
    QTreeWidgetItem *top = m_tree->topLevelItem(m);
    for (int v = 0; v < top->childCount(); ++v)
    {
      QTreeWidgetItem *item = top->child(v);
      const ValueRef ref(item->data(0, kValue).toInt(), item->data(0, kSource).toString());
      item->setCheckState(0, values.contains(ref) ? Qt::Checked : Qt::Unchecked);
    }
  }
  m_filling = false;
}

QList<ValueRef> SourcesDlg::chosen() const
{
  QList<ValueRef> values;
  for (int m = 0; m < m_tree->topLevelItemCount(); ++m)
  {
    const QTreeWidgetItem *top = m_tree->topLevelItem(m);
    for (int v = 0; v < top->childCount(); ++v)
      if (top->child(v)->checkState(0) == Qt::Checked)
        values << ValueRef(top->child(v)->data(0, kValue).toInt(), top->child(v)->data(0, kSource).toString());
  }
  return values;
}

void SourcesDlg::ticked(QTreeWidgetItem *item)
{
  if (m_filling || m_several || item->checkState(0) != Qt::Checked)
    return;
  m_filling = true;
  for (int m = 0; m < m_tree->topLevelItemCount(); ++m)
  {
    QTreeWidgetItem *top = m_tree->topLevelItem(m);
    for (int v = 0; v < top->childCount(); ++v)
      if (top->child(v) != item)
        top->child(v)->setCheckState(0, Qt::Unchecked);
  }
  m_filling = false;
}
