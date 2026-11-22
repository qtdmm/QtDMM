// Copyright (c) 2026 The QtDMM developers
// SPDX-License-Identifier: GPL-3.0-or-later
#include "service/vxi11server.h"

#include <QRandomGenerator>
#include <QTcpSocket>

namespace
{
// Does something accept connections on localhost:port?
bool portAnswers(quint16 port)
{
  QTcpSocket s;
  s.connectToHost(QHostAddress::LocalHost, port);
  return s.waitForConnected(300);
}
} // namespace

QString Vxi11Server::modeName(RpcbindMode mode)
{
  switch (mode)
  {
    case RpcbindMode::System: return QStringLiteral("system");
    case RpcbindMode::Own:    return QStringLiteral("own");
    default:                  return QStringLiteral("auto");
  }
}

Vxi11Server::RpcbindMode Vxi11Server::modeFromName(const QString &name, bool *ok)
{
  for (RpcbindMode m : { RpcbindMode::Auto, RpcbindMode::System, RpcbindMode::Own })
    if (name == modeName(m))
    {
      if (ok)
        *ok = true;
      return m;
    }
  if (ok)
    *ok = false;
  return RpcbindMode::Auto;
}

Vxi11Server::Vxi11Server(const ScpiEndpoint &endpoint, QObject *parent)
  : QObject(parent), m_core(endpoint), m_abort(&m_core), m_coreSrv(&m_coreDisp), m_abortSrv(&m_abortDisp),
    m_pmapTcp(&m_pmapDisp), m_pmapUdp(&m_pmapDisp)
{
  m_coreDisp.addService(&m_core);
  m_abortDisp.addService(&m_abort);
  m_pmapDisp.addService(&m_pmap);
  m_core.setLinksChanged([this] { Q_EMIT linksChanged(m_core.linkCount()); });
}

Vxi11Server::~Vxi11Server()
{
  m_core.setLinksChanged(nullptr);
  stop();
}

void Vxi11Server::setRpcbind(const QStringList &socketPaths, quint16 tcpPort)
{
  m_rpcbind = RpcbindClient(tcpPort);
  m_rpcbind.setSocketPaths(socketPaths);
}

