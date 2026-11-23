// Copyright (c) 2026 The QtDMM developers
// SPDX-License-Identifier: GPL-3.0-or-later

#include "core/calcexpr.h"
#include "core/siprefix.h"

#include <QCoreApplication>
#include <QRandomGenerator>
#include <cmath>

// Recursive descent over the source string. Precedence, loosest first:
//   expr   := term (('+' | '-') term)*
//   term   := unary (('*' | '/') unary)*
//   unary  := '-' unary | power
//   power  := atom ('^' unary)?
//   atom   := number | name | name '(' expr (',' expr)* ')' | '(' expr ')'
struct CalcExpr::Parser
{
  const QString &s;
  int pos = 0;
  int depth = 0;   ///< nesting of unary()/atom(), bounded so a pasted "((((..." cannot blow the stack
  static constexpr int kMaxDepth = 200;
  CalcExpr &e;
  QString error;
  int errorPos = -1;

  Parser(const QString &src, CalcExpr &expr) : s(src), e(expr) {}

  bool failed() const { return errorPos >= 0; }
  int fail(const QString &msg, int at)
  {
    if (!failed())
    {
      error = msg;
      errorPos = at;
    }
    return -1;
  }

  void skipSpace()
  {
    while (pos < s.size() && s[pos].isSpace())
      pos++;
  }
  bool peek(QChar c)
  {
    skipSpace();
    return pos < s.size() && s[pos] == c;
  }
  bool accept(QChar c)
  {
    if (!peek(c))
      return false;
    pos++;
    return true;
  }

  int add(Node n)
  {
    e.m_nodes.push_back(n);
    return static_cast<int>(e.m_nodes.size()) - 1;
  }

  int expr()
  {
    int left = term();
    while (!failed())
    {
      if (accept('+'))      left = add({Op::Add, 0, -1, left, term()});
      else if (accept('-')) left = add({Op::Sub, 0, -1, left, term()});
      else break;
    }
    return left;
  }

  int term()
  {
    int left = unary();
    while (!failed())
    {
      if (accept('*'))      left = add({Op::Mul, 0, -1, left, unary()});
      else if (accept('/')) left = add({Op::Div, 0, -1, left, unary()});
      else break;
    }
    return left;
  }

  // every recursion into the grammar passes through unary(); one counter
  // there bounds the parse (and thereby the eval) depth
  struct DepthGuard
  {
    int &d;
    explicit DepthGuard(int &depth) : d(depth) { ++d; }
    ~DepthGuard() { --d; }
  };

  int unary()
  {
    DepthGuard guard(depth);
    if (depth > kMaxDepth)
      return fail(QCoreApplication::translate("CalcExpr", "Expression too deeply nested"), pos);
    if (accept('-'))
      return add({Op::Neg, 0, -1, unary(), -1});
    accept('+');
    return power();
  }

  int power()
  {
    int base = atom();
    if (!failed() && accept('^'))
      return add({Op::Pow, 0, -1, base, unary()});
    return base;
  }

  static bool isNameStart(QChar c) { return c.isLetter() || c == '_'; }
  static bool isNameChar(QChar c) { return c.isLetterOrNumber() || c == '_'; }
  /// After a name at @p pos: past ".<digits>", a value of it ("v1.2");
  /// @p pos when none follows.
  static int valueNumber(const QString &text, int pos)
  {
    if (pos + 1 < text.size() && text[pos] == '.' && text[pos + 1].isDigit())
    {
      pos += 2;
      while (pos < text.size() && text[pos].isDigit())
        pos++;
    }
    return pos;
  }

  int atom()
  {
    skipSpace();
    if (pos >= s.size())
      return fail(QCoreApplication::translate("CalcExpr", "Unexpected end of expression"), pos);

    const QChar c = s[pos];
    if (c == '(')
    {
      pos++;
      int inner = expr();
      if (!failed() && !accept(')'))
        return fail(QCoreApplication::translate("CalcExpr", "Missing ')'"), pos);
      return inner;
    }
    if (c.isDigit() || c == '.')
      return number();
    if (isNameStart(c))
      return name();
    return fail(QCoreApplication::translate("CalcExpr", "Unexpected '%1'").arg(c), pos);
  }

