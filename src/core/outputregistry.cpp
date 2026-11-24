// Copyright (c) 2026 The QtDMM developers
// SPDX-License-Identifier: GPL-3.0-or-later
#include "core/outputregistry.h"

#include "core/functiongenerator.h"
#include "core/powersupply.h"

OutputRegistry &OutputRegistry::instance()
{
  static OutputRegistry registry;
  return registry;
}

void OutputRegistry::add(PowerSupply *supply)
{
  if (!supply || m_supplies.contains(supply))
    return;
  m_supplies << supply;
  connect(supply, &QObject::destroyed, this, &OutputRegistry::forget);
  Q_EMIT changed();
}

void OutputRegistry::add(FunctionGenerator *generator)
{
  if (!generator || m_generators.contains(generator))
    return;
  m_generators << generator;
  connect(generator, &QObject::destroyed, this, &OutputRegistry::forget);
  Q_EMIT changed();
}

void OutputRegistry::remove(QObject *device)
{
  if (device)
    disconnect(device, &QObject::destroyed, this, &OutputRegistry::forget);
  forget(device);
}

void OutputRegistry::forget(QObject *device)
{
  // a destroyed device's QPointer is null already: those go too
  const qsizetype before = m_supplies.size() + m_generators.size();
  m_supplies.removeIf([device](const QPointer<PowerSupply> &p) { return !p || p == device; });
  m_generators.removeIf([device](const QPointer<FunctionGenerator> &p) { return !p || p == device; });
  if (m_supplies.size() + m_generators.size() != before)
    Q_EMIT changed();
}

int OutputRegistry::count() const
{
  int n = 0;
  for (const auto &p : m_supplies)
    n += !p.isNull();
  for (const auto &p : m_generators)
    n += !p.isNull();
  return n;
}

int OutputRegistry::allOff()
{
  int n = 0;
  for (const auto &p : m_supplies)
    if (p)
    {
      p->allOff();
      ++n;
    }
  for (const auto &p : m_generators)
    if (p)
    {
      p->allOff();
      ++n;
    }
  return n;
}
