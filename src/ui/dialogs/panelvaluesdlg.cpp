// Copyright (c) 2026 The QtDMM developers
// SPDX-License-Identifier: GPL-3.0-or-later
#include "ui/dialogs/panelvaluesdlg.h"

#include <QComboBox>
#include <QDialogButtonBox>
#include <QFormLayout>

#include "ui/views/lcdwidget.h"

PanelValuesDlg::PanelValuesDlg(const QStringList &names, int main, const QList<int> &secondaries, QWidget *parent)
  : QDialog(parent)
{
  setWindowTitle(tr("Values on the LCD"));
  auto *form = new QFormLayout(this);
  const QStringList labels { tr("&Main value:"), tr("Secondary value &left:"), tr("Secondary value &right:") };
  // the small line as far as the device has values for it
  const int places = qMin(1 + LcdWidget::kSmallLineValues, int(names.size()));
  for (int i = 0; i < places; ++i)
  {
    auto *box = new QComboBox(this);
    if (i > 0)
      box->addItem(tr("None"), -1);
    for (int id = 0; id < names.size(); ++id)
      box->addItem(names[id], id);
    const int chosen = i == 0 ? main : secondaries.value(i - 1, -1);
    box->setCurrentIndex(box->findData(chosen));
    form->addRow(labels[i], box);
    m_places << box;
    m_chosen << chosen;
    connect(box, &QComboBox::activated, this, [this, i] { swapInto(i); });
  }
  auto *buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
  connect(buttons, &QDialogButtonBox::accepted, this, &QDialog::accept);
  connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
  form->addRow(buttons);
}

void PanelValuesDlg::swapInto(int place)
{
  const int id = m_places[place]->currentData().toInt();
  for (int i = 0; id >= 0 && i < m_places.size(); ++i)
    if (i != place && m_places[i]->currentData().toInt() == id)
      m_places[i]->setCurrentIndex(m_places[i]->findData(m_chosen[place]));
  for (int i = 0; i < m_places.size(); ++i)
    m_chosen[i] = m_places[i]->currentData().toInt();
}

int PanelValuesDlg::main() const
{
  return m_places.first()->currentData().toInt();
}

QList<int> PanelValuesDlg::secondaries() const
{
  QList<int> ids;
  for (int i = 1; i < m_places.size(); ++i)
    if (const int id = m_places[i]->currentData().toInt(); id >= 0)
      ids << id;
  return ids;
}
