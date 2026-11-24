// Copyright (c) 2026 The QtDMM developers
// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <QList>
#include <QWidget>

#include "core/functiongenerator.h"

class ColorButton;
class QCheckBox;
class QDoubleSpinBox;
class QGridLayout;

/// The channels of a function generator in its settings, one row each:
/// shown on the front or not, its colour, the highest amplitude and offset
/// the knobs may set (to protect what is connected; within the device's
/// limits) and the steps of those two knobs, coarse and fine (Shift). At
/// least one channel stays shown. A limit left at the device's, a colour
/// left at the channel's own, stays "the device's" (NaN, invalid in
/// ChannelSettings). As PsuChannelsEditor for a power supply.
class GenChannelsEditor : public QWidget
{
  Q_OBJECT
public:
  explicit GenChannelsEditor(QWidget *parent = nullptr);

  void    load(const FunctionGenerator *generator);
  QList<FunctionGenerator::ChannelSettings> settings() const;
  void    apply(FunctionGenerator *generator) const;

Q_SIGNALS:
  void    changed();

private:
  struct Row
  {
    FunctionGenerator::ChannelInfo info;
    int            ch = 0;
    QCheckBox     *shown = nullptr;
    ColorButton   *color = nullptr;
    QDoubleSpinBox *ampMax = nullptr, *offsetMax = nullptr;
    QDoubleSpinBox *ampCoarse = nullptr, *ampFine = nullptr, *offsetCoarse = nullptr, *offsetFine = nullptr;
  };
  void    keepOneShown();

  QGridLayout *m_grid;
  QList<Row>   m_rows;
};
