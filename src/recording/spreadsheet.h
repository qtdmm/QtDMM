// Copyright (c) 2026 The QtDMM developers
// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <QList>
#include <QString>
#include <QStringList>
#include <QVariant>

/// Writes one table as an Excel (.xlsx) or OpenDocument (.ods) spreadsheet:
/// a bold header row, then rows of numbers, text and date/times as real
/// cell types (a QDateTime becomes a date cell with a millisecond format,
/// a double a number - nothing is text that a spreadsheet would have to
/// convert). Both formats are ZIP + XML, written directly with the
/// miniz (bundled or the system's); no office library involved. One sheet for now, the
/// structure allows more.
class SpreadsheetWriter
{
public:
  enum Format { Xlsx, Ods };
  /// Format for a file name by its suffix; CSV and unknown give nothing.
  static std::optional<Format> formatForFile(const QString &path);

  explicit SpreadsheetWriter(const QString &sheetName = QStringLiteral("Sheet1"));

  void setHeader(const QStringList &header);
  /// A row: QDateTime, double/int, or anything else as text (QString).
  void addRow(const QList<QVariant> &row);
  int rowCount() const { return m_rows.size(); }

  /// Writes the file; false with @p error on failure or when empty.
  bool write(const QString &path, Format format, QString *error = nullptr) const;

  // the XML parts, public for the test
  QByteArray xlsxSheet() const;
  QByteArray odsContent() const;

private:
  QString m_sheetName;
  QStringList m_header;
  QList<QList<QVariant>> m_rows;
};
