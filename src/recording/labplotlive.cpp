// Copyright (c) 2026 The QtDMM developers
// SPDX-License-Identifier: GPL-3.0-or-later
#include "recording/labplotlive.h"

#include <QFileInfo>
#include <QSaveFile>

#include <cmath>

#include "recording/labplotproject.h"

namespace
{
void setError(QString *error, const QString &text)
{
  if (error)
    *error = text;
}

QString number(double v)
{
  return std::isnan(v) ? QStringLiteral("nan") : QString::number(v, 'g', 12);
}
}

LabPlotLive::LabPlotLive(std::function<Recording()> source, std::function<bool()> running, QObject *parent) :
  QObject(parent),
  m_source(std::move(source)),
  m_running(std::move(running))
{
  m_timer.setInterval(1000);   // LabPlot reads the file on each change; once a second is enough
  connect(&m_timer, &QTimer::timeout, this, &LabPlotLive::update);
}

bool LabPlotLive::start(const QString &csvPath, const QString &projectPath, QString *error)
{
  stop();
  const Recording rec = m_source();
  if (rec.values.isEmpty())
  {
    setError(error, tr("Nothing recorded yet."));
    return false;
  }
  QVector<double> seconds;
  const QList<RecordingFile::NumberColumn> columns = RecordingFile::numberColumns(rec, &seconds);
  const QStringList names = LabPlotProject::liveColumnNames(columns);
  const QByteArray project = LabPlotProject::liveXml(columns, seconds, QFileInfo(csvPath).absoluteFilePath(),
                                                     QFileInfo(projectPath).completeBaseName());
  if (project.isEmpty())
  {
    setError(error, tr("The LabPlot templates are missing."));
    return false;
  }
  m_file.setFileName(csvPath);
  // without Text: '\n' on every system, as the rows write it, so the file
  // grows by whole lines alike on Windows (LabPlot reads either)
  if (!m_file.open(QIODevice::WriteOnly | QIODevice::Truncate)
      || m_file.write(("#" + names.join(';') + "\n").toUtf8()) < 0)
  {
    m_file.close();
    setError(error, tr("Cannot write %1").arg(csvPath));
    return false;
  }
  m_names = names.mid(1);
  m_written = 0;
  append(rec, m_running() ? int(rec.values.size()) - 1 : int(rec.values.size()));
  QSaveFile out(projectPath);
  if (!out.open(QIODevice::WriteOnly) || out.write(project) != project.size() || !out.commit())
  {
    m_file.close();
    setError(error, tr("Cannot write %1").arg(projectPath));
    return false;
  }
  m_project = projectPath;
  if (m_running())
    m_timer.start();
  else
    stop();   // ended already: the file has it all
  return true;
}

void LabPlotLive::update()
{
  if (!isRunning())
    return;
  const Recording rec = m_source();
  const bool running = m_running();
  if (rec.values.size() < m_written)   // cleared, another recording
  {
    stop();
    return;
  }
  append(rec, running ? int(rec.values.size()) - 1 : int(rec.values.size()));
  if (!running)
    stop();
}

void LabPlotLive::stop()
{
  m_timer.stop();
  if (!m_file.isOpen())
    return;
  m_file.close();
  Q_EMIT stopped();
}

// rows m_written to @p end - 1, in the columns of the start
void LabPlotLive::append(const Recording &rec, int end)
{
  if (end <= m_written)
    return;
  QVector<double> seconds;
  const QList<RecordingFile::NumberColumn> columns = RecordingFile::numberColumns(rec, &seconds);
  const QStringList names = LabPlotProject::liveColumnNames(columns).mid(1);
  QList<int> at;   // of each column of the start: its index now, -1 gone
  for (const QString &name : m_names)
    at << int(names.indexOf(name));
  QByteArray rows;
  for (int i = m_written; i < end; ++i)
  {
    QString row = number(seconds[i]);
    for (int c : at)
      row += ';' + number(c < 0 ? qQNaN() : columns[c].values.value(i, qQNaN()));
    rows += row.toUtf8() + '\n';
  }
  m_file.write(rows);
  m_file.flush();
  m_written = end;
}
