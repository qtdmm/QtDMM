// Copyright (c) 2026 The QtDMM developers
// SPDX-License-Identifier: GPL-3.0-or-later
//
// OutputRegistry, the emergency off: power supplies and generators added
// and counted, every shown output off at once - also one set from outside
// (EXT) and while control is not allowed - a hidden channel untouched, a
// device destroyed or removed leaving with changed().

#include <QCoreApplication>

#include <memory>

#include "core/functiongenerator.h"
#include "core/outputregistry.h"
#include "core/powersupply.h"
#include "testutil.h"

namespace
{
class TestSupply : public PowerSupply
{
public:
  TestSupply() { channelsChanged(); }
  int channelCount() const override { return 2; }
  ChannelInfo channel(int ch) const override
  {
    ChannelInfo info;
    info.name = QString("CH%1").arg(ch + 1);
    return info;
  }
  bool allowed = true;
  bool controlAllowed() const override { return allowed; }
  void external(int ch, bool on) { setExternal(ch, on); }
  void force(int ch, bool on) { confirmOutput(ch, on); }

protected:
  void sendVoltage(int ch, double u) override { confirmVoltage(ch, u); }
  void sendOutput(int ch, bool on) override { confirmOutput(ch, on); }
};

class TestGen : public FunctionGenerator
{
public:
  TestGen() { channelsChanged(); }
  int channelCount() const override { return 1; }
  ChannelInfo channel(int) const override { return ChannelInfo(); }
  void external(bool on) { setExternal(0, on); }
  void force(bool on) { confirmOutput(0, on); }

protected:
  void sendShape(int ch, Shape shape, const QString &arbitrary) override { confirmShape(ch, shape, arbitrary); }
  void sendValue(int ch, Value v, double value) override { confirmValue(ch, v, value); }
  void sendOutput(int ch, bool on) override { confirmOutput(ch, on); }
};
}

int main(int argc, char **argv)
{
  QCoreApplication app(argc, argv);
  OutputRegistry &reg = OutputRegistry::instance();
  int changes = 0;
  QObject::connect(&reg, &OutputRegistry::changed, [&changes] { ++changes; });

  check(reg.count() == 0 && reg.allOff() == 0, "none open: nothing to switch off");

  auto supply = std::make_unique<TestSupply>();
  auto gen = std::make_unique<TestGen>();
  reg.add(supply.get());
  reg.add(gen.get());
  reg.add(supply.get());
  check(reg.count() == 2 && changes == 2, "two devices, each added once");

  // outputs on: CH1 set from outside, CH2 hidden; the generator from outside too
  supply->force(0, true);
  supply->force(1, true);
  supply->external(0, true);
  PowerSupply::ChannelSettings hidden;
  hidden.shown = false;
  supply->setSettings(1, hidden);
  supply->allowed = false;
  gen->force(true);
  gen->external(true);

  check(reg.allOff() == 2, "both asked");
  check(!supply->state(0).output, "supply CH1 off, though set from outside and control not allowed");
  check(supply->state(1).output, "a hidden channel is never touched");
  check(!gen->state(0).output, "generator off, though set from outside");

  // gone: destroyed or removed
  gen.reset();
  check(reg.count() == 1 && changes == 3, "a generator destroyed leaves");
  reg.remove(supply.get());
  check(reg.count() == 0 && changes == 4, "a supply removed leaves");
  supply.reset();
  check(changes == 4, "destroyed after it was removed: no second change");

  if (failed == 0)
    qInfo() << "All output registry tests passed.";
  return failed == 0 ? 0 : 1;
}