  int number()
  {
    const int start = pos;
    while (pos < s.size() && (s[pos].isDigit() || s[pos] == '.'))
      pos++;
    // exponent: "1e-3", but only when digits follow, so "2e" is a number
    // followed by a bad suffix rather than a broken exponent
    if (pos < s.size() && (s[pos] == 'e' || s[pos] == 'E'))
    {
      int p = pos + 1;
      if (p < s.size() && (s[p] == '+' || s[p] == '-'))
        p++;
      if (p < s.size() && s[p].isDigit())
      {
        pos = p;
        while (pos < s.size() && s[pos].isDigit())
          pos++;
      }
    }
    bool ok = false;
    double v = s.mid(start, pos - start).toDouble(&ok);
    if (!ok)
      return fail(QCoreApplication::translate("CalcExpr", "Bad number '%1'").arg(s.mid(start, pos - start)), start);

    // SI suffix directly after the digits: 1.5k, 22u, 4n7 is not supported
    if (pos < s.size() && isNameStart(s[pos]))
    {
      const QString suffix = s.mid(pos, 1);
      const double factor = SiPrefix::factor(suffix);
      if (factor == 1.0 && suffix != "u")
        return fail(QCoreApplication::translate("CalcExpr", "Unknown unit prefix '%1'").arg(suffix), pos);
      if (pos + 1 < s.size() && isNameChar(s[pos + 1]))
        return fail(QCoreApplication::translate("CalcExpr", "Unknown unit prefix '%1'").arg(s.mid(pos, 2)), pos);
      v *= factor;
      pos++;
    }
    return add({Op::Num, v, -1, -1, -1});
  }

  int name()
  {
    const int start = pos;
    while (pos < s.size() && isNameChar(s[pos]))
      pos++;
    pos = valueNumber(s, pos);
    const QString id = s.mid(start, pos - start);

    if (!accept('('))
    {
      if (id == "pi")
        return add({Op::Num, M_PI, -1, -1, -1});
      int idx = e.m_variables.indexOf(id);
      if (idx < 0)
      {
        e.m_variables << id;
        idx = e.m_variables.size() - 1;
      }
      return add({Op::Var, 0, idx, -1, -1});
    }

    // function call
    Op op;
    int arity = 1;
    if (id == "sqrt")       op = Op::Sqrt;
    else if (id == "abs")   op = Op::Abs;
    else if (id == "log10") op = Op::Log10;
    else if (id == "ln")    op = Op::Ln;
    else if (id == "sin")   op = Op::Sin;
    else if (id == "cos")   op = Op::Cos;
    else if (id == "exp")   op = Op::Exp;
    else if (id == "floor") op = Op::Floor;
    else if (id == "min")   { op = Op::Min; arity = 2; }
    else if (id == "max")   { op = Op::Max; arity = 2; }
    else if (id == "rand")  { op = Op::Rand; arity = 0; }
    else
      return fail(QCoreApplication::translate("CalcExpr", "Unknown function '%1'").arg(id), start);

    if (arity == 0)
    {
      if (!accept(')'))
        return fail(QCoreApplication::translate("CalcExpr", "'%1' takes no arguments").arg(id), pos);
      return add({op, 0, -1, -1, -1});
    }

    int a = expr();
    int b = -1;
    if (!failed() && arity == 2)
    {
      if (!accept(','))
        return fail(QCoreApplication::translate("CalcExpr", "'%1' needs two arguments").arg(id), pos);
      b = expr();
    }
    if (!failed() && !accept(')'))
      return fail(QCoreApplication::translate("CalcExpr", "Missing ')'"), pos);
    return add({op, 0, -1, a, b});
  }
};

QString CalcExpr::renameVariable(const QString &text, const QString &from, const QString &to)
{
  QString out;
  int pos = 0;
  while (pos < text.size())
  {
    const QChar c = text[pos];
    if (c.isDigit() || c == '.')
    {
      // a number with its exponent and SI suffix is one token
      const int start = pos;
      while (pos < text.size() && (Parser::isNameChar(text[pos]) || text[pos] == '.'
                                   || ((text[pos] == '+' || text[pos] == '-') && pos > start
                                       && (text[pos - 1] == 'e' || text[pos - 1] == 'E'))))
        ++pos;
      out += text.mid(start, pos - start);
    }
    else if (Parser::isNameStart(c))
    {
      const int start = pos;
      while (pos < text.size() && Parser::isNameChar(text[pos]))
        ++pos;
      pos = Parser::valueNumber(text, pos);
      const QString name = text.mid(start, pos - start);
      int next = pos;
      while (next < text.size() && text[next].isSpace())
        ++next;
      const bool function = next < text.size() && text[next] == '(';
      out += name == from && !function ? to : name;
    }
    else
      out += text[pos++];
  }
  return out;
}

