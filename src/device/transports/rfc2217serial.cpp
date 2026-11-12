#include "device/transports/rfc2217serial.h"

#include <QDebug>

RFC2217SerialDevice::RFC2217SerialDevice(const DmmDecoder::DMMInfo &info, QString device, QObject *parent)
  : BufferedDevice{parent}
  , m_dmmInfo(info)
{
  // "host:port"; the last colon separates the port so "[::1]:4000" works too
  const int colon = device.lastIndexOf(':');
  bool portOk = false;
  if (colon > 0)
  {
    m_host = device.left(colon);
    if (m_host.startsWith('[') && m_host.endsWith(']'))
      m_host = m_host.mid(1, m_host.size() - 2);
    m_port = device.mid(colon + 1).toUShort(&portOk);
  }
  if (!portOk || m_port == 0 || m_host.isEmpty())
  {
    setErrorString(tr("RFC2217: expected host:port, got '%1'").arg(device));
    qWarning() << errorString();
    return;
  }

  m_socket = new QTcpSocket(this);
  connect(m_socket, &QTcpSocket::readyRead, this, &RFC2217SerialDevice::onReadyRead);
  connect(m_socket, &QTcpSocket::connected, this, &RFC2217SerialDevice::onConnected);
  connect(m_socket, &QTcpSocket::disconnected, this, &RFC2217SerialDevice::onDisconnected);
  connect(m_socket, &QTcpSocket::errorOccurred, this, &RFC2217SerialDevice::onError);
  m_socket->connectToHost(m_host, m_port);

}

RFC2217SerialDevice::~RFC2217SerialDevice()
{
  close();
}


void RFC2217SerialDevice::onConnected()
{
  QIODevice::open(ReadWrite);
  sendRFC2217Negotiation();
}

void RFC2217SerialDevice::onDisconnected()
{
  // Has to go through the base class: Transport::isOpen() asks QIODevice, so
  // without this a dropped connection still looked open to the rest of the app.
  QIODevice::close();
  Q_EMIT finished();
}

void RFC2217SerialDevice::onError(QAbstractSocket::SocketError err)
{
  Q_UNUSED(err)
  // connection refused, host unreachable, reset by peer: the device is not
  // usable any more and says why
  setErrorString(m_socket ? m_socket->errorString() : tr("socket error"));
  qWarning() << "RFC2217:" << errorString();
  QIODevice::close();
  Q_EMIT finished();
}

bool RFC2217SerialDevice::open(OpenMode mode)
{
  if (!m_socket)
    return false;   // bad address, errorString() set in the constructor
  return QIODevice::open(mode);
}

void RFC2217SerialDevice::close()
{
  // detach first: disconnectFromHost() can emit disconnected() synchronously,
  // which would re-enter here through finished() -> MeterConnection::portLost()
  QTcpSocket *socket = m_socket;
  m_socket = nullptr;
  if (socket)
  {
    socket->disconnect(this);
    socket->disconnectFromHost();
    socket->deleteLater();
  }
  QIODevice::close();
}

// 0xff is the telnet IAC: in the data stream and in an option's data it
// goes doubled
static void appendEscaped(QByteArray &out, QByteArrayView data)
{
  out.reserve(out.size() + data.size() + 4);
  for (char c : data)
  {
    out.append(c);
    if (quint8(c) == 0xFF)
      out.append(c);
  }
}

qint64 RFC2217SerialDevice::writeData(const char *data, qint64 len)
{
  if (!m_socket || !m_socket->isOpen())
    return -1;
  QByteArray escaped;
  appendEscaped(escaped, QByteArrayView(data, len));
  return m_socket->write(escaped) < 0 ? -1 : len;
}

void RFC2217SerialDevice::onReadyRead()
{
  while (m_socket && m_socket->bytesAvailable() > 0 && m_socket->state() == QAbstractSocket::ConnectedState)
  {
    const QByteArray raw = m_socket->read(4096);
    if (raw.isEmpty())
      break;
    filterTelnet(raw);
  }
  if (hasBufferedData())
    Q_EMIT readyRead();
}

