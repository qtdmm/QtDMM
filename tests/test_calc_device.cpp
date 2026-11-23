// Copyright (c) 2026 The QtDMM developers
// SPDX-License-Identifier: GPL-3.0-or-later

// Tests for CalcDevice, the virtual meter that evaluates a formula over the
// other instances' readings: line format through the real ASCII decoder,
// missing/stale/invalid inputs, the hyphen/underscore alias, open() errors.
#include <QCoreApplication>
#include <QDateTime>
#include <QSharedMemory>
#include <QDebug>
#include <cmath>

#include "device/transports/calc.h"
#include "device/dmmdecoder.h"
#include "testutil.h"

static CalcDevice::Input reading(double value, const QString &unit, qint64 now, bool valid = true)
{
  CalcDevice::Input r;
  r.value = value;
  r.unit = unit;
  r.msecs = now;
  r.valid = valid;
  return r;
}

// runs the 30-byte line through DecoderAscii exactly like FrameReader does
static std::optional<DmmDecoder::DmmResponse> decode(const QByteArray &line)
{
  auto decoder = DmmDecoder::getInstance(FrameFormat::Sigrok);
  if (!decoder || line.size() != int(decoder->getPacketLength()))
    return std::nullopt;
  if (!decoder->checkFormat(line.constData(), line.size() - 1))
    return std::nullopt;
  return decoder->decode(line, 0);
}

