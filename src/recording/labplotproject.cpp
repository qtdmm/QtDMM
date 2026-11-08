// Copyright (c) 2026 The QtDMM developers
// SPDX-License-Identifier: GPL-3.0-or-later
#include "recording/labplotproject.h"

#include <QCoreApplication>
#include <QFile>
#include <QFileInfo>
#include <QHash>
#include <QRegularExpression>
#include <QSaveFile>
#include <QtEndian>

#include <cmath>
#include <cstring>

#include "recording/recordingfile.h"

namespace
{
QString tr(const char *text)
{
  return QCoreApplication::translate("LabPlotProject", text);
}

void setError(QString *error, const QString &text)
{
  if (error)
    *error = text;
}

QString part(const QString &name)
{
  QFile file(":/labplot/" + name);
  return file.open(QIODevice::ReadOnly) ? QString::fromUtf8(file.readAll()) : QString();
}

// ${KEY} of @p tpl replaced by its value, in one pass: what goes in is not
// looked at again; the caller escapes what is no XML itself
QString fill(const QString &tpl, const QHash<QString, QString> &values)
{
  static const QRegularExpression key(R"(\$\{(\w+)\})");
  QString out;
  qsizetype last = 0;
  for (auto it = key.globalMatch(tpl); it.hasNext();)
  {
    const QRegularExpressionMatch m = it.next();
    out += tpl.mid(last, m.capturedStart() - last) + values.value(m.captured(1));
    last = m.capturedEnd();
  }
  return out + tpl.mid(last);
}

QString esc(const QString &text)
{
  return text.toHtmlEscaped();   // & < > " - enough for attributes and text
}

QString num(double v)
{
  return QString::number(v, 'g', 15);
}

// LabPlot finds a column by its path "project/spreadsheet/column"
QString aspectName(QString name)
{
  return name.replace('/', '-');
}

// the numbers as LabPlot keeps a column: the doubles, little endian, base64
QByteArray packed(const QVector<double> &values)
{
  QByteArray raw;
  raw.reserve(values.size() * qsizetype(sizeof(double)));
  for (double v : values)
  {
    quint64 bits = 0;
    std::memcpy(&bits, &v, sizeof bits);
    bits = qToLittleEndian(bits);
    raw.append(reinterpret_cast<const char *>(&bits), sizeof bits);
  }
  return raw.toBase64();
}

// the curves' colours in a fixed order, the colour follows the value
struct Rgb
{
  int r, g, b;
};
constexpr Rgb kColors[] = { { 31, 119, 180 },  { 255, 127, 14 }, { 44, 160, 44 },
                            { 214, 39, 40 },   { 148, 103, 189 }, { 140, 86, 75 },
                            { 227, 119, 194 }, { 127, 127, 127 }, { 188, 189, 34 },
                            { 23, 190, 207 } };
}

std::pair<double, double> LabPlotProject::niceRange(double lo, double hi)
{
  if (!(lo <= hi))   // NaN, nothing to show
    return { 0.0, 1.0 };
  if (hi == lo)
  {
    const double around = lo == 0 ? 1.0 : std::abs(lo) * 0.1;
    lo -= around;
    hi += around;
  }
  const double rough = (hi - lo) / 5;
  const double decade = std::pow(10.0, std::floor(std::log10(rough)));
  double step = 10 * decade;
  for (double f : { 1.0, 2.0, 2.5, 5.0 })
    if (f * decade >= rough)
    {
      step = f * decade;
      break;
    }
  // a little slack: 0.3 / 0.02 is 15.000000000000002; 12 digits drop the
  // noise of 10 x 0.02 (0.20000000000000001)
  auto clean = [](double v) { return QString::number(v, 'g', 12).toDouble(); };
  return { clean(std::floor(lo / step + 1e-9) * step), clean(std::ceil(hi / step - 1e-9) * step) };
}

