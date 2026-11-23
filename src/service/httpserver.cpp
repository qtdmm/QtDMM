// Copyright (c) 2026 The QtDMM developers
// SPDX-License-Identifier: GPL-3.0-or-later
#include "service/httpserver.h"

#include <QNetworkInterface>
#include <QRandomGenerator>
#include <QTcpServer>
#include <QTcpSocket>
#include <QTimer>

#include <algorithm>

namespace
{
// the head of a request may be no longer (bytes)
constexpr int kMaxHead = 16 * 1024;
// a connection's whole time (ms): a large photo over a slow WLAN fits; a
// browser opens a new one after it
constexpr int kClientTimeout = 120 * 1000;

QByteArray reason(int status)
{
  switch (status)
  {
  case 200: return "OK";
  case 400: return "Bad Request";
  case 403: return "Forbidden";
  case 404: return "Not Found";
  case 405: return "Method Not Allowed";
  case 413: return "Payload Too Large";
  case 415: return "Unsupported Media Type";
  case 503: return "Service Unavailable";
  default: return "Error";
  }
}

// names of virtual cards: bridges of VMs and containers, VPN tunnels
bool isVirtual(const QString &card)
{
  static const QStringList prefixes{ "docker", "veth", "br-", "virbr", "vmnet", "vboxnet", "tun", "tap", "wg",
                                     "zt", "tailscale", "utun", "lxc", "lxd", "podman", "cni", "flannel" };
  const QString name = card.toLower();
  for (const QString &p : prefixes)
    if (name.startsWith(p))
      return true;
  return name.contains("virtual") || name.contains("vethernet") || name.contains("hyper-v")
         || name.contains("vmware") || name.contains("virtualbox");
}
}   // namespace

HttpServer::HttpServer(int maxClients, QObject *parent)
  : QObject(parent), m_server(new QTcpServer(this)), m_maxClients(maxClients)
{
  connect(m_server, &QTcpServer::newConnection, this, &HttpServer::onConnection);
}

HttpServer::~HttpServer()
{
  stop();
}

QString HttpServer::newToken()
{
  quint32 words[4];
  QRandomGenerator::system()->fillRange(words);
  QString token;
  for (quint32 w : words)
    token += QString("%1").arg(w, 8, 16, QChar('0'));
  return token;
}

bool HttpServer::start(const QHostAddress &address, quint16 port, const QString &token)
{
  stop();
  m_error.clear();
  if (!m_server->listen(address, port))
  {
    m_error = m_server->errorString();
    return false;
  }
  m_token = token;
  return true;
}

void HttpServer::stop()
{
  m_server->close();
  const QList<QTcpSocket *> sockets = m_clients.keys();
  m_clients.clear();
  for (QTcpSocket *s : sockets)
  {
    s->disconnect(this);
    s->abort();
    s->deleteLater();
  }
  m_token.clear();
}

bool HttpServer::isRunning() const
{
  return m_server->isListening();
}

quint16 HttpServer::port() const
{
  return isRunning() ? m_server->serverPort() : 0;
}

QHostAddress HttpServer::address() const
{
  return isRunning() ? m_server->serverAddress() : QHostAddress();
}

QUrl HttpServer::url(const QString &host) const
{
  if (!isRunning())
    return {};
  QUrl url;
  url.setScheme("http");
  url.setHost(host.isEmpty() ? m_server->serverAddress().toString() : host);
  url.setPort(m_server->serverPort());
  url.setPath('/' + m_token + '/');
  return url;
}

int HttpServer::rank(const QHostAddress &address, const QString &card)
{
  const bool privateNet = address.isInSubnet(QHostAddress("192.168.0.0"), 16)
                          || address.isInSubnet(QHostAddress("10.0.0.0"), 8)
                          || address.isInSubnet(QHostAddress("172.16.0.0"), 12);
  return (isVirtual(card) ? 2 : 0) + (privateNet ? 0 : 1);
}

QList<HttpServer::Address> HttpServer::addresses()
{
  QList<Address> list;
  for (const QNetworkInterface &card : QNetworkInterface::allInterfaces())
  {
    const auto flags = card.flags();
    if (!(flags & QNetworkInterface::IsUp) || !(flags & QNetworkInterface::IsRunning)
        || (flags & QNetworkInterface::IsLoopBack))
      continue;
    for (const QNetworkAddressEntry &entry : card.addressEntries())
    {
      const QHostAddress a = entry.ip();
      if (a.protocol() == QAbstractSocket::IPv4Protocol && !a.isLoopback() && !a.isLinkLocal())
        list << Address{ a.toString(), card.humanReadableName() };
    }
  }
  std::stable_sort(list.begin(), list.end(), [](const Address &x, const Address &y)
                   { return rank(QHostAddress(x.address), x.card) < rank(QHostAddress(y.address), y.card); });
  return list;
}

