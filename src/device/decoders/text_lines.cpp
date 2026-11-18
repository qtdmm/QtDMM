// Copyright (c) 2026 The QtDMM developers
// SPDX-License-Identifier: GPL-3.0-or-later
#include "device/decoders/text_lines.h"

#include <QRegularExpression>
#include <cmath>
#include <utility>

#include "core/siprefix.h"

static const bool registered = []() {
  DmmDecoder::addConfig({"Generic", "Microcontroller (text lines: Serial Plotter, Teleplot)", "", 115200,
                         FrameFormat::TextLines, 8, 1, 1, 0, 0, 0, 0, 0, "", "", 4});
  return true;
}();

namespace
{
constexpr int kMaxValues = 4;

// a number as the sketch prints it: sign, digits, point, exponent; no nan/inf
bool number(const QString &s, double *value)
{
  static const QRegularExpression re(QStringLiteral("^[+-]?(\\d+\\.?\\d*|\\.\\d+)([eE][+-]?\\d+)?$"));
  if (!re.match(s).hasMatch())
    return false;
  bool ok = false;
  *value = s.toDouble(&ok);   // C locale: a point, whatever the desktop's language
  return ok && std::isfinite(*value);
}

// a label that is a unit QtDMM knows ("V", "mA", "kOhm", "°C", "%RH"): the
// unit of its value too [E 18.11.]; "C", "K" and other letters alone stay
// names (a channel "C" is no temperature). Empty for any other label.
QString unitOfLabel(const QString &label)
{
  static const QStringList bases = { "V", "A", "Ohm", "F", "Hz", "W", "Wh", "Ah", "VA", "%", "%RH", "lx",
                                     "ppm", "dB", "dBm", "dBA", "dBC", "°C", "°F", "m/s" };
  QString unit = label;
  unit.replace(QChar(0x03A9), QStringLiteral("Ohm")).replace(QChar(0x2126), QStringLiteral("Ohm"));   // Ω, Ω
  SiPrefix::Split split = SiPrefix::split(unit);
  if (!bases.contains(split.baseUnit))
    return QString();
  if (split.prefix == QLatin1String("u"))
    split.prefix = QStringLiteral("µ");
  return split.prefix + split.baseUnit;
}

// the number as QtDMM shows it: as sent, but without "+" and exponent
QString shown(const QString &s, double value)
{
  static const QRegularExpression plain(QStringLiteral("^-?\\d+(\\.\\d+)?$"));
  QString t = s.startsWith(QLatin1Char('+')) ? s.mid(1) : s;
  return plain.match(t).hasMatch() ? t : QString::number(value, 'g', 12);
}

QList<DecoderTextLines::Item> serialPlotter(const QString &line)
{
  static const QRegularExpression sep(QStringLiteral("[,\\s]+"));
  QList<DecoderTextLines::Item> out;
  const QStringList tokens = line.split(sep, Qt::SkipEmptyParts);
  for (int i = 0; i < tokens.size(); ++i)
  {
    const QString &t = tokens.at(i);
    const int colon = t.indexOf(QLatin1Char(':'));
    const QString label = colon >= 0 ? t.left(colon) : QString();
    const QString s = colon >= 0 ? t.mid(colon + 1) : t;
    double v = 0;
    if ((colon >= 0 && (label.isEmpty() || s.contains(QLatin1Char(':')))) || !number(s, &v))
      return {};   // one bad token: the whole line is not one
    out << DecoderTextLines::Item{ label.isEmpty() ? QString::number(i + 1) : label, shown(s, v), v, unitOfLabel(label) };
  }
  return out;
}

QList<DecoderTextLines::Item> teleplot(QString line)
{
  QStringList flags;
  const int bar = line.lastIndexOf(QLatin1Char('|'));
  if (bar >= 0)
  {
    static const QRegularExpression flagList(QStringLiteral("^([A-Za-z0-9_]+(,[A-Za-z0-9_]+)*)?$"));
    const QString f = line.mid(bar + 1);
    if (!flagList.match(f).hasMatch())
      return {};
    flags = f.split(QLatin1Char(','), Qt::SkipEmptyParts);
    line = line.left(bar);
  }
  if (flags.contains(QLatin1String("t")))
    return {};   // a text value
  const int colon = line.indexOf(QLatin1Char(':'));
  if (colon <= 0)
    return {};
  const QString key = line.left(colon);
  QString rest = line.mid(colon + 1);
  QString unit;
  const int section = rest.indexOf(QChar(0x00A7));   // §
  if (section >= 0)
  {
    unit = rest.mid(section + 1).trimmed();
    rest = rest.left(section);
  }
  // several points: all must be numbers, the last counts
  const QStringList points = rest.split(QLatin1Char(';'));
  const bool xy = flags.contains(QLatin1String("xy"));
  QList<DecoderTextLines::Item> out;
  for (const QString &point : points)
  {
    const QStringList f = point.split(QLatin1Char(':'));
    out.clear();
    if (xy)
    {
      double x = 0, y = 0, ts = 0;
      if ((f.size() != 2 && f.size() != 3) || !number(f.at(0), &x) || !number(f.at(1), &y)
          || (f.size() == 3 && !number(f.at(2), &ts)))
        return {};
      out << DecoderTextLines::Item{ key + QStringLiteral(".x"), shown(f.at(0), x), x, QString() }
          << DecoderTextLines::Item{ key + QStringLiteral(".y"), shown(f.at(1), y), y, unit };
    }
    else
    {
      double v = 0, ts = 0;
      if ((f.size() != 1 && f.size() != 2) || !number(f.last(), &v) || (f.size() == 2 && !number(f.first(), &ts)))
        return {};
      out << DecoderTextLines::Item{ key, shown(f.last(), v), v, unit.isEmpty() ? unitOfLabel(key) : unit };
    }
  }
  return out;
}
}

