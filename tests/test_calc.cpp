// Tests for CalcExpr, the formula evaluator of calculated instances.
#include <QCoreApplication>
#include <QDebug>
#include <cmath>

#include "core/calcexpr.h"
#include "testutil.h"

static void expectValue(const QString &text, double expected, const QMap<QString, double> &vars = {})
{
  QString err;
  auto e = CalcExpr::parse(text, &err);
  if (!e)
  {
    check(false, QString("'%1' should parse, got: %2").arg(text, err));
    return;
  }
  auto v = e->eval(vars);
  check(v.has_value(), QString("'%1' should evaluate").arg(text));
  if (v)
    check(std::fabs(*v - expected) <= 1e-9 * std::max(1.0, std::fabs(expected)),
          QString("'%1' = %2, expected %3").arg(text).arg(*v, 0, 'g', 12).arg(expected, 0, 'g', 12));
}

static void expectParseError(const QString &text, int pos = -1)
{
  QString err;
  int at = -1;
  auto e = CalcExpr::parse(text, &err, &at);
  check(!e, QString("'%1' should not parse").arg(text));
  if (!e)
  {
    check(!err.isEmpty(), QString("'%1': error text set").arg(text));
    if (pos >= 0)
      check(at == pos, QString("'%1': error at %2, expected %3 (%4)").arg(text).arg(at).arg(pos).arg(err));
  }
}

static void expectEvalFails(const QString &text, const QMap<QString, double> &vars = {})
{
  auto e = CalcExpr::parse(text);
  check(e.has_value(), QString("'%1' should parse").arg(text));
  if (e)
    check(!e->eval(vars).has_value(), QString("'%1' should not evaluate").arg(text));
}