QByteArray HttpServer::forbidden() const
{
  return "Forbidden";
}

QByteArray HttpServer::tooLarge() const
{
  return "Payload too large";
}

void HttpServer::onConnection()
{
  while (QTcpSocket *socket = m_server->nextPendingConnection())
  {
    if (m_clients.size() >= m_maxClients)
    {
      socket->abort();
      socket->deleteLater();
      continue;
    }
    m_clients.insert(socket, Client());
    connect(socket, &QTcpSocket::readyRead, this, [this, socket] { onData(socket); });
    connect(socket, &QTcpSocket::disconnected, this, [this, socket]
    {
      m_clients.remove(socket);
      socket->deleteLater();
    });
    // no connection stays for ever
    QTimer::singleShot(kClientTimeout, socket, [socket] { socket->abort(); });
  }
}

void HttpServer::onData(QTcpSocket *socket)
{
  auto it = m_clients.find(socket);
  if (it == m_clients.end())
    return;
  Client &c = *it;
  if (c.length < 0)
  {
    c.head += socket->readAll();
    const qsizetype end = c.head.indexOf("\r\n\r\n");
    if (end < 0)
    {
      if (c.head.size() > kMaxHead)
        answer(socket, 400, "text/plain", "Request head too long");
      return;
    }
    c.body = c.head.mid(end + 4);
    c.head.truncate(end);
    const QList<QByteArray> lines = c.head.split('\n');
    const QList<QByteArray> request = lines.value(0).trimmed().split(' ');
    if (request.size() < 3)
    {
      answer(socket, 400, "text/plain", "Bad request line");
      return;
    }
    c.request.method = request.at(0);
    QByteArray path = request.at(1);
    c.request.peer = socket->peerAddress();
    c.length = 0;
    for (const QByteArray &line : lines.mid(1))
    {
      const qsizetype colon = line.indexOf(':');
      if (colon < 0)
        continue;
      const QByteArray name = line.left(colon).trimmed().toLower();
      const QByteArray value = line.mid(colon + 1).trimmed();
      if (name == "content-length")
      {
        bool ok = false;
        c.length = value.toLongLong(&ok);
        if (!ok || c.length < 0)
        {
          answer(socket, 400, "text/plain", "Bad Content-Length");
          return;
        }
      }
      else if (name == "content-type")
        c.request.type = value.toLower();
      else if (name == "transfer-encoding")
      {
        answer(socket, 400, "text/plain", "Send the body with a length");
        return;
      }
    }
    // the token before anything is read: no stranger fills the memory
    const QByteArray prefix = '/' + m_token.toLatin1() + '/';
    if (m_token.isEmpty() || !path.startsWith(prefix))
    {
      answer(socket, 403, "text/plain; charset=utf-8", forbidden());
      return;
    }
    if (c.length > maxBody())
    {
      answer(socket, 413, "text/plain; charset=utf-8", tooLarge());
      return;
    }
    path = path.mid(prefix.size());
    const qsizetype question = path.indexOf('?');
    c.request.path = question < 0 ? path : path.left(question);
    c.request.query = question < 0 ? QByteArray() : path.mid(question + 1);
  }
  else
    c.body += socket->readAll();
  if (c.body.size() < c.length)
    return;
  c.body.truncate(c.length);
  Request done = c.request;
  done.body = c.body;
  c = Client();   // a next request on the same connection starts anew
  handle(socket, done);
}

void HttpServer::answer(QTcpSocket *socket, int status, const QByteArray &type, const QByteArray &body,
                        const QList<QByteArray> &headers)
{
  QByteArray reply = "HTTP/1.1 " + QByteArray::number(status) + ' ' + reason(status) + "\r\n";
  reply += "Content-Type: " + type + "\r\n";
  reply += "Content-Length: " + QByteArray::number(body.size()) + "\r\n";
  reply += "Cache-Control: no-store\r\n";
  for (const QByteArray &h : headers)
    reply += h + "\r\n";
  // an error ends the connection: what is left of its body is not read
  if (status != 200)
    reply += "Connection: close\r\n";
  reply += "\r\n" + body;
  socket->write(reply);
  if (status != 200)
  {
    m_clients.remove(socket);
    socket->disconnectFromHost();
  }
}
