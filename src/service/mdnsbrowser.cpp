// Copyright (c) 2026 The QtDMM developers
// SPDX-License-Identifier: GPL-3.0-or-later
#include "service/mdnsbrowser.h"

#include <QLoggingCategory>
#include <QNetworkInterface>
#include <QTimer>
#include <QUdpSocket>

Q_LOGGING_CATEGORY(lcMdns, "qtdmm.mdns", QtWarningMsg)

namespace
{
const QHostAddress kMdnsGroup(QStringLiteral("224.0.0.251"));
constexpr quint16 kMdnsPort = 5353;

// Reads a DNS name at @p pos (compression pointers followed, at most a
// few hops); returns false on malformed data. @p pos ends after the name
// as it appears in place (a pointer counts two bytes).
bool readName(const QByteArray &d, int &pos, QString &name)
{
  QStringList labels;
  int p = pos;
  int hops = 0;
  bool jumped = false;
  while (true)
  {
    if (p >= d.size())
      return false;
    const int len = quint8(d[p]);
    if (len == 0)
    {
      ++p;
      break;
    }
    if ((len & 0xC0) == 0xC0)
    {
      if (p + 1 >= d.size())
        return false;
      if (++hops > 16)
        return false;
      const int target = ((len & 0x3F) << 8) | quint8(d[p + 1]);
      if (!jumped)
        pos = p + 2;
      jumped = true;
      if (target >= d.size())
        return false;
      p = target;
      continue;
    }
    if (p + 1 + len > d.size())
      return false;
    labels << QString::fromUtf8(d.constData() + p + 1, len);
    p += 1 + len;
  }
  if (!jumped)
    pos = p;
  name = labels.join('.');
  return true;
}

quint16 u16(const QByteArray &d, int pos)
{
  return quint16((quint8(d[pos]) << 8) | quint8(d[pos + 1]));
}

quint32 u32(const QByteArray &d, int pos)
{
  return (quint32(u16(d, pos)) << 16) | u16(d, pos + 2);
}
}

QList<MdnsMessage::Record> MdnsMessage::parse(const QByteArray &d)
{
  QList<Record> records;
  if (d.size() < 12)
    return records;
  const int qd = u16(d, 4), an = u16(d, 6), ns = u16(d, 8), ar = u16(d, 10);
  int pos = 12;
  QString name;
  for (int i = 0; i < qd; ++i)   // questions: name, type, class
  {
    if (!readName(d, pos, name) || pos + 4 > d.size())
      return {};
    pos += 4;
  }
  for (int i = 0; i < an + ns + ar; ++i)
  {
    Record r;
    if (!readName(d, pos, r.name) || pos + 10 > d.size())
      return records;
    r.type = u16(d, pos);
    r.ttl = u32(d, pos + 4);
    const int rdlen = u16(d, pos + 8);
    pos += 10;
    if (pos + rdlen > d.size())
      return records;
    const int rd = pos;
    switch (r.type)
    {
      case PTR:
      {
        int p = rd;
        if (!readName(d, p, r.target))
          return records;
        break;
      }
      case SRV:
      {
        if (rdlen < 7)
          return records;
        r.port = u16(d, rd + 4);
        int p = rd + 6;
        if (!readName(d, p, r.target))
          return records;
        break;
      }
      case TXT:
      {
        int p = rd;
        while (p < rd + rdlen)
        {
          const int len = quint8(d[p]);
          if (p + 1 + len > rd + rdlen)
            break;
          const QString entry = QString::fromUtf8(d.constData() + p + 1, len);
          const int eq = entry.indexOf('=');
          if (eq > 0)
            r.txt[entry.left(eq).toLower()] = entry.mid(eq + 1);
          else if (!entry.isEmpty())
            r.txt[entry.toLower()] = QString();
          p += 1 + len;
        }
        break;
      }
      case A:
        if (rdlen == 4)
          r.address = QHostAddress(u32(d, rd));
        break;
      case AAAA:
        if (rdlen == 16)
          r.address = QHostAddress(reinterpret_cast<const quint8 *>(d.constData() + rd));
        break;
      default:
        break;
    }
    pos += rdlen;
    records << r;
  }
  return records;
}