QByteArray LabPlotProject::xml(const Recording &recording, const QString &name)
{
  const QString plotTpl = part("plot.xml");
  const QString curveTpl = part("curve.xml");
  const QString legendTpl = part("legend.xml");
  if (plotTpl.isEmpty() || curveTpl.isEmpty() || legendTpl.isEmpty())
    return {};
  QVector<double> seconds;
  const QList<RecordingFile::NumberColumn> columns = RecordingFile::numberColumns(recording, &seconds);
  const QString project = aspectName(name);
  const QString data = tr("Data");
  const QString graph = tr("Graph");
  const QString timeName = QStringLiteral("time[s]");
  auto columnName = [](const RecordingFile::NumberColumn &col)
  {
    return aspectName(col.unit.isEmpty() ? col.name : QString("%1 [%2]").arg(col.name, col.unit));
  };

  QString sheet = QString(R"(<column name="%1" rows="%2" designation="1" mode="0"><comment/>%3</column>)")
                    .arg(esc(timeName))
                    .arg(seconds.size())
                    .arg(QString::fromLatin1(packed(seconds)));
  for (const RecordingFile::NumberColumn &col : columns)
    sheet += QString(R"(<column name="%1" rows="%2" designation="2" mode="0"><comment/>%3</column>)")
               .arg(esc(columnName(col)))
               .arg(col.values.size())
               .arg(QString::fromLatin1(packed(col.values)));

  // a plot per unit, in the order the units come
  QStringList units;
  for (const RecordingFile::NumberColumn &col : columns)
    if (!units.contains(col.unit))
      units << col.unit;
  const auto [x0, x1] = seconds.isEmpty() ? niceRange(qQNaN(), qQNaN()) : niceRange(seconds.first(), seconds.last());
  QString plots;
  for (const QString &unit : units)
  {
    QString curves;
    int count = 0;
    double lo = qInf(), hi = -qInf();
    for (int c = 0; c < columns.size(); ++c)
    {
      const RecordingFile::NumberColumn &col = columns[c];
      if (col.unit != unit)
        continue;
      for (double v : col.values)
        if (!std::isnan(v))
        {
          lo = std::min(lo, v);
          hi = std::max(hi, v);
        }
      const Rgb color = kColors[c % std::size(kColors)];
      curves += fill(curveTpl, { { "NAME", esc(aspectName(col.name)) },
                                 { "X", esc(project + "/" + data + "/" + timeName) },
                                 { "Y", esc(project + "/" + data + "/" + columnName(col)) },
                                 { "R", QString::number(color.r) },
                                 { "G", QString::number(color.g) },
                                 { "B", QString::number(color.b) } });
      ++count;
    }
    const auto [y0, y1] = lo <= hi ? niceRange(lo, hi) : niceRange(qQNaN(), qQNaN());
    const QString title = unit.isEmpty() ? tr("value") : unit;
    plots += fill(plotTpl, { { "PLOT", esc(aspectName(title)) },
                             { "XTITLE", esc(tr("time [s]")) },
                             { "YTITLE", esc(title) },
                             { "XMIN", num(x0) },
                             { "XMAX", num(x1) },
                             { "YMIN", num(y0) },
                             { "YMAX", num(y1) },
                             // the legend above the plot's frame, room for it
                             { "TOPPAD", count > 1 ? "130" : "50" },
                             { "CURVES", curves },
                             { "LEGEND", count > 1 ? legendTpl : QString() } });
  }

  const QString path = esc(project), dataPath = esc(project + "/" + data), graphPath = esc(project + "/" + graph);
  QString out = QString(R"(<?xml version="1.0" encoding="UTF-8"?>
<!DOCTYPE LabPlotXML>
<project version="2.12.1" xmlVersion="16" name="%1">
<comment/>
<child_aspect><spreadsheet name="%2"><comment/>%3</spreadsheet></child_aspect>
)")
                  .arg(path, esc(data), sheet);
  // 16 cm wide, 9 cm a plot (LabPlot's unit here is 1/100 cm)
  out += QString(R"(<child_aspect><worksheet name="%1"><comment/>
<geometry x="0" y="0" width="1600" height="%2" useViewSize="0" zoomFit="1"/>
<layout layout="1" topMargin="0" bottomMargin="0" leftMargin="0" rightMargin="0" verticalSpacing="0"
 horizontalSpacing="0" columnCount="1" rowCount="%3"/>
<background type="0" colorStyle="0" imageStyle="1" brushStyle="1" firstColor_r="255" firstColor_g="255"
 firstColor_b="255" secondColor_r="0" secondColor_g="0" secondColor_b="0" fileName="" opacity="1"/>
<plotProperties plotInteractive="1" cartesianPlotActionMode="0" cartesianPlotCursorMode="1"/>
)")
           .arg(esc(graph))
           .arg(900 * units.size())
           .arg(units.size());
  out += plots + "</worksheet></child_aspect>\n";
  // both open, the graph in front
  out += QString(R"(<state><expanded path="%1"/><view path="%2" state="0"/><view path="%3" state="0"/>
<selected path="%3"/><current path="%3"/></state>
</project>
)")
           .arg(path, dataPath, graphPath);
  return out.toUtf8();
}

bool LabPlotProject::write(const Recording &recording, const QString &path, QString *error)
{
  if (recording.values.isEmpty())
  {
    setError(error, tr("Nothing to export."));
    return false;
  }
  const QByteArray text = xml(recording, QFileInfo(path).completeBaseName());
  if (text.isEmpty())
  {
    setError(error, tr("The LabPlot templates are missing."));
    return false;
  }
  QSaveFile file(path);
  if (!file.open(QIODevice::WriteOnly) || file.write(text) != text.size() || !file.commit())
  {
    setError(error, tr("Cannot write the file."));
    return false;
  }
  return true;
}
