// Copyright (c) 2026 The QtDMM developers
// SPDX-License-Identifier: GPL-3.0-or-later
#include "ui/sourcelist.h"

#include <QAbstractItemModel>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QLabel>
#include <QListWidget>
#include <QPushButton>
#include <QTreeWidget>
#include <QVBoxLayout>

#include "logic/formulanode.h"

namespace
{
constexpr int kSource = Qt::UserRole;
constexpr int kValue = Qt::UserRole + 1;
constexpr int kText = Qt::UserRole + 2;   ///< a value's text without its variable
}

SourceList::SourceList(Mode mode, QWidget *parent) :
  QWidget(parent), m_mode(mode)
{
  auto *row = new QHBoxLayout(this);
  row->setContentsMargins(0, 0, 0, 0);
  m_tree = new QTreeWidget(this);
  m_tree->setObjectName("ui_sourceTree");
  m_tree->setHeaderHidden(true);
  m_tree->setRootIsDecorated(true);
  m_tree->setMinimumHeight(200);
  row->addWidget(m_tree, 3);
  if (mode == Devices)
  {
    // the inputs in their order: v1, v2 ...
    auto *side = new QVBoxLayout;
    side->addWidget(new QLabel(tr("Inputs, in order:"), this));
    m_list = new QListWidget(this);
    m_list->setObjectName("ui_sourceInputs");
    m_list->setDragDropMode(QAbstractItemView::InternalMove);
    m_list->setDefaultDropAction(Qt::MoveAction);
    side->addWidget(m_list, 1);
    auto *buttons = new QHBoxLayout;
    m_up = new QPushButton(QStringLiteral("↑"), this);
    m_up->setObjectName("ui_sourceUp");
    m_up->setToolTip(tr("Earlier: a lower number (v1)"));
    m_down = new QPushButton(QStringLiteral("↓"), this);
    m_down->setObjectName("ui_sourceDown");
    m_down->setToolTip(tr("Later: a higher number"));
    m_lead = new QPushButton(tr("&Leads"), this);
    m_lead->setObjectName("ui_sourceLead");
    m_lead->setToolTip(tr("A new reading of this input computes the values"));
    for (QPushButton *b : { m_up, m_down })
      b->setMaximumWidth(32);
    buttons->addWidget(m_up);
    buttons->addWidget(m_down);
    buttons->addWidget(m_lead);
    buttons->addStretch(1);
    side->addLayout(buttons);
    row->addLayout(side, 2);
    connect(m_up, &QPushButton::clicked, this, [this] { moveInput(m_list->currentRow(), m_list->currentRow() - 1); });
    connect(m_down, &QPushButton::clicked, this, [this] { moveInput(m_list->currentRow(), m_list->currentRow() + 1); });
    connect(m_lead, &QPushButton::clicked, this, [this]
    {
      if (m_list->currentRow() >= 0)
        changeInputs(m_inputs, m_list->currentRow());
    });
    // a drop in the list: its new order
    connect(m_list->model(), &QAbstractItemModel::rowsMoved, this, [this]
    {
      QStringList order;
      for (int i = 0; i < m_list->count(); ++i)
        order << m_list->item(i)->data(kSource).toString();
      const QString lead = m_inputs.value(m_leading);
      changeInputs(order, qMax(0, int(order.indexOf(lead))));
    });
    connect(m_tree, &QTreeWidget::itemChanged, this, [this](QTreeWidgetItem *item)
    {
      if (m_filling || item->parent())
        return;
      setTicked(item->data(0, kSource).toString(), item->checkState(0) == Qt::Checked);
    });
  }
  else
    connect(m_tree, &QTreeWidget::currentItemChanged, this, [this](QTreeWidgetItem *item)
    {
      if (m_filling || !item || !item->parent())
        return;
      m_value = ValueRef(item->data(0, kValue).toInt(), item->data(0, kSource).toString());
      Q_EMIT valueChanged(m_value);
    });
}

void SourceList::setSources(const QList<Source> &sources)
{
  m_sources = sources;
  fill();
}

void SourceList::setBlocked(std::function<QString(const QString &)> blocked)
{
  m_blocked = std::move(blocked);
  fill();
}

