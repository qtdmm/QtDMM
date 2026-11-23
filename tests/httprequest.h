// Copyright (c) 2026 The QtDMM developers
// SPDX-License-Identifier: GPL-3.0-or-later
//
// One HTTP request to a server of QtDMM on localhost, for the tests of
// PhotoServer and WebViewServer.
#pragma once

#include <QHostAddress>
#include <QTcpSocket>

#include "testutil.h"

// the whole answer (the server closes after an error)
struct Reply
{
  int status = 0;
  QByteArray head;   ///< the status line and the headers
  QByteArray body;
  /// One header's value ("x-frame-time"), empty without it.
  QByteArray header(const QByteArray &name) const
  {
    for (const QByteArray &line : head.split('\n'))
      if (line.toLower().startsWith(name.toLower() + ':'))
        return line.mid(name.size() + 1).trimmed();
    return {};
  }
};

inline Reply request(quint16 port, const QByteArray &head, const QByteArray &body = {})
{
  QTcpSocket s;
  s.connectToHost(QHostAddress::LocalHost, port);
  if (!s.waitForConnected(2000))
    return {};
  s.write(head + "\r\n" + body);
  QByteArray all;
  // until the head and the body of its Content-Length are in
  waitFor([&]
  {
    all += s.readAll();
    const qsizetype end = all.indexOf("\r\n\r\n");
    if (end < 0)
      return false;
    const qsizetype at = all.toLower().indexOf("content-length:");
    const qsizetype len = at < 0 ? 0 : all.mid(at + 15, all.indexOf("\r\n", at) - at - 15).trimmed().toLongLong();
    return all.size() >= end + 4 + len;
  }, 3000);
  Reply r;
  r.status = all.mid(9, 3).toInt();
  r.head = all.left(all.indexOf("\r\n\r\n"));
  r.body = all.mid(all.indexOf("\r\n\r\n") + 4);
  return r;
}