bool Vxi11Server::start(const QHostAddress &address, RpcbindMode mode, quint16 corePort, quint16 abortPort,
                        quint16 portmapperPort)
{
  stop();
  m_error.clear();
  m_address = address;
  m_mode = mode;

  if (!m_coreSrv.listen(address, corePort))
  {
    m_error = tr("core channel: %1").arg(m_coreSrv.errorString());
    stop();
    return false;
  }
  if (!m_abortSrv.listen(address, abortPort))
  {
    m_error = tr("abort channel: %1").arg(m_abortSrv.errorString());
    stop();
    return false;
  }
  m_core.setAbortPort(m_abortSrv.port());
  m_regs = {{m_core.program(), m_core.version(), m_coreSrv.port()},
            {m_abort.program(), m_abort.version(), m_abortSrv.port()}};

  // A system rpcbind owns port 111, so an own portmapper cannot run next to it: if one answers
  // it is the only way, and a refused registration is an error. (Another QtDMM's own
  // portmapper answers too: the registration then says who serves VXI-11.)
  if (mode != RpcbindMode::Own && m_rpcbind.isAvailable())
  {
    if (!registerWithRpcbind())
    {
      stop();
      return false;
    }
    m_systemRpcbind = true;
    m_pmapPort = kPortmapperPort;
    qCDebug(lcVxi11) << "registered with the system rpcbind"
                     << (m_rpcbind.via() == RpcbindClient::Via::UnixSocket ? "(Unix socket)" : "(TCP)")
                     << ", core port" << m_coreSrv.port();
  }
  else if (mode == RpcbindMode::System)
  {
    m_error = tr("no rpcbind is running on this computer");
    stop();
    return false;
  }
  else
  {
    m_pmap.clear();
    for (const Reg &reg : std::as_const(m_regs))
      m_pmap.registerProgram(reg.prog, reg.vers, reg.port);
    // TCP and UDP on the same port
    if (portmapperPort != 0)
    {
      if (!m_pmapTcp.listen(address, portmapperPort))
      {
        m_error = m_pmapTcp.serverError() == QAbstractSocket::AddressInUseError
                    ? tr("port %1 is in use by another program").arg(portmapperPort)
                    : tr("port %1 is not allowed for this program (a privileged port: see the handbook, "
                         "SCPI server, VXI-11)").arg(portmapperPort);
        stop();
        return false;
      }
      if (!m_pmapUdp.bind(address, portmapperPort))
      {
        m_error = tr("UDP port %1: %2").arg(portmapperPort).arg(m_pmapUdp.errorString());
        stop();
        return false;
      }
    }
    else
    {
      // TCP and UDP on one port, which Windows may refuse for either: it keeps whole blocks of
      // its dynamic ports for Hyper-V/WinNAT, for TCP or UDP alone ("The address is protected"),
      // and hands out free ports in a row, so the next ones often lie in the same block. First
      // a port the system picks for UDP, then one it picks for TCP, then ports chosen at random
      // all over the dynamic range, away from the block
      QString why;
      auto bindBoth = [&](quint16 port, bool udpFirst)
      {
        bool ok = false;
        if (udpFirst)
        {
          if (!m_pmapUdp.bind(address, port))
            why = tr("UDP: %1").arg(m_pmapUdp.errorString());
          else if (!m_pmapTcp.listen(address, m_pmapUdp.port()))
            why = tr("TCP port %1: %2").arg(m_pmapUdp.port()).arg(m_pmapTcp.errorString());
          else
            ok = true;
        }
        else if (!m_pmapTcp.listen(address, port))
          why = tr("TCP port %1: %2").arg(port).arg(m_pmapTcp.errorString());
        else if (!m_pmapUdp.bind(address, m_pmapTcp.port()))
          why = tr("UDP port %1: %2").arg(m_pmapTcp.port()).arg(m_pmapUdp.errorString());
        else
          ok = true;
        if (!ok)
        {
          m_pmapTcp.close();
          m_pmapUdp.close();
        }
        return ok;
      };
      bool bound = false;
      for (int attempt = 0; attempt < 60 && !bound; ++attempt)
      {
        if (attempt < 20)
          bound = bindBoth(0, true);
        else if (attempt < 40)
          bound = bindBoth(0, false);
        else
          bound = bindBoth(quint16(QRandomGenerator::global()->bounded(49152, 65536)), true);
      }
      if (!bound)
      {
        m_error = why;
        stop();
        return false;
      }
    }
    m_pmapPort = m_pmapTcp.port();
    qCDebug(lcVxi11) << "own portmapper on port" << m_pmapPort << ", core port" << m_coreSrv.port();
  }

  m_running = true;
  return true;
}

// Registers core and abort with the system rpcbind. On failure m_error says why.
bool Vxi11Server::registerWithRpcbind()
{
  QList<Reg> done;
  for (const Reg &reg : std::as_const(m_regs))
  {
    bool ok = m_rpcbind.set(reg.prog, reg.vers, reg.port);
    if (!ok)
    {
      // An entry exists already: left behind by a crashed run, or served by another live process?
      const quint16 other = m_rpcbind.getPort(reg.prog, reg.vers);
      if (other != 0 && portAnswers(other))
      {
        m_error = tr("another program or QtDMM offers VXI-11 already (port %1); "
                     "there can be only one per computer").arg(other);
      }
      else if (other != 0)
      {
        m_rpcbind.unset(reg.prog, reg.vers); // stale entry
        ok = m_rpcbind.set(reg.prog, reg.vers, reg.port);
      }
    }
    if (!ok)
    {
      if (m_error.isEmpty())
        m_error = m_rpcbind.via() == RpcbindClient::Via::Tcp
                    ? tr("rpcbind refused the registration over TCP; it takes it over its Unix "
                         "socket (/run/rpcbind.sock), which is missing")
                    : tr("rpcbind refused the registration");
      for (const Reg &d : std::as_const(done))
        m_rpcbind.unset(d.prog, d.vers);
      return false;
    }
    done << reg;
  }
  return true;
}

void Vxi11Server::stop()
{
  if (m_systemRpcbind)
  {
    for (const Reg &reg : std::as_const(m_regs))
      m_rpcbind.unset(reg.prog, reg.vers);
    m_systemRpcbind = false;
  }
  m_pmapUdp.close();
  m_pmapTcp.close();
  m_abortSrv.close();
  m_coreSrv.close();
  m_core.reset();
  m_running = false;
  m_pmapPort = 0;
}
