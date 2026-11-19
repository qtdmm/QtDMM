// Copyright (c) 2026 The QtDMM developers
// SPDX-License-Identifier: GPL-3.0-or-later
//
// Logic nodes (PR 1a): the common form (LogicNode), a node as a source of a
// DeviceSession (MeterController Kind::Node, NodeRunner): triggered by its
// leading input at that input's time, inputs missing or too old, off is the
// bypass, loops refused, recorded and served like a device. The node types
// here are made for the test; the formula node (PR 1b) is tested at the
// end; integral and generator come later.

#include <QCoreApplication>
#include <QTemporaryDir>

#include "core/devicelibrary.h"
#include "core/reading.h"
#include "logic/formulanode.h"
#include "logic/generatornode.h"
#include "logic/logicnode.h"
#include "recording/recordingstore.h"
#include "service/devicesession.h"
#include "service/metercontroller.h"
#include "service/noderunner.h"
#include "service/scpiserver.h"
#include "service/servicehost.h"
#include "testutil.h"

namespace
{
// no input, on a clock: 1, 2, 3 ... V
class Counter : public LogicNode
{
public:
  explicit Counter(int periodMs) { setPeriodMs(periodMs); }
  QString type() const override { return "counter"; }
  QList<Output> outputs() const override { return { { "count", "V" } }; }
  Values evaluate(const Context &) override { return { double(++m_n) }; }
  void reset() override { m_n = 0; }
  int m_n = 0;
};

// two inputs: their product in W, a gap without both; off: nothing
class Product : public LogicNode
{
public:
  QString type() const override { return "product"; }
  QList<Output> outputs() const override { return { { QString(), "W" } }; }
  Values evaluate(const Context &ctx) override
  {
    if (ctx.inputs.size() < 2 || !ctx.inputs[0] || !ctx.inputs[1])
      return { std::nullopt };
    return { ctx.inputs[0]->value * ctx.inputs[1]->value };
  }
};

// one input: twice it; off: the input as it is (a filter's bypass)
class Twice : public LogicNode
{
public:
  QString type() const override { return "twice"; }
  QList<Output> outputs() const override { return { { QString(), "V" } }; }
  Values evaluate(const Context &ctx) override { return { ctx.inputs.value(0) ? std::optional(ctx.inputs[0]->value * 2) : std::nullopt }; }
  Values bypass(const Context &ctx) override { return { ctx.inputs.value(0) ? std::optional(ctx.inputs[0]->value) : std::nullopt }; }
  QJsonObject parameters() const override { return { { "factor", 2 } }; }
};

QList<ValueRef> refs(std::initializer_list<const char *> sources)
{
  QList<ValueRef> out;
  for (const char *s : sources)
    out << ValueRef(0, QString::fromLatin1(s));
  return out;
}
}