int main(int argc, char **argv)
{
  QCoreApplication app(argc, argv);

  // --- 1. numbers and precedence ---
  expectValue("42", 42);
  expectValue("1.5", 1.5);
  expectValue(".5", 0.5);
  expectValue("1e3", 1000);
  expectValue("2.5e-3", 0.0025);
  expectValue("2+3*4", 14);
  expectValue("(2+3)*4", 20);
  expectValue("10-4-3", 3);            // left associative
  expectValue("64/4/2", 8);
  expectValue("2^3^2", 512);           // right associative
  expectValue("-2^2", -4);             // unary minus binds weaker than ^
  expectValue("(-2)^2", 4);
  expectValue("2^-1", 0.5);
  expectValue("--3", 3);
  expectValue("+3", 3);
  expectValue(" 1 + 2 ", 3);

  // --- 2. SI suffixes ---
  expectValue("1.5k", 1500);
  expectValue("22u", 22e-6);
  expectValue("22µ", 22e-6);
  expectValue("4.7n", 4.7e-9);
  expectValue("3M", 3e6);
  expectValue("2m*3", 0.006);
  expectValue("1k+1", 1001);

  // --- 3. variables and functions ---
  QMap<QString, double> vars{{"u", 12.0}, {"i", 0.5}, {"r_1", 100}, {"default", 2}};
  expectValue("u*i", 6, vars);
  expectValue("u / i", 24, vars);
  expectValue("u^2/r_1", 1.44, vars);
  expectValue("default*2", 4, vars);
  expectValue("sqrt(u*3)", 6, vars);
  expectValue("abs(-u)", 12, vars);
  expectValue("log10(1000)", 3);
  expectValue("ln(exp(2))", 2);
  // the dew point (Magnus) of 20 °C at 50 %RH, as a formula over a TFA's values
  {
    const double gamma = 17.62 * 20 / (243.12 + 20) + std::log(0.5);
    expectValue("243.12*((17.62*v1.2)/(243.12+v1.2)+ln(v1.3/100))/((17.62*243.12)/(243.12+v1.2)-ln(v1.3/100))",
                243.12 * gamma / (17.62 - gamma), { { "v1.2", 20 }, { "v1.3", 50 } });
  }
  expectValue("min(u, i)", 0.5, vars);
  expectValue("max(u, i*100)", 50, vars);
  expectValue("sqrt(abs(-16))", 4);
  expectValue("sin(pi/2)", 1);
  expectValue("cos(0)", 1);
  expectValue("exp(0)", 1);
  expectValue("floor(2.7)", 2);
  expectValue("floor(-0.5)", -1);
  expectValue("2*pi", 2 * M_PI);
  {
    auto e = CalcExpr::parse("rand()");
    bool inRange = true, varies = false;
    double first = e ? *e->eval({}) : -1;
    for (int i = 0; i < 20 && e; ++i)
    {
      double v = *e->eval({});
      inRange = inRange && v >= 0.0 && v < 1.0;
      varies = varies || v != first;
    }
    check(e && inRange && varies, "rand() is uniform in [0,1) and changes per evaluation");
    e = CalcExpr::parse("pi");
    check(e && e->variables().isEmpty(), "pi is a constant, not a variable");
  }

  {
    auto e = CalcExpr::parse("u * i + u - sqrt(r_1)");
    check(e && e->variables() == QStringList{"u", "i", "r_1"}, "variables() lists each name once, in order");
    e = CalcExpr::parse("2 + 3");
    check(e && e->variables().isEmpty(), "constant expression has no variables");
    e = CalcExpr::parse("  u*i ");
    check(e && e->text() == "u*i", "text() is the trimmed source");
  }

  // --- 3b. the virtual meter's waveforms ---
  {
    auto at = [](CalcExpr::Waveform w, double t, const QString &noise = "0")
    {
      QString err;
      auto e = CalcExpr::parse(CalcExpr::waveformFormula(w, "2", "6", "8", noise), &err);
      check(e.has_value(), QString("waveform %1 parses: %2").arg(int(w)).arg(err));
      auto v = e ? e->eval({{"t", t}}) : std::nullopt;
      check(v.has_value(), QString("waveform %1 evaluates").arg(int(w)));
      return v.value_or(-1e9);
    };
    auto near = [](double a, double b) { return std::fabs(a - b) < 1e-9; };
    check(near(at(CalcExpr::Constant, 0), 6), "constant = max");
    check(near(at(CalcExpr::Sine, 0), 4) && near(at(CalcExpr::Sine, 2), 6) && near(at(CalcExpr::Sine, 6), 2), "sine: mid, max at P/4, min at 3P/4");
    check(near(at(CalcExpr::Triangle, 0), 2) && near(at(CalcExpr::Triangle, 4), 6) && near(at(CalcExpr::Triangle, 2), 4), "triangle: min, max at P/2, mid at P/4");
    check(near(at(CalcExpr::Square, 1), 2) && near(at(CalcExpr::Square, 5), 6) && near(at(CalcExpr::Square, 9), 2), "square: low half, high half, repeats");
    check(near(at(CalcExpr::Sawtooth, 0), 2) && near(at(CalcExpr::Sawtooth, 4), 4) && near(at(CalcExpr::Sawtooth, 8), 2), "sawtooth: ramps min..max, restarts");
    check(near(at(CalcExpr::Discharge, 0), 6) && near(at(CalcExpr::Discharge, 8), 2 + 4 * std::exp(-1.0)), "discharge: max, 1/e at t = P");
    const double r = at(CalcExpr::Random, 0);
    check(r >= 2 && r < 6, "random within min..max");
    const double n = at(CalcExpr::Constant, 0, "0.5");
    check(n >= 5.75 && n <= 6.25, "noise adds at most +-noise/2");
    check(CalcExpr::waveformFormula(CalcExpr::Custom, "0", "1", "1", "0").isEmpty(), "custom has no preset");
  }

  // --- 4. parse errors, with position ---
  expectParseError("", 0);
  // nesting is bounded (a pasted "(((((" must not blow the stack); moderate depth is fine
  expectValue(QString(100, '(') + "1" + QString(100, ')'), 1);
  expectValue(QString(150, '-') + "2", 2);
  expectParseError(QString(20000, '(') + "1" + QString(20000, ')'));
  expectParseError(QString(20000, '-') + "1");
  expectParseError("1e400");   // out of range for a double
  expectEvalFails("2^1000000");
  expectParseError("   ", 0);
  expectParseError("2 +", 3);
  expectParseError("(2+3", 4);
  expectParseError("2+3)", 3);
  expectParseError("2 3", 2);
  expectParseError("foo(2)", 0);
  expectParseError("min(1)", 5);
  expectParseError("sqrt()", 5);
  expectParseError("2 $ 3", 2);
  expectParseError("3x", 1);          // unknown suffix
  expectParseError("3kg", 1);
  expectParseError("*2", 0);
  expectParseError("2**3", 2);
  expectParseError("rand(1)", 5);
  expectParseError("sin()", 4);

  // --- 5. evaluation failures ---
  expectEvalFails("u * i", {{"u", 1}});   // i missing
  expectEvalFails("1/0");
  expectEvalFails("sqrt(-1)");
  expectEvalFails("log10(0)");
  expectEvalFails("u", {});

  if (failed == 0)
    qInfo() << "All calc expression tests passed.";
  else
    qWarning() << failed << "calc expression test(s) failed.";
  return failed == 0 ? 0 : 1;
}
