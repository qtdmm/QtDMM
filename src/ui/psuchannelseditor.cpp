// Copyright (c) 2026 The QtDMM developers
// SPDX-License-Identifier: GPL-3.0-or-later
#include "ui/psuchannelseditor.h"

#include <QCheckBox>
#include <QDoubleSpinBox>
#include <QGridLayout>
#include <QLabel>

#include <cmath>

#include "ui/colorbutton.h"

namespace
{
enum Column { Shown, Colour, Min, Max, Coarse, Fine };
}

PsuChannelsEditor::PsuChannelsEditor(QWidget *parent) : QWidget(parent), m_grid(new QGridLayout(this))
{
  m_grid->setContentsMargins(0, 0, 0, 0);
  const QStringList heads = { tr("Channel"), tr("Colour"), tr("Lowest"), tr("Highest"), tr("Step"), tr("Fine step") };
  const QStringList tips = {
    tr("Shown on the front, recorded; a channel not shown is never switched by QtDMM"),
    tr("On the LCD, the knob and the key"),
    tr("The lowest voltage the knob sets"),
    tr("The highest voltage the knob sets"),
    tr("Per wheel step or drag step"),
    tr("Per step with Shift held"),
  };
  for (int c = 0; c < int(heads.size()); ++c)
  {
    auto *head = new QLabel(heads[c], this);
    head->setToolTip(tips[c]);
    m_grid->addWidget(head, 0, c);
  }
}

void PsuChannelsEditor::load(const PowerSupply *supply)
{
  for (const Row &r : std::as_const(m_rows))
  {
    delete r.shown;
    delete r.color;
    delete r.uMin;
    delete r.uMax;
    delete r.coarse;
    delete r.fine;
  }
  m_rows.clear();
  if (!supply)
    return;
  for (int ch = 0; ch < supply->channelCount(); ++ch)
  {
    Row r;
    r.ch = ch;
    r.info = supply->channel(ch);
    const PowerSupply::ChannelSettings s = supply->settings(ch);
    const int line = ch + 1;
    r.shown = new QCheckBox(r.info.name, this);
    r.shown->setChecked(s.shown);
    r.color = new ColorButton(this);
    r.color->setColor(supply->color(ch));
    auto volts = [this, &r](double value, double lo, double hi)
    {
      auto *box = new QDoubleSpinBox(this);
      box->setDecimals(r.info.uDecimals);
      box->setRange(lo, hi);
      box->setSuffix(QStringLiteral(" V"));
      box->setValue(value);
      return box;
    };
    r.uMin = volts(supply->minVoltage(ch), r.info.uMin, r.info.uMax);
    r.uMax = volts(supply->maxVoltage(ch), r.info.uMin, r.info.uMax);
    const double resolution = std::pow(10.0, -r.info.uDecimals);
    r.coarse = volts(s.stepCoarse, resolution, r.info.uMax);
    r.fine = volts(s.stepFine, resolution, r.info.uMax);
    m_grid->addWidget(r.shown, line, Shown);
    m_grid->addWidget(r.color, line, Colour);
    m_grid->addWidget(r.uMin, line, Min);
    m_grid->addWidget(r.uMax, line, Max);
    m_grid->addWidget(r.coarse, line, Coarse);
    m_grid->addWidget(r.fine, line, Fine);
    // the lowest never above the highest
    connect(r.uMin, &QDoubleSpinBox::valueChanged, r.uMax, [box = r.uMax](double v) { box->setMinimum(v); });
    r.uMax->setMinimum(r.uMin->value());
    connect(r.shown, &QCheckBox::toggled, this, [this] { keepOneShown(); Q_EMIT changed(); });
    connect(r.color, qOverload<>(&ColorButton::valueChanged), this, &PsuChannelsEditor::changed);
    for (QDoubleSpinBox *box : { r.uMin, r.uMax, r.coarse, r.fine })
      connect(box, &QDoubleSpinBox::valueChanged, this, &PsuChannelsEditor::changed);
    m_rows << r;
  }
  keepOneShown();
}

void PsuChannelsEditor::keepOneShown()
{
  int shown = 0;
  for (const Row &r : std::as_const(m_rows))
    shown += r.shown->isChecked();
  for (const Row &r : std::as_const(m_rows))
    r.shown->setEnabled(!(shown == 1 && r.shown->isChecked()));
}

QList<PowerSupply::ChannelSettings> PsuChannelsEditor::settings() const
{
  QList<PowerSupply::ChannelSettings> list;
  for (const Row &r : m_rows)
  {
    PowerSupply::ChannelSettings s;
    s.shown = r.shown->isChecked();
    // the channel's own colour and the device's limits stay "the device's"
    s.color = r.color->color() == PowerSupply::defaultColor(r.ch) ? QColor() : r.color->color();
    const double resolution = std::pow(10.0, -r.info.uDecimals) / 2;
    s.uMin = std::abs(r.uMin->value() - r.info.uMin) < resolution ? PowerSupply::kNone : r.uMin->value();
    s.uMax = std::abs(r.uMax->value() - r.info.uMax) < resolution ? PowerSupply::kNone : r.uMax->value();
    s.stepCoarse = r.coarse->value();
    s.stepFine = r.fine->value();
    list << s;
  }
  return list;
}

void PsuChannelsEditor::apply(PowerSupply *supply) const
{
  if (!supply)
    return;
  const QList<PowerSupply::ChannelSettings> list = settings();
  for (int k = 0; k < int(list.size()); ++k)
    supply->setSettings(m_rows[k].ch, list[k]);
}
