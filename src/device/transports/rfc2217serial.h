#pragma once

#include "device/transports/buffereddevice.h"

#include <QTcpSocket>
#include <QTimer>
#include <QHostAddress>

#include "device/dmmdecoder.h"

/// A meter on a serial port shared over the network (RFC 2217, "telnet com
/// port control"), e.g. by ser2net or ESP-Link.
///
/// On connect the telnet options are negotiated and the meter's line settings
/// from the DMMInfo are sent as COM-PORT-OPTION commands. The socket's data
/// is buffered and handed to FrameReader through the QIODevice interface.
class RFC2217SerialDevice : public BufferedDevice
{
  Q_OBJECT
public:
  /// @param info   the meter, for the line settings sent to the server
  /// @param device "host:port"; the TCP connection is started right away
  /// @param parent parent object
  explicit RFC2217SerialDevice(const DmmDecoder::DMMInfo &info,
                               QString device,
                               QObject *parent = nullptr);
  ~RFC2217SerialDevice();

  /// False when the address could not be parsed (errorString() says so);
  /// otherwise the device counts as open while the TCP connection comes up.
  bool open(OpenMode mode) override;
  void close() override;
  /// The connection failed or ended already (finished() came): the connect
  /// starts in the constructor, so this can be before anyone listened.
  bool hasFinished() const { return m_finished; }

Q_SIGNALS:
  void finished();

protected:
  qint64 writeData(const char *data, qint64 len) override;

private Q_SLOTS:
  void onReadyRead();
  void onConnected();
  void onDisconnected();
  void onError(QAbstractSocket::SocketError err);

private:
  void sendRFC2217Negotiation();
  void sendPortOption(quint8 option, const QByteArray &data = {});
  void setPortParameters();

  QTcpSocket *m_socket = nullptr;
  bool m_finished = false;
  DmmDecoder::DMMInfo m_dmmInfo;
  QString m_host;
  quint16 m_port = 0;

  /// Strips telnet commands/subnegotiations (the server's option replies)
  /// from the stream and undoes the IAC IAC escaping; the rest is meter data.
  void filterTelnet(const QByteArray &raw);

  static constexpr unsigned int m_buflen = 1024;
  /// telnet parser state between socket reads
  enum class TelnetState { Data, Iac, Option, Sub, SubIac } m_telnet = TelnetState::Data;
};
