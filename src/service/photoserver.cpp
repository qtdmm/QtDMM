// Copyright (c) 2026 The QtDMM developers
// SPDX-License-Identifier: GPL-3.0-or-later
#include "service/photoserver.h"

#include "core/photostore.h"

#include <QFile>
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
// a connection's whole time (ms): a large photo over a slow WLAN fits
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

PhotoServer::PhotoServer(QObject *parent) : QObject(parent), m_server(new QTcpServer(this))
{
  connect(m_server, &QTcpServer::newConnection, this, &PhotoServer::onConnection);
}

PhotoServer::~PhotoServer()
{
  stop();
}

bool PhotoServer::start(const QHostAddress &address, quint16 port)
{
  stop();
  m_error.clear();
  if (!m_server->listen(address, port))
  {
    m_error = m_server->errorString();
    return false;
  }
  // a new token each time: a link of before no longer works
  quint32 words[4];
  QRandomGenerator::system()->fillRange(words);
  m_token.clear();
  for (quint32 w : words)
    m_token += QString("%1").arg(w, 8, 16, QChar('0'));
  return true;
}

void PhotoServer::stop()
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

bool PhotoServer::isRunning() const
{
  return m_server->isListening();
}

QUrl PhotoServer::url() const
{
  if (!isRunning())
    return {};
  QUrl url;
  url.setScheme("http");
  url.setHost(m_server->serverAddress().toString());
  url.setPort(m_server->serverPort());
  url.setPath('/' + m_token + '/');
  return url;
}

int PhotoServer::rank(const QHostAddress &address, const QString &card)
{
  const bool privateNet = address.isInSubnet(QHostAddress("192.168.0.0"), 16)
                          || address.isInSubnet(QHostAddress("10.0.0.0"), 8)
                          || address.isInSubnet(QHostAddress("172.16.0.0"), 12);
  return (isVirtual(card) ? 2 : 0) + (privateNet ? 0 : 1);
}

QList<PhotoServer::Address> PhotoServer::addresses()
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

QByteArray PhotoServer::page()
{
  QFile file(":/photo/upload.html");
  if (!file.open(QIODevice::ReadOnly))
    return {};
  QByteArray html = file.readAll();
  // the words, as the program speaks; HTML-escaped
  const QList<std::pair<const char *, QString>> words = {
    // the language the words come in: the one of the translation loaded,
    // English without one (the system's language may have none)
    { "{{LANG}}", tr("en", "the language code (BCP 47) of this translation, for the phone's page") },
    { "{{TITLE}}", tr("Photo for QtDMM") },
    { "{{TAKE}}", tr("Take a photo") },
    { "{{GALLERY}}", tr("Choose from the gallery") },
    { "{{HINT}}", tr("Each photo goes straight into the open project in QtDMM, with a note in the graph. "
                     "Keep this page open for more.") },
    { "{{SENDING}}", tr("Sending...") },
    { "{{SENT}}", tr("Sent") },
    { "{{FAILED}}", tr("Not sent") },
    { "{{GONE}}", tr("QtDMM no longer takes photos: the dialog was closed. Open it again and scan the new code.") },
  };
  for (const auto &[key, text] : words)
    html.replace(key, text.toHtmlEscaped().toUtf8());
  return html;
}

void PhotoServer::onConnection()
{
  while (QTcpSocket *socket = m_server->nextPendingConnection())
  {
    if (m_clients.size() >= kMaxClients)
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

void PhotoServer::onData(QTcpSocket *socket)
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
    c.method = request.at(0);
    c.path = request.at(1);
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
        c.type = value.toLower();
      else if (name == "transfer-encoding")
      {
        answer(socket, 400, "text/plain", "Send the photo with a length");
        return;
      }
    }
    // the token before anything is read: no stranger fills the memory
    if (!c.path.startsWith('/' + m_token.toLatin1() + '/'))
    {
      answer(socket, 403, "text/plain; charset=utf-8", tr("This link no longer works. Scan the code in QtDMM again.").toUtf8());
      return;
    }
    if (c.length > kMaxBytes)
    {
      answer(socket, 413, "text/plain; charset=utf-8", tr("The photo is larger than %1 MB.").arg(kMaxBytes / (1024 * 1024)).toUtf8());
      return;
    }
  }
  else
    c.body += socket->readAll();
  if (c.body.size() < c.length)
    return;
  c.body.truncate(c.length);
  const Client done = c;
  c = Client();   // a next request on the same connection starts anew
  handle(socket, done);
}

void PhotoServer::handle(QTcpSocket *socket, const Client &c)
{
  const QByteArray rest = c.path.mid(m_token.size() + 2);   // after /<token>/
  if (c.method == "GET" && (rest.isEmpty() || rest.startsWith('?')))
  {
    Q_EMIT visited();
    answer(socket, 200, "text/html; charset=utf-8", page());
  }
  else if (rest == "upload")
  {
    if (c.method != "POST")
      answer(socket, 405, "text/plain", "POST a photo");
    else if (!c.type.startsWith("image/") || !PhotoStore::isPhoto(c.body))
      answer(socket, 415, "text/plain; charset=utf-8", tr("QtDMM cannot read this picture (try JPEG).").toUtf8());
    else
    {
      answer(socket, 200, "text/plain", "ok");
      Q_EMIT received(c.body);
    }
  }
  else
    answer(socket, 404, "text/plain", "Not found");
}

void PhotoServer::answer(QTcpSocket *socket, int status, const QByteArray &type, const QByteArray &body)
{
  QByteArray reply = "HTTP/1.1 " + QByteArray::number(status) + ' ' + reason(status) + "\r\n";
  reply += "Content-Type: " + type + "\r\n";
  reply += "Content-Length: " + QByteArray::number(body.size()) + "\r\n";
  reply += "Cache-Control: no-store\r\n";
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
