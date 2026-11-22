// Copyright (c) 2026 The QtDMM developers
// SPDX-License-Identifier: GPL-3.0-or-later
#include "ui/views/graphstats.h"

#include <QApplication>
#include <QCoreApplication>
#include <QTextDocument>
#include <cmath>

#include "core/siprefix.h"
#include "recording/recordingstore.h"

// the words are GraphWidget's, translated there
static QString word(const char *text)
{
  return QCoreApplication::translate("GraphWidget", text);
}

RecordingStore::Stats GraphStats::Curve::statsOf(const RecordingSeries &series, qint64 from, qint64 to) const
{
  if (!store)
    return {};
  std::function<bool(int)> skip;
  if (hidden)
    skip = [this, &series](int i) { return hidden(series, i); };
  return store->statsOf(series, from - shift, to - shift, skip);
}

const RecordingSeries *GraphStats::Curve::seriesAt(qint64 t) const
{
  // the series whose reading holds is the one with the latest reading up to t
  const RecordingSeries *best = nullptr;
  qint64 bestT = -1;
  for (const RecordingSeries *s : parts)
    if (const int i = s->holding(t - shift); i >= 0 && s->at(i).t >= bestT)
    {
      best = s;
      bestT = s->at(i).t;
    }
  return best;
}

double GraphStats::Curve::valueAt(qint64 t) const
{
  const RecordingSeries *series = seriesAt(t);
  const int i = series ? series->holding(t - shift) : -1;
  if (i < 0 || !store || t - shift > store->duration() || series->at(i).gap())
    return std::nan("");
  return series->at(i).value;
}

const RawPoint *GraphStats::Curve::readingNear(qint64 t, qint64 *at) const
{
  const RecordingSeries *series = seriesAt(t);
  if (!series && !parts.isEmpty())
    series = parts.first();   // before the first reading: it may be the nearer one
  if (!series || !store)
    return nullptr;
  const qint64 own = t - shift;
  int i = series->holding(own);
  if (i + 1 < series->count() && (i < 0 || (!series->at(i).gap() && !series->at(i + 1).gap()
                                            && series->at(i + 1).t - own < own - series->at(i).t)))
    i++;
  if (i < 0 || i >= series->count() || own > qMax(store->duration(), series->last().t))
    return nullptr;
  if (at)
    *at = series->at(i).t + shift;
  return &series->at(i);
}

QString GraphStats::withUnit(double v, const QString &unit)
{
  if (!std::isfinite(v))
    return QStringLiteral("--");
  QString prefix;
  const double x = SiPrefix::scale(v, unit, &prefix);
  return QString("%1 %2").arg(QString::number(x, 'g', 4), SiPrefix::displayText(prefix + unit)).trimmed();
}

QString GraphStats::spanText(qint64 ms)
{
  if (ms < 1000)
    return QString("%1 ms").arg(ms);
  if (ms < 60000)
    return QString("%1 s").arg(QString::number(ms / 1000.0, 'f', ms < 10000 ? 3 : 2));
  const qint64 seconds = qRound64(ms / 1000.0);
  const qint64 d = seconds / 86400, h = seconds / 3600 % 24, m = seconds / 60 % 60, sec = seconds % 60;
  const QString text = h || d ? QString("%1:%2:%3").arg(h).arg(m, 2, 10, QChar('0')).arg(sec, 2, 10, QChar('0'))
                              : QString("%1:%2").arg(m).arg(sec, 2, 10, QChar('0'));
  return d ? QString("%1 d %2").arg(d).arg(text) : text;
}

QString GraphStats::timeText(qint64 ms)
{
  ms = qMax<qint64>(0, ms);
  const qint64 seconds = ms / 1000;
  return QString("%1:%2:%3.%4").arg(seconds / 3600).arg(seconds / 60 % 60, 2, 10, QChar('0')).arg(seconds % 60, 2, 10, QChar('0'))
         .arg(ms % 1000, 3, 10, QChar('0'));
}

