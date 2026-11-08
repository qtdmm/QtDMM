// Copyright (c) 2026 The QtDMM developers
// SPDX-License-Identifier: GPL-3.0-or-later
#include "recording/zipfile.h"

#include "3rdparty/miniz/miniz.h"

bool ZipWriter::add(const QByteArray &name, const QByteArray &data, bool store)
{
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
  const quint32 crc = quint32(mz_crc32(MZ_CRC32_INIT, reinterpret_cast<const mz_uint8 *>(data.constData()), size_t(data.size())));

  Entry e;
  e.name = name;
  e.crc = crc;
  e.compressed = quint32(payload.size());
  e.uncompressed = quint32(data.size());
  e.method = method;
  e.offset = quint32(m_data.size());
  m_entries << e;

  // local file header
  put32(0x04034b50);
  put16(20);            // version needed
  put16(0x0800);        // flags: UTF-8 names
  put16(method);
  put16(0); put16(0x21);   // time 00:00:00, date 1980-01-01
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
    put16(0); put16(0x21);
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