QByteArray MdnsMessage::query(const QString &service)
{
  QByteArray q;
  q.append(12, '\0');
  q[5] = 1;   // one question, id 0, flags 0
  for (const QString &label : service.split('.', Qt::SkipEmptyParts))
  {
    const QByteArray l = label.toUtf8().left(63);
    q.append(char(l.size()));
    q.append(l);
  }
  q.append('\0');
  q.append('\0'); q.append(char(MdnsMessage::PTR));   // type PTR
  q.append('\0'); q.append(char(1));                  // class IN
  return q;
}

QList<MdnsMessage::Question> MdnsMessage::questions(const QByteArray &d)
{
  QList<Question> qs;
  if (d.size() < 12 || (quint8(d[2]) & 0x80))   // QR set: a response
    return qs;
  const int qd = u16(d, 4);
  int pos = 12;
  for (int i = 0; i < qd; ++i)
  {
    Question q;
    if (!readName(d, pos, q.name) || pos + 4 > d.size())
      return {};
    q.type = u16(d, pos);
    q.unicastResponse = quint8(d[pos + 2]) & 0x80;
    pos += 4;
    qs << q;
  }
  return qs;
}

namespace
{
QByteArray encodeName(const QString &name)
{
  QByteArray out;
  for (const QString &label : name.split('.', Qt::SkipEmptyParts))
  {
    const QByteArray l = label.toUtf8().left(63);
    out.append(char(l.size()));
    out.append(l);
  }
  out.append('\0');
  return out;
}

void appendU16(QByteArray &d, quint16 v)
{
  d.append(char(v >> 8));
  d.append(char(v & 0xFF));
}

void appendRecord(QByteArray &d, const MdnsMessage::Record &r)
{
  d.append(encodeName(r.name));
  appendU16(d, r.type);
  // IN; unique records carry the cache-flush bit, the shared PTR of the
  // service enumeration must not (RFC 6762 10.2)
  appendU16(d, r.type == MdnsMessage::PTR ? 0x0001 : 0x8001);
  appendU16(d, quint16(r.ttl >> 16));
  appendU16(d, quint16(r.ttl & 0xFFFF));
  QByteArray rdata;
  switch (r.type)
  {
    case MdnsMessage::PTR:
      rdata = encodeName(r.target);
      break;
    case MdnsMessage::SRV:
      appendU16(rdata, 0);   // priority
      appendU16(rdata, 0);   // weight
      appendU16(rdata, r.port);
      rdata.append(encodeName(r.target));
      break;
    case MdnsMessage::TXT:
      for (auto it = r.txt.cbegin(); it != r.txt.cend(); ++it)
      {
        const QByteArray entry = (it.key() + '=' + it.value()).toUtf8().left(255);
        rdata.append(char(entry.size()));
        rdata.append(entry);
      }
      if (rdata.isEmpty())
        rdata.append('\0');
      break;
    case MdnsMessage::A:
    {
      const quint32 ip = r.address.toIPv4Address();
      appendU16(rdata, quint16(ip >> 16));
      appendU16(rdata, quint16(ip & 0xFFFF));
      break;
    }
    case MdnsMessage::AAAA:
    {
      const Q_IPV6ADDR ip = r.address.toIPv6Address();
      rdata.append(reinterpret_cast<const char *>(ip.c), 16);
      break;
    }
    default:
      break;
  }
  appendU16(d, quint16(rdata.size()));
  d.append(rdata);
}
}

QList<MdnsMessage::Interface> MdnsMessage::multicastInterfaces()
{
  QList<Interface> result;
  for (const QNetworkInterface &iface : QNetworkInterface::allInterfaces())
  {
    const auto flags = iface.flags();
    if (!(flags & QNetworkInterface::IsUp) || !(flags & QNetworkInterface::IsRunning)
        || (flags & QNetworkInterface::IsLoopBack) || !(flags & QNetworkInterface::CanMulticast))
      continue;
    for (const QNetworkAddressEntry &entry : iface.addressEntries())
      if (entry.ip().protocol() == QAbstractSocket::IPv4Protocol)
        result.append({ iface, entry });
  }
  return result;
}

QByteArray MdnsMessage::response(const QList<Record> &answers, const QList<Record> &additional)
{
  QByteArray d;
  appendU16(d, 0);        // id
  appendU16(d, 0x8400);   // response, authoritative
  appendU16(d, 0);        // questions
  appendU16(d, quint16(answers.size()));
  appendU16(d, 0);        // authority
  appendU16(d, quint16(additional.size()));
  for (const Record &r : answers)
    appendRecord(d, r);
  for (const Record &r : additional)
    appendRecord(d, r);
  return d;
}

