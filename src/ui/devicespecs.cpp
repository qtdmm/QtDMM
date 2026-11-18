// Copyright (c) 2026 The QtDMM developers
// SPDX-License-Identifier: GPL-3.0-or-later
#include "ui/devicespecs.h"

#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QHash>
#include <QJsonArray>
#include <QJsonDocument>
#include <QRegularExpression>
#include <cmath>

#include "core/reading.h"
#include "core/siprefix.h"

namespace DeviceSpecs
{
namespace
{
/// "<vendor> <model>" -> its data, from all files in the resources
const QHash<QString, QJsonObject> &all()
{
  static const QHash<QString, QJsonObject> specs = []
  {
    QHash<QString, QJsonObject> map;
    const QDir dir(QStringLiteral(":/specs"));
    for (const QString &name : dir.entryList({ QStringLiteral("*.json") }, QDir::Files))
    {
      QFile file(dir.filePath(name));
      if (!file.open(QIODevice::ReadOnly))
        continue;
      const QJsonObject spec = QJsonDocument::fromJson(file.readAll()).object();
      for (const QJsonValue &model : spec.value("models").toArray())
        map.insert(model.toString(), spec);
    }
    return map;
  }();
  return specs;
}

QString esc(const QJsonValue &v)
{
  return v.toString().toHtmlEscaped();
}

// what a table heading measures, from its words: the manuals name them
// freely ("DC Voltage", "B. AC Voltage", "Resistance(Ω)", "I. Frequency",
// "Diode and Continuity")
enum Coupling { Any, AC, DC, ACDC };
struct Kind
{
  QList<Quantity> quantities;
  bool diode = false;
  Coupling coupling = Any;
};

Kind kindOf(const QString &heading)
{
  QString h = heading.toLower();
  h.remove(QRegularExpression(QStringLiteral("\\(.*\\)")));   // "(AC+DC measurement is available)"
  Kind k;
  static const QList<QPair<const char *, Quantity>> words = {
    { "voltage", Quantity::Voltage }, { "current", Quantity::Current }, { "resistance", Quantity::Resistance },
    { "capacitance", Quantity::Capacitance }, { "frequency", Quantity::Frequency }, { "duty", Quantity::DutyCycle },
    { "continuity", Quantity::Continuity }, { "on-off", Quantity::Continuity }, { "temperature", Quantity::Temperature },
    { "illuminance", Quantity::Illuminance }, { "wind speed", Quantity::WindSpeed }, { "transistor", Quantity::Gain },
    { "sound level", Quantity::SoundPressureLevel }, { "humidity", Quantity::RelativeHumidity },
  };
  for (const auto &w : words)
    if (h.contains(QLatin1String(w.first)))
      k.quantities << w.second;
  k.diode = h.contains(QLatin1String("diode"));
  if (h.contains(QLatin1String("ac+dc")))
    k.coupling = ACDC;
  else if (h.contains(QRegularExpression(QStringLiteral("\\bac\\b"))))
    k.coupling = AC;
  else if (h.contains(QRegularExpression(QStringLiteral("\\bdc\\b"))))
    k.coupling = DC;
  return k;
}

bool fits(const Kind &k, const Reading &r)
{
  if (r.diode())
    return k.diode;
  if (!k.quantities.contains(r.port.quantity))
    return false;
  if (k.coupling == Any)
    return true;
  const Coupling c = r.ac() && r.dc() ? ACDC : r.ac() ? AC : r.dc() ? DC : Any;
  return c == k.coupling;
}

// the full scale a range names ("220.00mV" 0.22, "6.000kΩ" 6000), NaN for
// a span or a word ("0~30 m/s", "Diode")
double fullScale(const QString &range)
{
  static const QRegularExpression re(QStringLiteral("^(\\d+(?:\\.\\d+)?)\\s*([nµumkMG]?)(V|A|Ω|F|Hz)"));
  const QRegularExpressionMatch m = re.match(range);
  if (!m.hasMatch())
    return std::nan("");
  QString prefix = m.captured(2);
  if (prefix == QLatin1String("u"))
    prefix = QStringLiteral("µ");
  return m.captured(1).toDouble() * SiPrefix::factor(prefix);
}

// the range a note is about: "600Ω: ..." or "20.00mF range: ...", -1 when
// it is about the whole table
int noteRow(const QString &note, const QJsonArray &ranges)
{
  const int colon = note.indexOf(QLatin1Char(':'));
  if (colon <= 0)
    return -1;
  QString head = note.left(colon).trimmed();
  head.remove(QRegularExpression(QStringLiteral("\\s+range$")));
  for (int i = 0; i < ranges.size(); ++i)
    if (ranges.at(i).toObject().value("range").toString() == head)
      return i;
  return -1;
}
}

QJsonObject find(const QString &vendor, const QString &model)
{
  // "UT61E+ (UT-D07B Bluetooth) *" is the UT61E+
  QString base = model;
  base.remove(QRegularExpression(QStringLiteral("\\s*\\*$")));
  base.remove(QRegularExpression(QStringLiteral("\\s*\\(.*\\)$")));
  return all().value(vendor + ' ' + base);
}

bool knowsHeading(const QString &heading)
{
  const Kind k = kindOf(heading);
  return !k.quantities.isEmpty() || k.diode;
}

Match match(const QJsonObject &spec, const Reading &reading)
{
  Match first;
  const QJsonArray tables = spec.value("tables").toArray();
  const double full = reading.measuringRange.full;
  for (int t = 0; t < tables.size(); ++t)
  {
    const QJsonObject table = tables.at(t).toObject();
    if (!fits(kindOf(table.value("table").toString()), reading))
      continue;
    const QJsonArray ranges = table.value("ranges").toArray();
    for (int i = 0; i < ranges.size(); ++i)
    {
      const QString range = ranges.at(i).toObject().value("range").toString();
      // diode and continuity share a table, their rows named so
      if (reading.diode() && range.contains(QLatin1String("diode"), Qt::CaseInsensitive))
        return { t, i };
      if (reading.continuity() && range.contains(QLatin1String("continuity"), Qt::CaseInsensitive))
        return { t, i };
      const double named = fullScale(range);
      if (std::isfinite(full) && std::isfinite(named) && std::abs(named - full) <= 1e-6 * std::abs(full))
        return { t, i };
    }
    if (!first.isValid())
      first.table = t;
  }
  return first;
}

QString html(const QJsonObject &spec, const Match &current, const QColor &mark)
{
  QString out;
  QString last;
  const QString markStyle = mark.isValid() ? QString(" style=\"background-color:%1\"").arg(mark.name()) : QString();
  const QJsonArray tables = spec.value("tables").toArray();
  for (int t = 0; t < tables.size(); ++t)
  {
    const QJsonObject table = tables.at(t).toObject();
    const bool here = current.table == t;
    if (here)
      out += QStringLiteral("<a name=\"current\"></a>");
    // a function split into several tables (other input impedance or fuse)
    // has its heading once
    const QString name = table.value("table").toString();
    if (name != last || here)
    {
      out += QString("<h3%1>%2</h3>").arg(here ? markStyle : QString(), name.toHtmlEscaped());
      last = name;
    }
    QStringList facts;
    if (table.value("input_impedance").isString())
      facts << QCoreApplication::translate("DeviceSpecs", "Input impedance: %1").arg(esc(table.value("input_impedance")));
    if (table.value("overload_protection").isString())
      facts << QCoreApplication::translate("DeviceSpecs", "Overload protection: %1").arg(esc(table.value("overload_protection")));
    if (table.value("page").isDouble())
      facts << QCoreApplication::translate("DeviceSpecs", "manual p. %1").arg(table.value("page").toInt());
    if (!facts.isEmpty())
      out += QString("<p>%1</p>").arg(facts.join(" &middot; "));
    const QString range = QCoreApplication::translate("DeviceSpecs", "Range");
    const QString resolution = QCoreApplication::translate("DeviceSpecs", "Resolution");
    const QString accuracy = QCoreApplication::translate("DeviceSpecs", "Accuracy ±(% of reading + digits)");
    out += QString("<table border=\"1\" cellspacing=\"0\" cellpadding=\"3\"><tr><th>%1</th><th>%2</th><th>%3</th></tr>")
             .arg(range.toHtmlEscaped(), resolution.toHtmlEscaped(), accuracy.toHtmlEscaped());
    // notes on one range go below it, the others below the table
    const QJsonArray ranges = table.value("ranges").toArray();
    QHash<int, QStringList> rowNotes;
    QStringList notes;
    for (const QJsonValue &n : table.value("notes").toArray())
    {
      const int row = noteRow(n.toString(), ranges);
      if (row >= 0)
        rowNotes[row] << esc(n);
      else
        notes << "<li>" + esc(n) + "</li>";
    }
    for (int i = 0; i < ranges.size(); ++i)
    {
      const QJsonObject row = ranges.at(i).toObject();
      QStringList bands;
      for (const QJsonValue &a : row.value("accuracy").toArray())
      {
        const QJsonObject band = a.toObject();
        bands << (band.value("freq_range").isString() ? esc(band.value("freq_range")) + ": " : QString())
                 + esc(band.value("accuracy"));
      }
      const QString style = here && current.row == i ? markStyle : QString();
      out += QString("<tr%1><td>%2</td><td>%3</td><td>%4</td></tr>")
               .arg(style, esc(row.value("range")), esc(row.value("resolution")), bands.join("<br>"));
      for (const QString &note : rowNotes.value(i))
        out += QString("<tr%1><td colspan=\"3\"><small>%2</small></td></tr>").arg(style, note);
    }
    out += "</table>";
    if (!notes.isEmpty())
      out += "<ul>" + notes.join(QString()) + "</ul>";
  }
  // dmm-tools' transcriptions (update_device_specs.py) or our own from a manual
  const QString source = spec.value("source").toString();
  const QString from = source.startsWith(QLatin1String("dmm-tools"))
    ? QCoreApplication::translate("DeviceSpecs", "From the manual, transcribed by <a href=\"https://github.com/antoinecellerier/dmm-tools\">dmm-tools</a> "
                                  "(%1, GPL-3.0-or-later) - thank you. Check the manual before relying on it.")
    : QCoreApplication::translate("DeviceSpecs", "From the %1. Check the manual before relying on it.");
  out += QString("<p><small>%1</small></p>").arg(from.arg(source.toHtmlEscaped()));
  return out;
}
}