// The waveforms as formulas over t (seconds since connecting). Kept as
// plain text so the Custom choice can start from any of them.
QString CalcExpr::waveformFormula(Waveform waveform, const QString &min, const QString &max,
                                 const QString &period, const QString &noise)
{
  const QString lo = "(" + min.trimmed() + ")";
  const QString hi = "(" + max.trimmed() + ")";
  const QString span = "(" + hi + " - " + lo + ")";
  const QString P = "(" + period.trimmed() + ")";
  QString f;
  switch (waveform)
  {
    case Constant: f = hi; break;                                                          // Constant
    case Random: f = lo + " + " + span + " * rand()"; break;                             // Random
    case Sine: f = lo + " + " + span + " * (0.5 + 0.5 * sin(2 * pi * t / " + P + "))"; break;
    case Triangle: f = lo + " + " + span + " * abs(2 * (t / " + P + " - floor(t / " + P + " + 0.5)))"; break;
    case Square: f = lo + " + " + span + " * (floor(2 * t / " + P + ") - 2 * floor(t / " + P + "))"; break;
    case Sawtooth: f = lo + " + " + span + " * (t / " + P + " - floor(t / " + P + "))"; break;
    case Discharge: f = lo + " + " + span + " * exp(-t / " + P + ")"; break;               // Discharge
    default: return QString();                                                      // Custom
  }
  const double n = noise.trimmed().toDouble();
  if (n > 0)
    f += " + " + noise.trimmed() + " * (rand() - 0.5)";
  return f;
}

std::optional<CalcExpr> CalcExpr::parse(const QString &text, QString *error, int *errorPos)
{
  CalcExpr e;
  e.m_text = text.trimmed();
  Parser p(e.m_text, e);

  if (e.m_text.isEmpty())
    p.fail(QCoreApplication::translate("CalcExpr", "Empty expression"), 0);
  else
  {
    e.m_root = p.expr();
    p.skipSpace();
    if (!p.failed() && p.pos < e.m_text.size())
      p.fail(QCoreApplication::translate("CalcExpr", "Unexpected '%1'").arg(e.m_text[p.pos]), p.pos);
  }

  if (p.failed())
  {
    if (error)
      *error = p.error;
    if (errorPos)
      *errorPos = p.errorPos;
    return std::nullopt;
  }
  return e;
}

std::optional<double> CalcExpr::eval(const QMap<QString, double> &values) const
{
  if (m_root < 0)
    return std::nullopt;
  bool ok = true;
  const double v = evalNode(m_root, values, ok);
  if (!ok || !std::isfinite(v))
    return std::nullopt;
  return v;
}

double CalcExpr::evalNode(int idx, const QMap<QString, double> &values, bool &ok) const
{
  const Node &n = m_nodes[idx];
  auto A = [&]{ return evalNode(n.a, values, ok); };
  auto B = [&]{ return evalNode(n.b, values, ok); };
  switch (n.op)
  {
    case Op::Num:   return n.value;
    case Op::Var:
    {
      auto it = values.constFind(m_variables[n.name]);
      if (it == values.constEnd())
      {
        ok = false;
        return 0;
      }
      return it.value();
    }
    case Op::Neg:   return -A();
    case Op::Add:   return A() + B();
    case Op::Sub:   return A() - B();
    case Op::Mul:   return A() * B();
    case Op::Div:   return A() / B();
    case Op::Pow:   return std::pow(A(), B());
    case Op::Sqrt:  return std::sqrt(A());
    case Op::Abs:   return std::fabs(A());
    case Op::Log10: return std::log10(A());
    case Op::Ln:    return std::log(A());
    case Op::Sin:   return std::sin(A());
    case Op::Cos:   return std::cos(A());
    case Op::Exp:   return std::exp(A());
    case Op::Floor: return std::floor(A());
    case Op::Min:   return std::min(A(), B());
    case Op::Max:   return std::max(A(), B());
    case Op::Rand:  return QRandomGenerator::global()->generateDouble();
  }
  ok = false;
  return 0;
}
