// Copyright (c) 2026 The QtDMM developers
// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <QList>
#include <QWidget>

#include "core/powersupply.h"

class ColorButton;
class QCheckBox;
class QDoubleSpinBox;
class QGridLayout;

/// The channels of a power supply in its settings, one row each: shown on
/// the front or not, its colour, the voltage the knob may set (within the
/// device's limits) and the knob's steps, coarse and fine (Shift). At
/// least one channel stays shown. A value left at the device's limit, a
/// colour left at the channel's own, stays "the device's" (NaN, invalid
/// in ChannelSettings), so they follow the device.
class PsuChannelsEditor : public QWidget
{
  Q_OBJECT
public:
  explicit PsuChannelsEditor(QWidget *parent = nullptr);

  /// The channels of @p supply with their settings.
  void    load(const PowerSupply *supply);
  /// The settings as edited, one per channel loaded.
  QList<PowerSupply::ChannelSettings> settings() const;
  /// settings() into @p supply.
  void    apply(PowerSupply *supply) const;

Q_SIGNALS:
  void    changed();

private:
  struct Row
  {
    PowerSupply::ChannelInfo info;
    int            ch = 0;
    QCheckBox     *shown = nullptr;
    ColorButton   *color = nullptr;
    QDoubleSpinBox *uMin = nullptr, *uMax = nullptr, *coarse = nullptr, *fine = nullptr;
  };
  /// The last channel shown cannot be unticked.
  void    keepOneShown();

  QGridLayout *m_grid;
  QList<Row>   m_rows;
};
