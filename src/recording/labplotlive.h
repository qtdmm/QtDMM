// Copyright (c) 2026 The QtDMM developers
// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <QFile>
#include <QObject>
#include <QStringList>
#include <QTimer>

#include <functional>

#include "recording/recordingfile.h"

/// A recording that goes on, live in LabPlot: a CSV that grows by its rows
/// and a LabPlot project (LabPlotProject::liveXml()) whose live data source
/// reads that file on each change, so LabPlot's plots follow the recording.
///
/// start() writes what was recorded so far and the project; then, every
/// interval, the rows that came since are appended (the newest held back
/// while it records: on the grid it may still change). The columns are those
/// at the start: a value that begins later is not in the file, one that ends
/// is `nan` from then on. When the recording ends (@p running false) the
/// last rows go in and stopped() comes; so it does when the recording has
/// fewer rows than written (cleared, a new one).
///
/// The recording comes from @p source, each time anew (MeterController::
/// recording(): every meter that records with it).
class LabPlotLive : public QObject
{
  Q_OBJECT

public:
  LabPlotLive(std::function<Recording()> source, std::function<bool()> running, QObject *parent = nullptr);

  /// Writes the CSV @p csvPath and the project @p projectPath, then follows
  /// the recording; false with @p error (translated) when there is nothing
  /// recorded or a file cannot be written.
  bool start(const QString &csvPath, const QString &projectPath, QString *error = nullptr);
  /// Appends what came since, and when the recording ended stops.
  void update();
  /// Stops following; the files stay.
  void stop();
  bool isRunning() const { return m_file.isOpen(); }
  QString projectPath() const { return m_project; }
  int rowsWritten() const { return m_written; }
  void setInterval(int ms) { m_timer.setInterval(ms); }

Q_SIGNALS:
  void stopped();

private:
  void append(const Recording &rec, int end);

  std::function<Recording()> m_source;
  std::function<bool()>      m_running;
  QFile       m_file;
  QString     m_project;
  QStringList m_names;     ///< the columns at the start, without the time
  int         m_written = 0;
  QTimer      m_timer;
};