// The server answers every COM-PORT-OPTION with IAC SB 44 <cmd+100> ... IAC SE
// and may negotiate options with IAC WILL/DO/...; none of that is meter data.
// A binary protocol's 0xff bytes arrive as IAC IAC.
void RFC2217SerialDevice::filterTelnet(const QByteArray &raw)
{
  QByteArray payload;
  payload.reserve(raw.size());
  const quint8 IAC = 0xFF, SB = 0xFA, WILL = 0xFB, DONT = 0xFE;
  for (char c : raw)
  {
    const quint8 b = quint8(c);
    switch (m_telnet)
    {
      case TelnetState::Data:
        if (b == IAC) m_telnet = TelnetState::Iac;
        else payload.append(c);
        break;
      case TelnetState::Iac:
        if (b == IAC) { payload.append(c); m_telnet = TelnetState::Data; }
        else if (b == SB) m_telnet = TelnetState::Sub;
        else if (b >= WILL && b <= DONT) m_telnet = TelnetState::Option;   // one option byte follows
        else m_telnet = TelnetState::Data;                                 // NOP and friends
        break;
      case TelnetState::Option:
        m_telnet = TelnetState::Data;
        break;
      case TelnetState::Sub:
        if (b == IAC) m_telnet = TelnetState::SubIac;
        break;
      case TelnetState::SubIac:
        // IAC IAC is a 0xff of the option's data; IAC SE (or anything else) ends it
        m_telnet = b == IAC ? TelnetState::Sub : TelnetState::Data;
        break;
    }
  }
  if (!appendToBuffer(payload))
  {
    setErrorString(tr("Network receive buffer limit exceeded."));
    m_socket->disconnect(this);
    m_socket->abort();
    QIODevice::close();
    Q_EMIT finished();
  }
}


void RFC2217SerialDevice::sendRFC2217Negotiation()
{
  // 1 = SET-BAUDRATE
  quint32 baud = m_dmmInfo.baud;
  QByteArray baudBytes(4, 0x00);
  baudBytes[0] = (baud >> 24) & 0xFF;
  baudBytes[1] = (baud >> 16) & 0xFF;
  baudBytes[2] = (baud >> 8) & 0xFF;
  baudBytes[3] = baud & 0xFF;
  sendPortOption(1, baudBytes);

  // 2 = SET-DATASIZE
  sendPortOption(2, QByteArray(1, char(m_dmmInfo.bits)));

  // 3 = SET-PARITY: RFC 2217 codes 1 none, 2 odd, 3 even; DMMInfo::parity is
  // 0 none, 1 even, 2 odd
  quint8 parity = 1;
  switch (m_dmmInfo.parity)
  {
    case 0: parity = 1; break; // none
    case 1: parity = 3; break; // even
    case 2: parity = 2; break; // odd
    default: break;
  }
  sendPortOption(3, QByteArray(1, char(parity)));

  // 4 = SET-STOPSIZE
  sendPortOption(4, QByteArray(1, char(m_dmmInfo.stopBits)));

  // 5 = SET-CONTROL: DTR and RTS are separate SET-CONTROL suboptions, not a bitmask
  sendPortOption(5, QByteArray(1, char(m_dmmInfo.dtr ? 0x08 : 0x09))); // DTR ON/OFF
  sendPortOption(5, QByteArray(1, char(m_dmmInfo.rts ? 0x0b : 0x0c))); // RTS ON/OFF
}

// Sends IAC SB 0x2C <option> <data...> IAC SE
void RFC2217SerialDevice::sendPortOption(quint8 option, const QByteArray &data)
{
  if (!m_socket || !m_socket->isOpen()) return;

  QByteArray pkt;
  pkt.append(char(0xFF)); // IAC
  pkt.append(char(0xFA)); // SB
  pkt.append(char(0x2C)); // COM-PORT-OPTION
  pkt.append(char(option));
  appendEscaped(pkt, data);

  pkt.append(char(0xFF)); // IAC
  pkt.append(char(0xF0)); // SE
  m_socket->write(pkt);
}
