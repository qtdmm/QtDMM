// Copyright (c) 2026 The QtDMM developers
// SPDX-License-Identifier: GPL-3.0-or-later
#include "ui/dialogs/sourcesdlg.h"

#include <QDialogButtonBox>
#include <QComboBox>
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
      auto *item = new QTreeWidgetItem(top, { QStringLiteral("%1  %2").arg(id + 1).arg(meter.values[id]) });
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

SourcesDlg::SourcesDlg(const QString &view, const QList<Meter> &meters, const ValueRef &x, const ValueRef &y,
                       QWidget *parent) :
  SourcesDlg(view, true, meters, parent)
{
  setWindowTitle(tr("QtDMM: Sources - %1").arg(view));
  if (QLabel *hint = findChild<QLabel *>())
    hint->setText(tr("Choose one value for each axis:"));
  m_tree->hide();
  auto *layout = qobject_cast<QVBoxLayout *>(this->layout());
  m_x = new QComboBox(this);
  m_y = new QComboBox(this);
  for (const Meter &meter : meters)
    for (int id = 0; id < meter.values.size(); ++id)
    {
      const ValueRef value(id, meter.source);
      const QString text = QString("%1 — %2").arg(meter.name, meter.values[id]);
      m_x->addItem(text, value.toString());
      m_y->addItem(text, value.toString());
    }
  layout->insertWidget(1, new QLabel(tr("X axis:"), this));
  layout->insertWidget(2, m_x);
  layout->insertWidget(3, new QLabel(tr("Y axis:"), this));
  layout->insertWidget(4, m_y);
  auto select = [](QComboBox *combo, const ValueRef &value)
  {
    for (int i = 0; i < combo->count(); ++i)
      if (ValueRef::fromString(combo->itemData(i).toString()) == value)
        combo->setCurrentIndex(i);
  };
  select(m_x, x);
  select(m_y, y);
  resize(420, 180);
}

ValueRef SourcesDlg::xValue() const
{
  return m_x ? ValueRef::fromString(m_x->currentData().toString()) : ValueRef();
}

ValueRef SourcesDlg::yValue() const
{
  return m_y ? ValueRef::fromString(m_y->currentData().toString()) : ValueRef();
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


SourcesDlg::SourcesDlg(const QString &view, const QList<Meter> &meters, const ValueRef &x, const ValueRef &y,
                       const ValueRef &z, QWidget *parent) :
  SourcesDlg(view, meters, x, y, parent)
{
  m_z = new QComboBox(this);
  for (const Meter &meter : meters)
    for (int id = 0; id < meter.values.size(); ++id)
      m_z->addItem(QString("%1 — %2").arg(meter.name, meter.values[id]), ValueRef(id, meter.source).toString());
  for (int i = 0; i < m_z->count(); ++i)
    if (ValueRef::fromString(m_z->itemData(i).toString()) == z)
      m_z->setCurrentIndex(i);
  auto *layout = qobject_cast<QVBoxLayout *>(this->layout());
  layout->addWidget(new QLabel(tr("Color (Z):"), this));
  layout->addWidget(m_z);
  resize(420, 220);
}

ValueRef SourcesDlg::zValue() const
{
  return m_z ? ValueRef::fromString(m_z->currentData().toString()) : ValueRef();
}
