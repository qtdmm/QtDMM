// Copyright (c) 2026 The QtDMM developers
// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <QHash>
#include <QHostAddress>
#include <QList>
#include <QObject>
#include <QUrl>

class QTcpServer;
class QTcpSocket;

/// The small HTTP server under PhotoServer and WebViewServer: reads a
/// request (head up to 16 KiB, a body only with Content-Length and up to
/// maxBody()), answers 403 to every path that does not start with
/// /<token>/, and hands the rest to handle(). Plain HTTP/1.1 with
/// keep-alive; an error answer closes the connection.
///
/// The token is the caller's: PhotoServer makes a new one each start,
/// WebViewServer keeps one in the settings (a link to keep).
class HttpServer : public QObject
{
  Q_OBJECT

public:
  /// The addresses a phone or browser may reach the computer on: IPv4 of
  /// the network cards up and running, without loopback; LAN and WLAN
  /// first, virtual bridges, VPN and container networks last. Each with
  /// the card's name.
  struct Address
  {
    QString address;   ///< IPv4, as text ("192.168.1.20"); QString moves without throwing
    QString card;      ///< the network card's name ("wlan0", "Ethernet")
  };
  static QList<Address> addresses();
  /// The order of addresses(): lower first. A private address on a card
  /// whose name is no virtual one's (docker, veth, virbr, vmnet, tun ...)
  /// comes first.
  static int rank(const QHostAddress &address, const QString &card);
  /// A new random token: 128 bits as 32 hex digits.
  static QString newToken();

  explicit HttpServer(int maxClients, QObject *parent = nullptr);
  ~HttpServer() override;

  /// Listens on @p address, on a port the system picks (@p port 0) or
  /// @p port; the paths start with /<@p token>/. False when it cannot (error()).
  bool start(const QHostAddress &address, quint16 port, const QString &token);
  void stop();
  bool isRunning() const;
  QString error() const { return m_error; }
  quint16 port() const;
  /// The address listened on (Any, LocalHost ...); null while not running.
  QHostAddress address() const;
  QString token() const { return m_token; }
  /// Another token while running: links with the old one get 403 from now on.
  void setToken(const QString &token) { m_token = token; }
  /// The link to open: http://<host>:<port>/<token>/, empty while not
  /// running. @p host empty: the address listened on.
  QUrl url(const QString &host = {}) const;

protected:
  struct Request
  {
    QByteArray method;
    QByteArray path;    ///< after /<token>/, without the query
    QByteArray query;   ///< after '?'
    QByteArray type;    ///< Content-Type, lower case
    QByteArray body;
    QHostAddress peer;
  };
  /// One request with the right token; answer it with answer().
  virtual void handle(QTcpSocket *socket, const Request &request) = 0;
  /// The body of the 403 to a wrong token (text/plain).
  virtual QByteArray forbidden() const;
  /// The largest body taken (bytes); larger ones get 413 with tooLarge().
  virtual qint64 maxBody() const { return 0; }
  virtual QByteArray tooLarge() const;
  /// The answer; @p headers are added ("Name: value" lines without CRLF).
  void answer(QTcpSocket *socket, int status, const QByteArray &type, const QByteArray &body,
              const QList<QByteArray> &headers = {});

private:
  struct Client
  {
    QByteArray head;   ///< up to the empty line
    QByteArray body;
    Request request;
    qint64 length = -1;   ///< Content-Length, -1 before the head is read
  };

  void onConnection();
  void onData(QTcpSocket *socket);

  QTcpServer *m_server;
  int m_maxClients;
  QString m_token;
  QString m_error;
  QHash<QTcpSocket *, Client> m_clients;
};