int main(int argc, char **argv)
{
  QCoreApplication app(argc, argv);

  // the other meters' main values by name (DeviceSession::calcInputs())
  QMap<QString, CalcDevice::Input> in;
  const CalcDevice::Inputs p = [&in] { return in; };

  DmmDecoder::DMMInfo info;
  info.protocol = FrameFormat::Sigrok;
  info.display = 40000;
  const qint64 now = QDateTime::currentMSecsSinceEpoch();

  // --- 1. the device registered itself as a model ---
  {
    bool found = false;
    for (const auto &cfg : DmmDecoder::getDeviceConfigurations())
      if (cfg.vendor == "QtDMM" && cfg.model == "Calculated value" && cfg.protocol == FrameFormat::Sigrok)
        found = true;
    check(found, "QtDMM / Calculated value is a registered model with the Sigrok protocol");
  }

  // --- 2. open() rejects bad input with a message ---
  {
    CalcDevice bad(info, "W u * ", p);
    check(!bad.open(QIODevice::ReadWrite), "unparsable formula does not open");
    check(bad.errorString().contains("osition"), "error names the position: " + bad.errorString());
    CalcDevice noUnit(info, "", p);
    check(!noUnit.open(QIODevice::ReadWrite), "empty device string does not open");
    // without inputs: a formula of t alone has a result, a variable waits
    CalcDevice gen0(info, "V 2 + t", CalcDevice::Inputs());
    QString st;
    check(gen0.open(QIODevice::ReadWrite) && decode(gen0.currentLine(0, &st)).has_value() && st.isEmpty(),
          "no inputs: a formula without variables opens and has a result: " + st);
    CalcDevice none(info, "W u", CalcDevice::Inputs());
    check(none.open(QIODevice::ReadWrite) && none.currentLine(0, &st).contains("inf") && st.contains("'u'"),
          "no inputs: a variable waits: " + st);
  }

  // --- 3. P = U * I through the decoder ---
  CalcDevice calc(info, "W u * i", p);
  const bool opened = calc.open(QIODevice::ReadWrite);
  check(opened, "formula opens: " + calc.errorString());   // errorString() after open(): MSVC
                                                          // evaluates arguments right to left
  check(calc.unit() == "W" && calc.formula() == "u * i", "unit and formula split off the device string");

  QString status;
  QByteArray line = calc.currentLine(now, &status);
  check(line.size() == 30 && line.endsWith('\n'), "line is 30 bytes ending in LF");
  check(status.contains("'u'") || status.contains("'i'"), "nothing published yet -> waiting message: " + status);
  auto r = decode(line);
  check(r && r->main().val.trimmed() == "OL", "no inputs -> OL");

  in["u"] = reading(12.0, "V", now);
  in["i"] = reading(0.5, "A", now);
  line = calc.currentLine(now, &status);
  r = decode(line);
  check(status.isEmpty(), "all inputs present -> no status message, got: " + status);
  check(r.has_value(), "line decodes: " + QString::fromLatin1(line.trimmed()));
  if (r)
  {
    check(qFuzzyCompare(r->main().dval, 6.0), QString("dval %1, expected 6").arg(r->main().dval));
    check(r->main().unit == "W", "unit W, got " + r->main().unit);
    check(r->main().special == "DC" && r->range == "AUTO", "coupling DC, range AUTO");
  }

  // small results get an SI prefix and still decode to base units
  in["i"] = reading(0.0005, "A", now + 1);
  r = decode(calc.currentLine(now, nullptr));
  check(r && std::fabs(r->main().dval - 0.006) < 1e-9 && r->main().unit == "mW", QString("6 mW: dval %1 unit %2").arg(r ? r->main().dval : -1).arg(r ? r->main().unit : "-"));

  // units of their own are no prefix: 415 ppm is not 415 p·pm (R14-04)
  for (const auto &[text, value] : { std::pair{ "P1: 415 ppm AUTO", 415.0 }, { "P1: 5 min AUTO", 5.0 },
                                     { "P1: 3.5 m/s AUTO", 3.5 }, { "P1: 2.5 KOhm AUTO", 2500.0 }, { "P1: 12 mV AUTO", 0.012 } })
  {
    const auto u = decode(QByteArray(text).rightJustified(29, ' ') + '\n');
    check(u && std::fabs(u->main().dval - value) < 1e-9 * value, QString("%1: dval %2").arg(text).arg(u ? u->main().dval : -1));
  }

  // --- 4. stale and invalid inputs ---
  r = decode(calc.currentLine(now + CalcDevice::kStaleMs + 1, &status));
  check(r && r->main().val.trimmed() == "OL" && status.contains("No current value"), "stale input -> OL + message: " + status);

  in["i"] = reading(0, "A", now + 2, false);
  r = decode(calc.currentLine(now + 2, &status));
  check(r && r->main().val.trimmed() == "OL" && status.contains("'i'") && status.contains("numeric"), "overloaded input -> OL + message: " + status);

  // --- 5. division by zero, and an instance id with a hyphen ---
  in["i"] = reading(0.0, "A", now + 3, true);
  CalcDevice div(info, "Ohm u / i", p);
  check(div.open(QIODevice::ReadWrite), "u / i opens");
  r = decode(div.currentLine(now + 3, &status));
  check(r && r->main().val.trimmed() == "OL" && status.contains("division"), "1/0 -> OL + message: " + status);

  in["uni-t_803"] = reading(3.0, "V", now + 3);
  CalcDevice alias(info, "V uni_t_803 * 2", p);
  check(alias.open(QIODevice::ReadWrite), "aliased id opens");
  r = decode(alias.currentLine(now + 3, &status));
  check(r && qFuzzyCompare(r->main().dval, 6.0) && status.isEmpty(), "uni-t_803 is reachable as uni_t_803");

  // --- 5a. rounding to the display's counts, like an autoranging meter ---
  {
    QString pre;
    check(CalcDevice::formatValue(3.62423, 40000, &pre) == "3.6242" && pre.isEmpty(), "4 range: 3.6242");
    check(CalcDevice::formatValue(6.6242363, 40000, &pre) == "6.624", "40 range: 6.624");
    check(CalcDevice::formatValue(9.88170524, 40000, &pre) == "9.882", "40 range: 9.882");
    check(CalcDevice::formatValue(3.99996, 40000, &pre) == "4.000", "rounded up into the next range: 4.000");
    check(CalcDevice::formatValue(123.456789, 40000, &pre) == "123.46", "400 range: 123.46");
    check(CalcDevice::formatValue(0.0012345678, 40000, &pre) == "1.2346" && pre == "m", "prefix then round: 1.2346 m");
    check(CalcDevice::formatValue(6.0, 400000, &pre) == "6.0000", "400000 counts, 40 range: 6.0000");
    check(CalcDevice::formatValue(0.0, 40000, &pre) == "0.0000", "zero keeps decimals");
    check(CalcDevice::formatValue(-2.5, 4000, &pre) == "-2.500", "negative, 4000 counts");
  }

  // --- 5b. time and coupling: a signal generator needs no other instance ---
  {
    CalcDevice gen(info, "V/AC 10 + t", p);
    check(gen.open(QIODevice::ReadWrite), "generator opens");
    check(gen.unit() == "V" && gen.coupling() == "AC", "unit/coupling split: " + gen.unit() + "/" + gen.coupling());
    // t counts from open(); currentLine() takes the reference time
    const qint64 opened = QDateTime::currentMSecsSinceEpoch();
    r = decode(gen.currentLine(opened + 2500, &status));
    check(r && status.isEmpty() && r->main().special == "AC", "AC coupling in the line");
    check(r && std::fabs(r->main().dval - 12.5) < 0.01, QString("t = 2.5 s -> 12.5, got %1").arg(r ? r->main().dval : -1));
    CalcDevice rnd(info, "V 5 + rand()", p);
    check(rnd.open(QIODevice::ReadWrite), "rand() opens");
    r = decode(rnd.currentLine(opened, nullptr));
    // rand() is below 1, but the line has the meter's digits: 5.9997 comes as 6.000
    check(r && r->main().dval >= 5.0 && r->main().dval <= 6.0,
          QString("rand() based reading in range, got %1").arg(r ? r->main().dval : -1));
  }

  // --- 5c. several values: a line each, P1 ... P4, value n can use v1 ... v(n-1) ---
  {
    CalcDevice multi(info, "V/DC 10 + t; A/DC 0.5; W v1 * v2; °C/DC 21", p);
    check(multi.open(QIODevice::ReadWrite) && multi.valueCount() == 4, "four values open");
    check(multi.unit(1) == "A" && multi.unit(3) == "°C" && multi.formula(2) == "v1 * v2", "the values split at ';'");
    const qint64 opened = QDateTime::currentMSecsSinceEpoch();
    const QByteArray lines = multi.currentLine(opened + 2000, &status);
    check(lines.size() == 4 * 30 && status.isEmpty(), QString("four 30-byte lines, got %1 bytes").arg(lines.size()));
    QList<std::optional<DmmDecoder::DmmResponse>> rs;
    for (int n = 0; n < 4; ++n)
      rs << decode(lines.mid(n * 30, 30));
    check(rs[0] && rs[0]->main().id == 0 && std::fabs(rs[0]->main().dval - 12) < 0.01, "P1: the main value, 12 V");
    check(rs[1] && rs[1]->main().id == 1 && rs[1]->main().unit == "mA", "P2: value 2, 500 mA");
    check(rs[2] && rs[2]->main().id == 2 && std::fabs(rs[2]->main().dval - 6) < 0.01 && rs[2]->main().unit == "W",
          QString("P3: v1 * v2 = 6 W, got %1").arg(rs[2] ? rs[2]->main().dval : -1));
    check(rs[3] && rs[3]->main().id == 3 && rs[3]->main().unit == "C", "P4: °C");

    // a value can use only those before it
    CalcDevice later(info, "V v2; A 1", p);
    check(!later.open(QIODevice::ReadWrite) && later.errorString().contains("v2"), "v2 in value 1: " + later.errorString());
    CalcDevice noUnit2(info, "V 1; 2", p);
    check(!noUnit2.open(QIODevice::ReadWrite) && noUnit2.errorString().startsWith("Value 2"),
          "a value without unit says which: " + noUnit2.errorString());
    // a value without result leaves the ones after it without one, OL
    CalcDevice waits(info, "V nosuchinstance; W v1 * 2", p);
    check(waits.open(QIODevice::ReadWrite), "an input instance missing opens");
    const QByteArray ol = waits.currentLine(opened, &status);
    const auto w2 = decode(ol.mid(30, 30));
    check(w2 && w2->main().val.contains("OL") && status.contains("nosuchinstance"), "missing input: OL after it too");
    // sigrok channels past four are left out, not mixed into the main value
    check(!decode(QByteArray(29 - 13, ' ') + "P5: 1.00 V DC\n"), "P5: no value");
  }

  // --- 6. the QIODevice side: tick() offers the line, readData hands it out bytewise ---
  {
    int emitted = 0;
    QObject::connect(&calc, &QIODevice::readyRead, [&]{ emitted++; });
    QString lastStatus = "unset";
    QObject::connect(&calc, &CalcDevice::status, [&](const QString &s){ lastStatus = s; });
    // wait for two polls
    QDateTime until = QDateTime::currentDateTime().addMSecs(CalcDevice::kIntervalMs * 2 + 100);
    while (QDateTime::currentDateTime() < until)
      app.processEvents();
    check(emitted >= 1, "readyRead after a tick");
    check(calc.bytesAvailable() == 30, QString("30 bytes available, got %1").arg(calc.bytesAvailable()));
    QByteArray got;
    char c;
    while (calc.read(&c, 1) == 1)
      got.append(c);
    check(got.size() == 30 && got.endsWith('\n'), "bytewise read returns the whole line");
    check(lastStatus != "unset", "status() was emitted");
    calc.close();
    check(!calc.isOpen() && calc.bytesAvailable() == 0, "close clears");
  }

  if (failed == 0)
    qInfo() << "All calc device tests passed.";
  else
    qWarning() << failed << "calc device test(s) failed.";
  return failed == 0 ? 0 : 1;
}
