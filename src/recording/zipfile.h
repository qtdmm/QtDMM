// Copyright (c) 2026 The QtDMM developers
// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <QByteArray>
#include <QDateTime>
#include <QList>
#include <QString>

#include <optional>

/// A plain ZIP writer: local headers with the sizes known up front, no data
/// descriptors, no extra fields, then the central directory. miniz's own
/// writer sets the data-descriptor flag on every entry, which LibreOffice
/// rejects for the stored "mimetype" entry an ODS must begin with. Deflate
/// and CRC come from miniz.
class ZipWriter
{
public:
  /// Adds @p data as @p name, deflated or, with @p store, as it is, dated
  /// @p time (none: 1980-01-01). False when deflate fails or the archive
  /// would pass 4 GiB or 65535 entries (no ZIP64).
  bool add(const QByteArray &name, const QByteArray &data, bool store, const QDateTime &time = {});
  /// The archive with all entries added.
  QByteArray finish();

private:
  struct Entry { QByteArray name; quint32 crc, compressed, uncompressed, offset; quint16 method, time, date; };
  void put16(quint16 v) { m_data.append(char(v & 0xff)).append(char(v >> 8)); }
  void put32(quint32 v) { put16(quint16(v & 0xffff)); put16(quint16(v >> 16)); }
  QByteArray m_data;
  QList<Entry> m_entries;
};

/// The entry @p name of the ZIP archive @p zip, nothing when it has none or
/// @p zip is no archive (miniz's reader).
std::optional<QByteArray> zipEntry(const QByteArray &zip, const QString &name);