void SourceList::fill()
{
  m_filling = true;
  m_tree->clear();
  const QColor grey = palette().color(QPalette::Disabled, QPalette::Text);
  QStringList known;
  for (const Source &s : std::as_const(m_sources))
    known << s.source;
  // inputs no longer in My devices: kept, and said so
  QList<Source> all = m_sources;
  for (const QString &in : std::as_const(m_inputs))
    if (!known.contains(in))
      all << Source { in, tr("%1 (not in My devices)").arg(in), {}, {}, false, false };
  if (m_mode == Value && !m_value.source.isEmpty() && !known.contains(m_value.source))
    all << Source { m_value.source, tr("%1 (not in My devices)").arg(m_value.source), {}, {}, false, false };
  for (const Source &s : std::as_const(all))
  {
    auto *top = new QTreeWidgetItem(m_tree);
    top->setText(0, s.connected ? s.name : tr("%1 (not connected)").arg(s.name));
    top->setData(0, kSource, s.source);
    top->setIcon(0, QIcon::fromTheme(s.node ? "code-function" : "qtdmm-display"));
    if (!s.connected)
      top->setForeground(0, grey);
    const QString blocked = m_blocked ? m_blocked(s.source) : QString();
    if (m_mode == Devices)
    {
      top->setFlags(Qt::ItemIsEnabled | Qt::ItemIsUserCheckable);
      top->setCheckState(0, m_inputs.contains(s.source) ? Qt::Checked : Qt::Unchecked);
      if (!blocked.isEmpty() && !m_inputs.contains(s.source))
        top->setFlags(Qt::NoItemFlags);
    }
    else
      top->setFlags(Qt::ItemIsEnabled);
    top->setToolTip(0, blocked);
    for (int v = 0; v < s.values.size(); ++v)
    {
      auto *child = new QTreeWidgetItem(top);
      child->setData(0, kSource, s.source);
      child->setData(0, kValue, v);
      const QString unit = s.units.value(v);
      child->setData(0, kText, unit.isEmpty() || s.values[v].contains(unit) ? s.values[v]
                                                                             : QString("%1 (%2)").arg(s.values[v], unit));
      child->setText(0, child->data(0, kText).toString());
      if (m_mode == Devices)
      {
        // part of the device: for information, its name in the formula
        child->setFlags(Qt::ItemIsEnabled);
        child->setForeground(0, grey);
      }
      else
      {
        child->setFlags(blocked.isEmpty() ? Qt::ItemIsEnabled | Qt::ItemIsSelectable : Qt::NoItemFlags);
        child->setToolTip(0, blocked);
        if (m_value.source == s.source && m_value.id == v)
        {
          m_tree->setCurrentItem(child);
          top->setExpanded(true);
        }
      }
    }
    if (m_mode == Devices && m_inputs.contains(s.source))
      top->setExpanded(true);
  }
  m_filling = false;
  updateNames();
  updateList();
}

void SourceList::updateNames()
{
  if (m_mode != Devices)
    return;
  for (int i = 0; i < m_tree->topLevelItemCount(); ++i)
  {
    QTreeWidgetItem *top = m_tree->topLevelItem(i);
    const int input = int(m_inputs.indexOf(top->data(0, kSource).toString()));
    for (int c = 0; c < top->childCount(); ++c)
    {
      QTreeWidgetItem *child = top->child(c);
      const QString text = child->data(0, kText).toString();
      child->setText(0, input < 0 ? text : FormulaNode::inputName(input, c) + ' ' + text);
    }
  }
}

void SourceList::updateList()
{
  if (!m_list)
    return;
  const QSignalBlocker block(m_list->model());
  const int current = m_list->currentRow();
  m_list->clear();
  for (int i = 0; i < m_inputs.size(); ++i)
  {
    QString name = m_inputs[i];
    for (const Source &s : std::as_const(m_sources))
      if (s.source == m_inputs[i])
        name = s.name;
    auto *item = new QListWidgetItem(QString("%1  %2").arg(FormulaNode::inputName(i), name), m_list);
    item->setData(kSource, m_inputs[i]);
    if (i == m_leading)
    {
      // the leading one: bold, said so
      QFont bold = item->font();
      bold.setBold(true);
      item->setFont(bold);
      item->setText(item->text() + "  " + tr("(leads)"));
    }
  }
  m_list->setCurrentRow(qBound(-1, current, m_list->count() - 1));
  m_up->setEnabled(m_inputs.size() > 1);
  m_down->setEnabled(m_inputs.size() > 1);
  m_lead->setEnabled(m_inputs.size() > 1);
}

void SourceList::setInputs(const QStringList &sources, int leading)
{
  m_inputs = sources;
  m_leading = sources.isEmpty() ? 0 : qBound(0, leading, int(sources.size()) - 1);
  fill();
}

void SourceList::changeInputs(const QStringList &inputs, int leading)
{
  QList<int> from;
  for (const QString &s : inputs)
    from << int(m_inputs.indexOf(s));
  m_inputs = inputs;
  m_leading = inputs.isEmpty() ? 0 : qBound(0, leading, int(inputs.size()) - 1);
  updateNames();
  updateList();
  Q_EMIT inputsChanged(from);
}

void SourceList::moveInput(int from, int to)
{
  if (from < 0 || from >= m_inputs.size() || to < 0 || to >= m_inputs.size() || from == to)
    return;
  QStringList order = m_inputs;
  const QString lead = order.value(m_leading);
  order.move(from, to);
  changeInputs(order, int(order.indexOf(lead)));
  m_list->setCurrentRow(to);
}

void SourceList::setTicked(const QString &source, bool on)
{
  QStringList order = m_inputs;
  const QString lead = order.value(m_leading);
  if (on && !order.contains(source))
    order << source;
  else if (!on)
    order.removeAll(source);
  else
    return;
  changeInputs(order, qMax(0, int(order.indexOf(lead))));
  // the tree's tick as it is now (also when set from outside)
  m_filling = true;
  for (int i = 0; i < m_tree->topLevelItemCount(); ++i)
    if (QTreeWidgetItem *top = m_tree->topLevelItem(i); top->data(0, kSource).toString() == source)
    {
      top->setCheckState(0, on ? Qt::Checked : Qt::Unchecked);
      top->setExpanded(on);
    }
  m_filling = false;
}

void SourceList::setValue(const ValueRef &value)
{
  m_value = value;
  fill();
}

QString SourceList::unitOf(const ValueRef &ref) const
{
  for (const Source &s : m_sources)
    if (s.source == ref.source)
      return s.units.value(ref.id);
  return QString();
}