// ---------------------------------------------------------------------------

MdnsBrowser::MdnsBrowser(QObject *parent) : QObject(parent), m_timer(new QTimer(this))
{
  m_timer->setSingleShot(true);
  connect(m_timer, &QTimer::timeout, this, &MdnsBrowser::stop);
}

MdnsBrowser::~MdnsBrowser()
{
  stop();
}

void MdnsBrowser::browse(const QString &service, int ms)
{
  stop();
  m_service = service.toLower();
  m_services.clear();
  m_instances.clear();
  m_srv.clear();
  m_txt.clear();
  m_addresses.clear();
  m_display.clear();
  m_active = true;

  // one socket per multicast-capable interface: the query goes out with
  // that interface as source, answers (unicast or multicast) come back
  // on it
  const QByteArray q = MdnsMessage::query(m_service);
  for (const auto &[iface, entry] : MdnsMessage::multicastInterfaces())
  {
    auto *s = new QUdpSocket(this);
    if (!s->bind(entry.ip(), 0, QUdpSocket::ShareAddress))
    {
      delete s;
      continue;
    }
    s->setMulticastInterface(iface);
    s->joinMulticastGroup(kMdnsGroup, iface);
    connect(s, &QUdpSocket::readyRead, this, &MdnsBrowser::onReadyRead);
    s->writeDatagram(q, kMdnsGroup, kMdnsPort);
    m_sockets << s;
    qCDebug(lcMdns) << "query on" << iface.humanReadableName() << entry.ip().toString();
  }
  if (m_sockets.isEmpty())
  {
    qCWarning(lcMdns) << "no multicast-capable interface";
    stop();
    return;
  }
  m_timer->start(ms);
}

void MdnsBrowser::stop()
{
  m_timer->stop();
  for (QUdpSocket *s : m_sockets)
  {
    s->disconnect(this);
    s->close();
    s->deleteLater();
  }
  m_sockets.clear();
  if (m_active)
  {
    m_active = false;
    Q_EMIT finished();
  }
}

void MdnsBrowser::onReadyRead()
{
  auto *s = qobject_cast<QUdpSocket *>(sender());
  while (s && s->hasPendingDatagrams())
  {
    QByteArray packet(int(s->pendingDatagramSize()), '\0');
    s->readDatagram(packet.data(), packet.size());
    handlePacket(packet);
  }
}

void MdnsBrowser::handlePacket(const QByteArray &packet)
{
  const QList<MdnsMessage::Record> records = MdnsMessage::parse(packet);
  // DNS names compare case-insensitively; keys are lower case, the
  // instance keeps its spelling for display
  for (const MdnsMessage::Record &r : records)
  {
    const QString key = r.name.toLower();
    switch (r.type)
    {
      case MdnsMessage::PTR:
        if (key == m_service && !m_instances.contains(r.target.toLower()))
        {
          m_instances << r.target.toLower();
          m_display[r.target.toLower()] = r.target;
        }
        break;
      case MdnsMessage::SRV:
        m_srv[key] = qMakePair(r.target.toLower(), r.port);
        break;
      case MdnsMessage::TXT:
        m_txt[key] = r.txt;
        break;
      case MdnsMessage::A:
        if (!m_addresses.contains(key))
          m_addresses[key] = r.address;
        break;
      default:
        break;
    }
  }
  assemble();
}

// A service is complete once its PTR and SRV are here; TXT and A are
// welcome extras. Reported once.
void MdnsBrowser::assemble()
{
  for (const QString &instance : m_instances)
  {
    if (!m_srv.contains(instance))
      continue;
    Service s;
    // the instance label is what is left before the service name
    const QString full = m_display.value(instance, instance);
    s.instance = full.left(full.size() - m_service.size() - 1);
    s.host = m_srv[instance].first;
    s.port = m_srv[instance].second;
    s.address = m_addresses.value(s.host);
    s.txt = m_txt.value(instance);
    if (m_services.contains(s))
      continue;
    m_services << s;
    qCDebug(lcMdns) << "found" << s.instance << s.host << s.port << s.address.toString() << s.txt;
    Q_EMIT found(s);
  }
}