int main(int argc, char **argv)
{
  QCoreApplication app(argc, argv);

  // 1. loops: a node may not take its own values, nor close a ring
  {
    const QMap<QString, QList<ValueRef>> nodes { { "device:b", refs({ "device:c" }) }, { "device:c", refs({ "device:a" }) } };
    auto inputsOf = [&nodes](const QString &s) { return nodes.value(s); };
    check(LogicNode::makesLoop("device:a", refs({ "device:a" }), inputsOf), "its own value: a loop");
    check(LogicNode::makesLoop("device:a", refs({ "device:b" }), inputsOf), "a -> b -> c -> a: a loop");
    check(!LogicNode::makesLoop("device:x", refs({ "device:b", "device:m" }), inputsOf), "x -> b -> c -> a: no loop");
  }

  // 2. the common form as stored, and back
  {
    Twice t;
    t.setInputs(refs({ "device:u", "device:i" }));
    t.setLeadingInput(1);
    t.setMaxAgeMs(1500);
    t.setOn(false);
    const QJsonObject o = t.toJson();
    Twice back;
    back.fromJson(o);
    check(o["type"] == "twice" && o["parameters"].toObject()["factor"] == 2, "type and parameters stored");
    check(back.inputs().size() == 2 && back.inputs()[1].toString() == "0@device:i" && back.leadingInput() == 1
            && back.maxAgeMs() == 1500 && !back.isOn() && back.periodMs() == 0,
          "inputs, leading, max age, off read back");
  }

  QTemporaryDir dir;
  DeviceSession session;
  session.setInstanceId("test");
  session.setConfigDir(dir.path());
  auto ask = [&](const char *line) { return QString::fromLatin1(session.host()->scpi()->process(line)).trimmed(); };

  // 3. a node without inputs on a clock: a source of readings like a device
  QString error;
  MeterController *u = session.openNode("u", "Counter U", std::make_unique<Counter>(40), &error);
  check(u && error.isEmpty() && u->isNode() && !u->dmm(), "the counter opens as a node: " + error);
  if (!u)
    return 1;
  Reading lastU;
  QObject::connect(u, &MeterController::reading, [&lastU](const Reading &r) { if (r.id == 0) lastU = r; });
  check(waitFor([&] { return lastU.value >= 2; }, 3000), "the counter counts");
  check(lastU.source == "device:u" && lastU.baseUnit == "V" && lastU.name == "count" && u->isWanted()
          && u->linkState() == MeterConnection::LinkState::Connected,
        QString("its readings: %1 %2 from %3").arg(lastU.value).arg(lastU.baseUnit, lastU.source));
  check(u->sourceInfo().vendor == "QtDMM" && u->sourceInfo().model == "Counter" && u->valueCount() == 1, "its model: QtDMM Counter");

  // 4. two inputs, the first leads: computed at its readings, at its time
  MeterController *i = session.openNode("i", "Counter I", std::make_unique<Counter>(40), &error);
  auto product = std::make_unique<Product>();
  product->setInputs(refs({ "device:u", "device:i" }));
  MeterController *p = session.openNode("p", "Power", std::move(product), &error);
  check(i && p, "two more nodes: " + error);
  if (!i || !p)
    return 1;
  QList<Reading> powers;
  // lastU's connection is older than the node's: it has the reading first
  const Reading &leading = lastU;
  bool sameTime = true;
  QObject::connect(p, &MeterController::reading, [&](const Reading &r)
  {
    powers << r;
    // computed right at the leading input's reading, with its time
    sameTime = sameTime && r.t == leading.t && r.msecs == leading.msecs;
  });
  check(waitFor([&] { return powers.size() >= 5; }, 3000), "the product comes");
  check(sameTime, "each at the time of the leading input's reading");
  const Reading lastP = powers.last();
  check(lastP.baseUnit == "W" && lastP.quality == Quality::Valid && lastP.value > 0 && lastP.source == "device:p",
        QString("a power in W: %1 %2").arg(lastP.value).arg(lastP.baseUnit));
  check(ask("INST:CAT?").contains("\"POWER\""), "on the SCPI server like a device: " + ask("INST:CAT?"));
  check(session.nodeInputs("device:p").size() == 2 && session.nodeInputs("device:u").isEmpty(), "the session knows its inputs");

  // 5. a loop is refused: a node taking the product's value as the product's input
  {
    auto loop = std::make_unique<Twice>();
    loop->setInputs(refs({ "device:loop" }));
    check(!session.openNode("loop", "Loop", std::move(loop), &error) && error.contains("loop"), "its own value refused: " + error);
  }

  // 6. an input closed: no value (a gap), and the node says which it waits for
  session.close("i");
  powers.clear();
  check(waitFor([&] { return powers.size() >= 2; }, 3000), "still computed at the leading input");
  check(powers.last().quality == Quality::NoValue, "without the second input: no value");
  check(p->linkState() == MeterConnection::LinkState::Timeout && p->errorString().contains("device:i"),
        "waiting for device:i: " + p->errorString());

  // ... and when it opens again, it is taken at once
  i = session.openNode("i", "Counter I", std::make_unique<Counter>(40), &error);
  powers.clear();
  check(waitFor([&] { return !powers.isEmpty() && powers.last().quality == Quality::Valid; }, 3000), "a value again");
  check(p->linkState() == MeterConnection::LinkState::Connected, "running again");

  // 7. off is the bypass: the product gives nothing, a filter its input
  NodeRunner::of(p)->setOn(false);
  powers.clear();
  check(waitFor([&] { return powers.size() >= 2; }, 3000) && powers.last().quality == Quality::NoValue, "the product off: a gap");
  auto twice = std::make_unique<Twice>();
  twice->setInputs(refs({ "device:u" }));
  MeterController *t = session.openNode("t", "Twice", std::move(twice), &error);
  Reading lastT;
  QObject::connect(t, &MeterController::reading, [&lastT](const Reading &r) { lastT = r; });
  check(waitFor([&] { return lastT.msecs > 0; }, 3000) && qAbs(lastT.value - 2 * leading.value) < 1e-9, "on: twice the input");
  NodeRunner::of(t)->setOn(false);
  lastT = Reading();
  check(waitFor([&] { return lastT.msecs > 0; }, 3000) && qAbs(lastT.value - leading.value) < 1e-9, "off: the input passed on");

  // 8. a node stopped computes nothing
  p->connectMeter(false);
  powers.clear();
  waitFor([] { return false; }, 200);
  check(powers.isEmpty() && !p->isWanted() && p->linkState() == MeterConnection::LinkState::Closed, "stopped: no readings");
  p->connectMeter(true);
  check(waitFor([&] { return !powers.isEmpty(); }, 3000), "started again");

  // 9. recorded like a device: a recording of the session takes it
  session.startRecording();
  check(waitFor([&] { return p->recorder()->count() >= 3 && u->recorder()->count() >= 3; }, 3000),
        QString("recorded: %1 readings of the node").arg(p->recorder()->count()));
  session.stopRecording();

  // 10. the formula node: its own checks and units
  {
    QMap<QString, QString> units { { "v1", "V" }, { "v2", "A" }, { "v3", "C" }, { "v4", "C" }, { "y1", "mW" } };
    auto unit = [&units](const char *f) { return FormulaNode::suggestUnit(f, units).value_or("?"); };
    check(unit("v1 * v2") == "W" && unit("v1 / v2") == "Ohm" && unit("v2 / v1") == "S" && unit("y1 / v1") == "A"
            && unit("(v1 + 2*v1) * v2 / 1k") == "W" && unit("1 / v1 * v1") == "",
          "units: V*A W, V/A Ohm, A/V S, W/V A, a number none: " + unit("v1 * v2") + unit("v1 / v2") + unit("y1 / v1"));
    check(unit("v3 - v4") == "C" && unit("(v3 + v4) / 2") == "C", "a sum of °C is °C: " + unit("v3 - v4"));
    check(unit("sin(v1)") == "?" && unit("v1 + v2") == "?" && unit("v1 + 1") == "?" && unit("v3 * v4") == "?"
            && unit("v1 * rand()") == "?",
          "no unit for sin(V), V + A, V + 1, °C², rand()");
    check(FormulaNode::check("v1 * v2", 0, 2).isEmpty() && FormulaNode::check("y1 / v1", 1, 1).isEmpty(), "known variables");
    int pos = -1;
    check(!FormulaNode::check("v1 * v3", 0, 2, &pos).isEmpty() && pos == 5, "v3 of two inputs: unknown, at 5");
    check(!FormulaNode::check("y1", 0, 2).isEmpty() && !FormulaNode::check("v1 *", 0, 2).isEmpty(),
          "y1 in the first output, a formula cut short");
  }

  // 11. a formula as an entry of My devices: P = U * I and P in mW from y1
  {
    FormulaNode f;
    f.setInputs(refs({ "device:u", "device:i" }));
    f.setFormulas({ { "Power", "W", "v1 * v2" }, { QString(), "mW", "y1" }, { "Broken", "V", "v1 +" } });
    QVariantMap keys { { "DMM/model", "QtDMM Formula" }, { "Node/json", f.toJsonText() }, { "Port settings/baud", 9600 } };
    const QVariantMap entry = DeviceLibrary::entryKeys(keys);
    check(entry.size() == 2 && entry.contains("Node/json"), "an entry keeps its model and Node/json");
    MyDevice device;
    device.keys = entry;
    check(device.isNode() && device.where() == "v1 * v2; y1; v1 +", "where: its formulas: " + device.where());

    MeterController *w = session.open("w", entry, "Power W", &error);
    check(w && w->isNode() && w->valueCount() == 3 && w->sourceInfo().model == "Formula", "opened as a node: " + error);
    if (!w)
      return 1;
    QMap<int, Reading> last;
    QObject block;   // the connection ends with this block, as last does
    QObject::connect(w, &MeterController::reading, &block, [&last](const Reading &r) { last[r.id] = r; });
    check(waitFor([&] { return last.size() == 3 && last[0].quality == Quality::Valid; }, 3000), "three outputs come");
    // the inputs are counters: v1 the leading one at its reading, v2 its last
    const Reading p0 = last[0];
    check(p0.name == "Power" && p0.baseUnit == "W" && p0.value > 0, QString("Power in W: %1").arg(p0.value));
    check(last[1].baseUnit == "W" && qAbs(last[1].value - p0.value) < 1e-9 * qMax(1.0, p0.value),
          QString("y1 in mW is the same power: %1 %2").arg(last[1].value).arg(p0.value));
    check(last[2].quality == Quality::NoValue, "a formula that does not parse: no value");

    // the input it multiplies by closed: a gap
    session.close("i");
    last.clear();
    check(waitFor([&] { return last.contains(0) && last[0].quality == Quality::NoValue; }, 3000), "v2 closed: a gap");
    // off: a gap
    i = session.openNode("i", "Counter I", std::make_unique<Counter>(40), &error);
    NodeRunner::of(w)->setOn(false);
    last.clear();
    check(waitFor([&] { return last.contains(0); }, 3000) && last[0].quality == Quality::NoValue, "off: a gap");

    QVariantMap unknown { { "DMM/model", "QtDMM Formula" }, { "Node/json", "{\"type\":\"none\"}" } };
    check(!session.open("x", unknown, "X", &error) && error.contains("unknown"), "an unknown kind does not open: " + error);
  }

  // 12. a device of two values as one input: v1 its main value, v1.2 the second
  {
    auto gen = std::make_unique<GeneratorNode>();
    GeneratorNode::Wave volts;
    volts.shape = CalcExpr::Constant;
    volts.max = "3";
    GeneratorNode::Wave amps = volts;
    amps.unit = "A";
    amps.max = "5";
    gen->setWaves({ volts, amps });
    gen->setPeriodMs(40);
    MeterController *g = session.openNode("g", "Two values", std::move(gen), &error);
    auto q = std::make_unique<FormulaNode>();
    q->setInputs(refs({ "device:g" }));
    q->setFormulas({ { "P", "W", "v1 * v1.2" }, { QString(), "A", "v1.2 - v1.1" } });
    MeterController *qc = session.openNode("q", "P of two", std::move(q), &error);
    check(g && qc, "a device of two values and a formula over both: " + error);
    QMap<int, Reading> got;
    QObject block;   // the connection ends with this block, as got does
    if (qc)
      QObject::connect(qc, &MeterController::reading, &block, [&got](const Reading &r) { got[r.id] = r; });
    check(waitFor([&] { return got.size() == 2 && got[0].quality == Quality::Valid; }, 3000)
            && qAbs(got[0].value - 15) < 1e-9 && qAbs(got[1].value - 2) < 1e-9,
          QString("3 V * 5 A, 5 - 3: %1 %2").arg(got[0].value).arg(got[1].value));
  }

  // 13. a formula node stored with one value per input (1b) is read with devices
  {
    const char *before = R"({"type":"formula","inputs":["0@device:u","1@device:i","0@device:i"],"leading":1,
      "parameters":{"outputs":[{"name":"P","unit":"W","formula":"v1 * v2 + v3"},{"unit":"V","formula":"y1 / v2"}]}})";
    std::unique_ptr<LogicNode> n = LogicNode::fromJsonText(QString::fromLatin1(before));
    auto *f = dynamic_cast<FormulaNode *>(n.get());
    check(f && f->inputs().size() == 2 && f->inputs()[0].source == "device:u" && f->inputs()[1].source == "device:i"
            && f->leadingInput() == 1 && f->formulas().value(0).text == "v1 * v2.2 + v2" && f->formulas().value(1).text == "y1 / v2.2",
          "its inputs the devices, v2 of the second value v2.2: " + (f ? f->toJsonText() : QString()));
    check(f && f->parameters()["deviceInputs"].toBool(), "stored again with devices");
  }

  return failed == 0 ? 0 : 1;
}
