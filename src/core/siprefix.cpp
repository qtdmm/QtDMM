#include "core/siprefix.h"

#include <cmath>
#include <QRegularExpression>

namespace
{
struct Entry
{
  const char *prefix;
  double factor;
};

// Largest first: scale() walks this top-down and stops at the first factor
// the magnitude reaches.
const Entry kTable[] = {
  {"T", 1e12},
  {"G", 1e9},
  {"M", 1e6},
  {"k", 1e3},
  {"",  1.0},
  {"m", 1e-3},
  {"µ", 1e-6},
  {"n", 1e-9},
  {"p", 1e-12},
};

bool isPrefix(const QString &s)
{
  if (s == "u")
    return true;
  for (const Entry &e : kTable)
    if (*e.prefix && s == QString::fromUtf8(e.prefix))
      return true;
  return false;
}
}

namespace SiPrefix
{

double factor(const QString &prefix)
{
  const QString p = prefix == "u" ? QStringLiteral("µ") : prefix;
  for (const Entry &e : kTable)
    if (p == QString::fromUtf8(e.prefix))
      return e.factor;
  return 1.0;
}

Split split(const QString &unit)
{
  // "ppm" (CO2 sensors) is a unit of its own, not pico-"pm"
  if (unit == QLatin1String("ppm"))
    return {QString(), unit};
  if (unit.size() > 1 && isPrefix(unit.left(1)))
    return {unit.left(1), unit.mid(1)};
  return {QString(), unit};
}

double scale(double value, QString *prefixOut)
{
  const Entry *selected = &kTable[4]; // ""
  const double magnitude = std::fabs(value);

  if (magnitude > 0.0)
  {
    for (const Entry &e : kTable)
    {
      if (magnitude >= e.factor)
      {
        selected = &e;
        break;
      }
    }
  }

  if (prefixOut)
    *prefixOut = QString::fromUtf8(selected->prefix);
  return value / selected->factor;
}

QString format(double value, QString *prefixOut)
{
  return QString::number(scale(value, prefixOut), 'g', 12);
}

QString withoutLeadingZeros(const QString &text)
{
  static const QRegularExpression leadingZeros("^([-+]?)0+(?=\\d)");
  return text.trimmed().remove(' ').replace(leadingZeros, "\\1");
}

QString displayText(const QString &unit)
{
  const Split parts = split(unit);
  if (parts.baseUnit == QLatin1String("Ohm"))
    return parts.prefix + QStringLiteral("Ω");
  if (parts.baseUnit == QLatin1String("dF"))
    return QStringLiteral("°F");
  if (parts.baseUnit == QLatin1String("C"))
    return QStringLiteral("°C");
  if (parts.baseUnit == QLatin1String("cosphi"))
    return QStringLiteral("cosφ");
  return parts.prefix + parts.baseUnit;
}

}
