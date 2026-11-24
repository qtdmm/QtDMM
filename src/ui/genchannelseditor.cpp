// Copyright (c) 2026 The QtDMM developers
// SPDX-License-Identifier: GPL-3.0-or-later
#include "ui/genchannelseditor.h"

#include <QCheckBox>
#include <QDoubleSpinBox>
#include <QGridLayout>
#include <QLabel>

#include <cmath>

#include "ui/colorbutton.h"

namespace
{
enum Column { Shown, Colour, AmpMax, OffsetMax, AmpCoarse, AmpFine, OffsetCoarse, OffsetFine };
}

GenChannelsEditor::GenChannelsEditor(QWidget *parent) : QWidget(parent), m_grid(new QGridLayout(this))
{
  m_grid->setContentsMargins(0, 0, 0, 0);
  const QStringList heads = { tr("Channel"), tr("Colour"), tr("Highest amplitude"), tr("Highest offset"),
                              tr("Amplitude step"), tr("Fine step"), tr("Offset step"), tr("Fine step") };
  const QStringList tips = {
    tr("Shown on the front, recorded; a channel not shown is never switched by QtDMM"),
    tr("On the LCD, the knobs and the keys"),
    tr("The highest amplitude (peak to peak) the knob sets: protects what is connected"),
    tr("The highest offset the knob sets, either way; offset and half the amplitude stay within it"),
    tr("Per wheel step or drag step"),
    tr("Per step with Shift held"),
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

void GenChannelsEditor::load(const FunctionGenerator *generator)
{
  for (const Row &r : std::as_const(m_rows))
    for (QWidget *w : std::initializer_list<QWidget *> { r.shown, r.color, r.ampMax, r.offsetMax, r.ampCoarse, r.ampFine,
                                                          r.offsetCoarse, r.offsetFine })
      delete w;
  m_rows.clear();
  if (!generator)
    return;
  for (int ch = 0; ch < generator->channelCount(); ++ch)
  {
    Row r;
    r.ch = ch;
    r.info = generator->channel(ch);
    const FunctionGenerator::ChannelSettings s = generator->settings(ch);
    const int line = ch + 1;
    r.shown = new QCheckBox(r.info.name, this);
    r.shown->setChecked(s.shown);
    r.color = new ColorButton(this);
    r.color->setColor(generator->color(ch));
    auto volts = [this](double value, double lo, double hi, int decimals, const QString &unit)
    {
      auto *box = new QDoubleSpinBox(this);
      box->setDecimals(decimals);
      box->setRange(lo, hi);
      box->setSuffix(' ' + unit);
      box->setValue(value);
      return box;
    };
    const double ampResolution = std::pow(10.0, -r.info.ampDecimals);
    const double offsetResolution = std::pow(10.0, -r.info.offsetDecimals);
    r.ampMax = volts(std::isnan(s.ampMax) ? r.info.ampMax : s.ampMax, r.info.ampMin, r.info.ampMax, r.info.ampDecimals,
                     QStringLiteral("Vpp"));
    r.offsetMax = volts(std::isnan(s.offsetMax) ? r.info.offsetMax : s.offsetMax, 0, r.info.offsetMax, r.info.offsetDecimals,
                        QStringLiteral("V"));
    r.ampCoarse = volts(s.ampStepCoarse, ampResolution, r.info.ampMax, r.info.ampDecimals, QStringLiteral("V"));
    r.ampFine = volts(s.ampStepFine, ampResolution, r.info.ampMax, r.info.ampDecimals, QStringLiteral("V"));
    r.offsetCoarse = volts(s.offsetStepCoarse, offsetResolution, r.info.offsetMax, r.info.offsetDecimals, QStringLiteral("V"));
    r.offsetFine = volts(s.offsetStepFine, offsetResolution, r.info.offsetMax, r.info.offsetDecimals, QStringLiteral("V"));
    m_grid->addWidget(r.shown, line, Shown);
    m_grid->addWidget(r.color, line, Colour);
    m_grid->addWidget(r.ampMax, line, AmpMax);
    m_grid->addWidget(r.offsetMax, line, OffsetMax);
    m_grid->addWidget(r.ampCoarse, line, AmpCoarse);
    m_grid->addWidget(r.ampFine, line, AmpFine);
    m_grid->addWidget(r.offsetCoarse, line, OffsetCoarse);
    m_grid->addWidget(r.offsetFine, line, OffsetFine);
    connect(r.shown, &QCheckBox::toggled, this, [this] { keepOneShown(); Q_EMIT changed(); });
    connect(r.color, qOverload<>(&ColorButton::valueChanged), this, &GenChannelsEditor::changed);
    for (QDoubleSpinBox *box : { r.ampMax, r.offsetMax, r.ampCoarse, r.ampFine, r.offsetCoarse, r.offsetFine })
      connect(box, &QDoubleSpinBox::valueChanged, this, &GenChannelsEditor::changed);
    m_rows << r;
  }
  keepOneShown();
}

void GenChannelsEditor::keepOneShown()
{
  int shown = 0;
  for (const Row &r : std::as_const(m_rows))
    shown += r.shown->isChecked();
  for (const Row &r : std::as_const(m_rows))
    r.shown->setEnabled(!(shown == 1 && r.shown->isChecked()));
}

QList<FunctionGenerator::ChannelSettings> GenChannelsEditor::settings() const
{
  QList<FunctionGenerator::ChannelSettings> list;
  for (const Row &r : m_rows)
  {
    FunctionGenerator::ChannelSettings s;
    s.shown = r.shown->isChecked();
    // the channel's own colour and the device's limits stay "the device's"
    s.color = r.color->color() == FunctionGenerator::defaultColor(r.ch) ? QColor() : r.color->color();
    s.ampMax = std::abs(r.ampMax->value() - r.info.ampMax) < std::pow(10.0, -r.info.ampDecimals) / 2 ? FunctionGenerator::kNone
                                                                                                    : r.ampMax->value();
    s.offsetMax = std::abs(r.offsetMax->value() - r.info.offsetMax) < std::pow(10.0, -r.info.offsetDecimals) / 2
                    ? FunctionGenerator::kNone
                    : r.offsetMax->value();
    s.ampStepCoarse = r.ampCoarse->value();
    s.ampStepFine = r.ampFine->value();
    s.offsetStepCoarse = r.offsetCoarse->value();
    s.offsetStepFine = r.offsetFine->value();
    list << s;
  }
  return list;
}

void GenChannelsEditor::apply(FunctionGenerator *generator) const
{
  if (!generator)
    return;
  const QList<FunctionGenerator::ChannelSettings> list = settings();
  for (int k = 0; k < int(list.size()); ++k)
    generator->setSettings(m_rows[k].ch, list[k]);
}