QString GraphStats::stateWords(quint32 flags, Quality quality)
{
  QStringList words;
  if (quality == Quality::Overload)
    words << QStringLiteral("OL");
  if (flags & SampleFlag::Hold)
    words << QStringLiteral("HOLD");
  if (flags & SampleFlag::Peak)
    words << QStringLiteral("PEAK");
  if (flags & SampleFlag::Min)
    words << QStringLiteral("MIN");
  if (flags & SampleFlag::Max)
    words << QStringLiteral("MAX");
  if (flags & SampleFlag::Relative)
    words << QStringLiteral("REL");
  return words.join(' ');
}

QString GraphStats::gapCause(const RawPoint &p)
{
  if (p.quality == Quality::Fault)
    return word("fault");
  if (p.quality == Quality::NoValue)
    return word("no number");
  if (p.flags & SampleFlag::GapPause)
    return word("paused");
  if (p.flags & SampleFlag::GapLost)
    return word("connection lost");
  return word("no value");
}

QString GraphStats::ColumnScale::text(double v) const
{
  if (!std::isfinite(v))
    return QStringLiteral("--");
  return (QString::number(v * factor, 'f', decimals) + ' ' + unit).trimmed();
}

GraphStats::ColumnScale GraphStats::columnScale(const QList<double> &values, const QList<double> &fine, const QString &unit)
{
  ColumnScale c;
  c.unit = SiPrefix::displayText(unit);   // "°C" for C
  double top = 0;
  for (double v : values + fine)
    if (std::isfinite(v))
      top = qMax(top, std::abs(v));
  if (top <= 0)
    return c;   // nothing but zeros: "0 V"
  QString prefix;
  const double scaled = std::abs(SiPrefix::scale(top, unit, &prefix));
  c.factor = scaled / top;
  c.unit = SiPrefix::displayText(prefix + unit);
  c.decimals = qBound(0, 3 - int(std::floor(std::log10(scaled))), 6);
  double low = 0;
  for (double v : fine)
    if (std::isfinite(v) && v != 0 && (low == 0 || std::abs(v) < low))
      low = std::abs(v);
  if (low > 0)
    c.decimals = qMin(6, qMax(c.decimals, 1 - int(std::floor(std::log10(low * c.factor)))));
  return c;
}

