// Copyright (c) 2026 The QtDMM developers
// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

// Minimal XDR reader/writer (RFC 4506): big endian, everything aligned to 4 bytes.

#include <QByteArray>
#include <QtEndian>

class XdrReader
{
public:
  explicit XdrReader(const QByteArray &data) : m_d(data) {}

  quint32 u32()
  {
    if (m_pos > m_d.size() || m_d.size() - m_pos < 4)
    {
      m_ok = false;
      m_pos = m_d.size();
      return 0;
    }
    const quint32 v = qFromBigEndian<quint32>(m_d.constData() + m_pos);
    m_pos += 4;
    return v;
  }
  qint32 i32() { return static_cast<qint32>(u32()); }

  // variable-length opaque / string: length, data, padding
  QByteArray opaque()
  {
    const quint32 n = u32();
    if (!m_ok || m_pos > m_d.size() || qsizetype(n) > m_d.size() - m_pos)
    {
      m_ok = false;
      return {};
    }
    const qsizetype padding = (4 - (qsizetype(n) & 3)) & 3;
    const qsizetype remaining = m_d.size() - m_pos;
    if (padding > remaining - qsizetype(n))
    {
      m_ok = false;
      return {};
    }
    const QByteArray r = m_d.mid(m_pos, n);
    m_pos += qsizetype(n) + padding;
    return r;
  }

  bool ok() const { return m_ok; }

private:
  QByteArray m_d;
  qsizetype m_pos = 0;
  bool m_ok = true;
};

class XdrWriter
{
public:
  void u32(quint32 v)
  {
    char b[4];
    qToBigEndian(v, b);
    m_d.append(b, 4);
  }
  void i32(qint32 v) { u32(static_cast<quint32>(v)); }

  void opaque(const QByteArray &data)
  {
    u32(quint32(data.size()));
    m_d.append(data);
    m_d.append(QByteArray((4 - (data.size() & 3)) & 3, '\0'));
  }
  void raw(const QByteArray &d) { m_d.append(d); }

  const QByteArray &data() const { return m_d; }

private:
  QByteArray m_d;
};
