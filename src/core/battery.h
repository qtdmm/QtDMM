// Copyright (c) 2026 The QtDMM developers
// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <QtGlobal>

/// The meter's battery in three steps, as its display shows it. A meter
/// that only has a low-battery annunciator gives Low or Unknown, never Full.
enum class Battery : quint8
{
  Unknown,   ///< the meter does not tell
  Low,
  Mid,
  Full
};

/// A battery level @p level of a meter's scale 0..@p max (max = full) in
/// thirds; Unknown for a level off the scale.
constexpr Battery batteryOfLevel(int level, int max)
{
  if (max <= 0 || level < 0 || level > max)
    return Battery::Unknown;
  return 3 * level < max + 1 ? Battery::Low : 3 * level < 2 * max + 1 ? Battery::Mid : Battery::Full;
}