QString GraphStats::html(const Input &input, const QFontMetrics &fm, int room, int *wantOut)
{
  // nothing breaks inside a cell: a unit stays with its number
  auto solid = [](const QString &text) { return text.toHtmlEscaped().replace(' ', "&nbsp;"); };
  // a value's name in its curve's colour, cut to the column's width
  auto head = [&](const Curve &c)
  {
    const QString name = room > 0 ? fm.elidedText(c.name, Qt::ElideRight, qMax(60, room - 46)) : c.name;
    return QString("<tr><td colspan=\"2\"><span style=\"color:%1\">&#9632;</span>&nbsp;%2</td></tr>").arg(c.colour.name(), solid(name));
  };
  auto between = [](const Curve &c, qint64 from, qint64 to)
  {
    const RecordingSeries *series = c.seriesAt(qMax(from, to));
    return series ? c.statsOf(*series, from, to) : RecordingStore::Stats();
  };
  // a row: the number in full, its name cut to what the column leaves; the
  // widest row says how wide the column wants to be
  const int frame = 2 * 6 + 4 + 16;
  int want = 0;
  auto row = [&](QString name, const QString &value)
  {
    const int number = fm.horizontalAdvance(value) + 12, indent = fm.horizontalAdvance(QStringLiteral("  "));
    want = qMax(want, indent + fm.horizontalAdvance(name) + number + frame);
    if (room > 0)
      name = fm.elidedText(name, Qt::ElideRight, qMax(30, room - frame - indent - number));
    return QString("<tr><td>&nbsp;&nbsp;%1</td><td align=\"right\">%2</td></tr>").arg(solid(name), solid(value));
  };
  auto table = [](const QString &rows) { return "<table cellspacing=\"0\" cellpadding=\"1\" width=\"100%\">" + rows + "</table>"; };
  auto title = [](const QString &text) { return QString("<p style=\"margin-top:8px\"><b>%1</b></p>").arg(text.toHtmlEscaped()); };
  auto statRows = [&](const RecordingStore::Stats &st, const ColumnScale &c)
  {
    return row(word("mean"), c.text(st.mean)) + row(QString(QChar(0x03c3)), c.text(st.sigma))
           + row(word("min"), c.text(st.min)) + row(word("max"), c.text(st.max));
  };
  const qint64 a = input.cursor[0], b = input.cursor[1];

  QString html;
  // the cursors
  html += title(word("Cursors"));
  if (a < 0 && b < 0)
    html += QString("<p>%1</p>").arg(word("Click: cursor A, Shift+click: B").toHtmlEscaped());
  QString rows;
  for (int i = 0; i < 2; ++i)
    if (input.cursor[i] >= 0)
    {
      rows += QString("<tr><td colspan=\"2\"><b>%1</b>&nbsp;%2</td></tr>").arg(i ? "B" : "A", solid(timeText(input.cursor[i])));
      for (const Curve &c : input.curves)
      {
        // A and B of a value on one scale
        QList<double> both;
        for (qint64 t : input.cursor)
          if (t >= 0)
            both << c.valueAt(t);
        rows += row(c.name, columnScale(both, {}, c.unit).text(c.valueAt(input.cursor[i])));
      }
    }
  html += table(rows);

  // from A to B
  if (a >= 0 && b >= 0)
  {
    html += title(QString("A %1 B   %2 %3").arg(QChar(0x2013), QChar(0x0394) + QStringLiteral("t"), spanText(std::abs(b - a))));
    rows.clear();
    for (const Curve &curve : input.curves)
    {
      const QString base = curve.unit;
      const RecordingStore::Stats st = between(curve, a, b);
      const double delta = curve.valueAt(b) - curve.valueAt(a);
      const ColumnScale c = st.count ? columnScale({ delta, st.mean, st.min, st.max }, { st.sigma }, base)
                                     : columnScale({ delta }, {}, base);
      rows += head(curve);
      rows += row(QString(QChar(0x0394)), c.text(delta));
      rows += statRows(st, c);
      rows += row(word("readings"), QString::number(st.count));
      // the integral, in hours: for a current or a power
      if (base == QLatin1String("A") || base == QLatin1String("W") || base == QLatin1String("VA"))
        rows += row(QString(QChar(0x222b)), st.count ? withUnit(st.integral / 3600, base + "h") : QStringLiteral("--"));
    }
    html += table(rows);
  }

  // the time shown
  html += title(word("Shown") + QStringLiteral("   ") + spanText(input.to - input.from)
                + (input.outliersHidden > 0 ? QStringLiteral("   ")
                     + QCoreApplication::translate("GraphWidget", "without %n outlier(s)", nullptr, input.outliersHidden) : QString()));
  rows.clear();
  for (const Curve &curve : input.curves)
  {
    const RecordingStore::Stats st = between(curve, input.from, qMin(input.to, qMax(input.from, input.end)));
    const ColumnScale c = st.count ? columnScale({ st.mean, st.min, st.max }, { st.sigma, st.max - st.min }, curve.unit) : ColumnScale();
    rows += head(curve);
    rows += statRows(st, c);
    rows += row(word("peak-peak"), st.count ? c.text(st.max - st.min) : QStringLiteral("--"));
    rows += row(word("readings"), QString::number(st.count));
  }
  html += table(rows);
  if (wantOut)
    *wantOut = want;
  return html;
}

QString GraphStats::text(const Input &input)
{
  QTextDocument doc;
  doc.setHtml(html(input, QFontMetrics(QApplication::font())));
  return doc.toPlainText().replace(QChar(0xa0), ' ');   // the cells' fixed spaces
}
