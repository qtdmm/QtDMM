// Copyright (c) 2026 The QtDMM developers
// SPDX-License-Identifier: GPL-3.0-or-later
#include "recording/zipfile.h"

#include "3rdparty/miniz/miniz.h"

bool ZipWriter::add(const QByteArray &name, const QByteArray &data, bool store, const QDateTime &time)
{
  // the sizes and offsets are 32 bits, the number of entries 16
  constexpr qint64 kLimit = 0xffffffffLL;
  if (m_entries.size() >= 0xffff || qint64(m_data.size()) + data.size() + name.size() + 30 > kLimit)
    return false;
  QByteArray payload;
  quint16 method = 0;
  if (!store && !data.isEmpty())
  {
    size_t outSize = 0;
    void *out = tdefl_compress_mem_to_heap(data.constData(), size_t(data.size()), &outSize, TDEFL_DEFAULT_MAX_PROBES);
    if (!out)
      return false;
    payload = QByteArray(static_cast<const char *>(out), int(outSize));
    mz_free(out);
    method = 8;
  }
  else
    payload = data;
  if (qint64(m_data.size()) + payload.size() + name.size() + 30 > kLimit)
    return false;
  const quint32 crc = quint32(mz_crc32(MZ_CRC32_INIT, reinterpret_cast<const mz_uint8 *>(data.constData()), size_t(data.size())));

  Entry e;
  e.name = name;
  e.crc = crc;
  e.compressed = quint32(payload.size());
  e.uncompressed = quint32(data.size());
  e.method = method;
  e.offset = quint32(m_data.size());
  // MS-DOS date and time, local, two seconds a step
  e.time = 0;
  e.date = 0x21;   // 1980-01-01
  const QDateTime local = time.toLocalTime();
  if (local.isValid() && local.date().year() >= 1980 && local.date().year() < 2108)
  {
    e.time = quint16((local.time().hour() << 11) | (local.time().minute() << 5) | (local.time().second() / 2));
    e.date = quint16(((local.date().year() - 1980) << 9) | (local.date().month() << 5) | local.date().day());
  }
  m_entries << e;

  // local file header
  put32(0x04034b50);
  put16(20);            // version needed
  put16(0x0800);        // flags: UTF-8 names
  put16(method);
  put16(e.time); put16(e.date);
  put32(crc);
  put32(e.compressed);
  put32(e.uncompressed);
  put16(quint16(name.size()));
  put16(0);             // extra
  m_data += name;
  m_data += payload;
  return true;
}

QByteArray ZipWriter::finish()
{
  const quint32 cdStart = quint32(m_data.size());
  for (const Entry &e : m_entries)
  {
    put32(0x02014b50);
    put16(20); put16(20);
    put16(0x0800);
    put16(e.method);
    put16(e.time); put16(e.date);
    put32(e.crc);
    put32(e.compressed);
    put32(e.uncompressed);
    put16(quint16(e.name.size()));
    put16(0); put16(0);   // extra, comment
    put16(0);             // disk
    put16(0); put32(0);   // attributes
    put32(e.offset);
    m_data += e.name;
  }
  const quint32 cdSize = quint32(m_data.size()) - cdStart;
  put32(0x06054b50);
  put16(0); put16(0);
  put16(quint16(m_entries.size())); put16(quint16(m_entries.size()));
  put32(cdSize);
  put32(cdStart);
  put16(0);
  return m_data;
}

std::optional<QByteArray> zipEntry(const QByteArray &zip, const QString &name)
{
  mz_zip_archive archive{};
  if (!mz_zip_reader_init_mem(&archive, zip.constData(), size_t(zip.size()), 0))
    return std::nullopt;
  size_t size = 0;
  void *data = mz_zip_reader_extract_file_to_heap(&archive, name.toUtf8().constData(), &size, 0);
  mz_zip_reader_end(&archive);
  if (!data)
    return std::nullopt;
  QByteArray bytes(static_cast<const char *>(data), qsizetype(size));
  mz_free(data);
  return bytes;
}

std::optional<QList<ZipFileEntry>> zipFiles(const QByteArray &zip, qint64 maxTotal)
{
  mz_zip_archive archive{};
  if (!mz_zip_reader_init_mem(&archive, zip.constData(), size_t(zip.size()), 0))
    return std::nullopt;
  QList<ZipFileEntry> files;
  bool ok = true;
  const mz_uint count = mz_zip_reader_get_num_files(&archive);
  // the sizes the archive says first: a small file that unpacks to more
  // than the disk holds is refused before anything is unpacked
  qint64 total = 0;
  for (mz_uint i = 0; ok && i < count; ++i)
  {
    mz_zip_archive_file_stat stat;
    ok = mz_zip_reader_file_stat(&archive, i, &stat);
    total += ok ? qint64(stat.m_uncomp_size) : 0;
    ok = ok && total <= maxTotal;
  }
  for (mz_uint i = 0; ok && i < count; ++i)
  {
    if (mz_zip_reader_is_file_a_directory(&archive, i))
      continue;
    mz_zip_archive_file_stat stat;
    if (!mz_zip_reader_file_stat(&archive, i, &stat))
    {
      ok = false;
      break;
    }
    // an empty file: miniz's heap of 0 bytes may be none
    if (stat.m_uncomp_size == 0)
    {
      files.append(ZipFileEntry{ QString::fromUtf8(stat.m_filename), QByteArray() });
      continue;
    }
    size_t size = 0;
    void *data = mz_zip_reader_extract_to_heap(&archive, i, &size, 0);
    ok = data != nullptr;
    if (ok)
    {
      files.append(ZipFileEntry{ QString::fromUtf8(stat.m_filename), QByteArray(static_cast<const char *>(data), qsizetype(size)) });
      mz_free(data);
    }
  }
  mz_zip_reader_end(&archive);
  if (!ok)
    return std::nullopt;
  return files;
}