QList<DecoderTextLines::Item> DecoderTextLines::parse(const QString &line)
{
  if (line.isEmpty() || line.contains(QChar::ReplacementCharacter))
    return {};   // a wrong baud rate
  for (const QChar c : line)
    if (c.unicode() < 0x20 && c != QLatin1Char('\t'))
      return {};
  if (line.startsWith(QLatin1Char('>')) || line.startsWith(QLatin1String("3D|")))
    return {};   // Teleplot's log and 3D lines
  // only Teleplot has a unit, flags, several points or a time stamp
  const int colon = line.indexOf(QLatin1Char(':'));
  const bool teleplotOnly = line.contains(QChar(0x00A7)) || line.contains(QLatin1Char('|'))
                            || line.contains(QLatin1Char(';'))
                            || (colon > 0 && line.indexOf(QLatin1Char(':'), colon + 1) > 0
                                && !line.contains(QRegularExpression(QStringLiteral("[,\\s]"))));
  return teleplotOnly ? teleplot(line) : serialPlotter(line);
}

bool DecoderTextLines::checkFormat(const char *data, size_t idx)
{
  // a line ends with LF; the reader starts over when its buffer is full
  // without one, and then the rest of that line is no line of its own
  if (idx + 2 >= FIFO_LENGTH && data[idx] != '\n')
    m_dropNext = true;
  return data[idx] == '\n';
}

std::optional<DmmDecoder::DmmResponse> DecoderTextLines::decode(const QByteArray &data, int id)
{
  if (std::exchange(m_dropNext, false))
    return std::nullopt;
  QString line = QString::fromUtf8(data);
  while (line.endsWith(QLatin1Char('\n')) || line.endsWith(QLatin1Char('\r')))
    line.chop(1);
  line = line.trimmed();
  const QList<Item> items = parse(line);
  if (items.isEmpty())
    return std::nullopt;

  m_result = {};
  m_result.channels.clear();   // only the values this line has
  for (const Item &item : items)
  {
    int slot = m_keys.indexOf(item.key);
    if (slot < 0)
    {
      if (m_keys.size() >= kMaxValues)
        continue;   // a fifth name: left out
      slot = int(m_keys.size());
      m_keys << item.key;
    }
    DmmChannel c;
    c.id = id + slot;
    c.val = item.text;
    c.unit = item.unit;
    // unnamed numbers keep the name of their place only as a key
    static const QRegularExpression place(QStringLiteral("^\\d+$"));
    if (!place.match(item.key).hasMatch())
      c.name = item.key;
    c.dval = item.value * SiPrefix::factor(SiPrefix::split(item.unit).prefix);
    m_result.channels << c;
  }
  if (m_result.channels.isEmpty())
    return std::nullopt;
  return m_result;
}
