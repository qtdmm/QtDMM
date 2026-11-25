// Copyright (c) 2026 The QtDMM developers
// SPDX-License-Identifier: GPL-3.0-or-later
#include "device/scpi/scpi.h"

#include <QRegularExpression>

#include <cmath>
#include <limits>
#include <utility>

namespace
{
constexpr double kOverload = 9.9e37;     // SCPI-99 7.2.1.5: +infinity
constexpr double kNotANumber = 9.91e37;  // SCPI-99 7.2.1.5: not a number

/// @p text starts with the word @p word (any case), not followed by a letter.
bool startsWithWord(QStringView text, QStringView word)
{
  return text.startsWith(word, Qt::CaseInsensitive) && (text.size() == word.size() || !text[word.size()].isLetter());
}

Scpi::Number special(Scpi::Number::Kind kind, double value, QStringView rest)
{
  return { kind, value, rest.trimmed().toString() };
}
}

namespace Scpi
{
Number number(QStringView text)
{
  using Kind = Number::Kind;
  constexpr double inf = std::numeric_limits<double>::infinity();
  const QStringView t = text.trimmed();

  // the words: NAN, INF/NINF (SCPI), OL of the handheld meters
  if (startsWithWord(t, u"NAN"))
    return special(Kind::NotANumber, std::numeric_limits<double>::quiet_NaN(), t.mid(3));
  using Word = std::pair<QStringView, double>;
  for (const auto &[word, value] : { Word { u"NINF", -inf }, Word { u"-INF", -inf }, Word { u"+INF", inf }, Word { u"INF", inf } })
    if (startsWithWord(t, word))
      return special(Kind::Overload, value, t.mid(word.size()));
  static const QRegularExpression ol(QStringLiteral("^([+-]?)OL[.0-9]*"), QRegularExpression::CaseInsensitiveOption);
  if (const QRegularExpressionMatch m = ol.matchView(t); m.hasMatch())
    return special(Kind::Overload, m.capturedView(1) == u"-" ? -inf : inf, t.mid(m.capturedLength()));

  static const QRegularExpression nrf(QStringLiteral("^[+-]?(?:\\d+\\.?\\d*|\\.\\d+)(?:[eE][+-]?\\d+)?"));
  const QRegularExpressionMatch m = nrf.matchView(t);
  if (!m.hasMatch())
    return {};
  bool ok = false;
  const double v = m.capturedView().toDouble(&ok);
  if (!ok)
    return {};
  const QStringView rest = t.mid(m.capturedLength());
  if (std::fabs(v - kNotANumber) < kNotANumber * 1e-6)
    return special(Kind::NotANumber, std::numeric_limits<double>::quiet_NaN(), rest);
  if (std::fabs(v) >= kOverload * (1 - 1e-6))
    return special(Kind::Overload, v < 0 ? -inf : inf, rest);
  return special(Kind::Value, v, rest);
}

QStringList list(QStringView response)
{
  QStringList out;
  if (response.trimmed().isEmpty())
    return out;
  QChar quote;
  qsizetype start = 0;
  for (qsizetype i = 0; i < response.size(); ++i)
  {
    const QChar c = response[i];
    if (!quote.isNull())
    {
      if (c == quote)
        quote = QChar();   // a doubled quote closes and opens again
    }
    else if (c == u'"' || c == u'\'')
      quote = c;
    else if (c == u',')
    {
      out << response.mid(start, i - start).trimmed().toString();
      start = i + 1;
    }
  }
  out << response.mid(start).trimmed().toString();
  return out;
}

QString unquote(QStringView text)
{
  const QStringView t = text.trimmed();
  if (t.size() < 2 || (t.front() != u'"' && t.front() != u'\'') || t.back() != t.front())
    return t.toString();
  const QChar q = t.front();
  QString out;
  out.reserve(t.size() - 2);
  for (qsizetype i = 1; i < t.size() - 1; ++i)
  {
    out += t[i];
    if (t[i] == q && t[i + 1] == q)
      ++i;   // "" inside is one "
  }
  return out;
}

Identity identity(QStringView response)
{
  const QStringList f = list(response);
  if (f.size() < 2)
    return {};
  Identity id { unquote(f[0]), unquote(f[1]), {}, {} };
  if (f.size() > 2)
    id.serial = unquote(f[2]);
  if (f.size() > 3)
    id.firmware = f.mid(3).join(u',');   // some put commas into it
  return id;
}

Error error(QStringView response)
{
  const qsizetype comma = response.indexOf(u',');
  bool ok = false;
  const int code = (comma < 0 ? response : response.left(comma)).trimmed().toInt(&ok);
  if (!ok)
    return { 0, response.trimmed().toString() };
  return { code, comma < 0 ? QString() : unquote(response.mid(comma + 1)) };
}

bool isQuery(QByteArrayView line)
{
  char quote = 0;
  bool inHeader = true;   // a program message unit starts with its header
  bool started = false;   // the header's first character was seen
  char last = 0;
  for (const char c : line)
  {
    if (quote != 0)
    {
      if (c == quote)
        quote = 0;
      continue;
    }
    const bool space = c == ' ' || c == '\t' || c == '\r' || c == '\n';
    if (c == ';')
    {
      if (inHeader && last == '?')
        return true;
      inHeader = true;
      started = false;
      last = 0;
    }
    else if (inHeader)
    {
      if (space && started)
      {
        if (last == '?')
          return true;
        inHeader = false;
      }
      else if (!space)
      {
        started = true;
        last = c;
      }
    }
    else if (c == '"' || c == '\'')
      quote = c;
  }
  return inHeader && last == '?';
}

QByteArray format(double value)
{
  if (std::isnan(value))
    return QByteArrayLiteral("9.91E37");
  if (std::isinf(value))
    return value < 0 ? QByteArrayLiteral("-9.9E37") : QByteArrayLiteral("9.9E37");
  return QByteArray::number(value, 'g', 12);
}
}
