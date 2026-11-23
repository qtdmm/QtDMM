// Copyright (c) 2026 The QtDMM developers
// SPDX-License-Identifier: GPL-3.0-or-later
#include "service/servicehost.h"

#include <QDebug>
#include <QHostInfo>

#include "service/hislipserver.h"
#include "service/mdnsresponder.h"
#include "service/scpiserver.h"
#include "service/webviewserver.h"

namespace
{
// --vxi11-rpc: replaces the setting Scpi/vxi11-rpc for this run
std::optional<Vxi11Server::RpcbindMode> s_vxi11RpcOverride;
// --vxi11-portmapper-port: an own portmapper on this port for this run (tests)
std::optional<quint16> s_vxi11PortmapperPort;
}

ServiceHost::ServiceHost(QObject *parent)
  : QObject(parent)
  , m_scpi(new ScpiServer(this))
  , m_mdns(new MdnsResponder(this))
  , m_hislip(new HislipServer(m_scpi->endpoint(), this))
  , m_mdnsHislip(new MdnsResponder(this))
  , m_vxi11(new Vxi11Server(m_scpi->endpoint(), this))
  , m_mdnsVxi11(new MdnsResponder(this))
  , m_webView(new WebViewServer(this))
{
  connect(m_scpi, &ScpiServer::clientsChanged, this, [this](int) { updateStatus(); });
  connect(m_hislip, &HislipServer::sessionsChanged, this, [this](int) { updateStatus(); });
  connect(m_vxi11, &Vxi11Server::linksChanged, this, [this](int) { updateStatus(); });
  connect(m_webView, &WebViewServer::viewersChanged, this, [this](int) { updateStatus(); });
  m_scpi->setWebViewUrlSource([this] { return webViewUrl(); });
}

ServiceHost::~ServiceHost() = default;

void ServiceHost::setScreenshotSource(std::function<QByteArray(const QByteArray &)> source)
{
  m_scpi->setScreenshotSource(std::move(source));
}

void ServiceHost::apply(const ScpiConfig &config, const QString &instanceId, const QString &model)
{
  m_instanceId = instanceId;
  m_model = model;
  const QHostAddress address = config.allInterfaces ? QHostAddress::Any : QHostAddress::LocalHost;
  // keep a running server when nothing about it changed: clients stay
  const bool same = m_scpi->isListening() && m_scpi->address() == address
                    && m_scpi->port() >= config.port && m_scpi->port() < config.port + 10;
  if (!config.enabled)
  {
    m_mdns->stop();
    m_scpi->stop();
    m_scpiError.clear();
  }
  else if (!same)
  {
    m_mdns->stop();
    m_scpiError.clear();
    if (!m_scpi->start(address, config.port))
    {
      m_scpiError = m_scpi->errorString();
      Q_EMIT error(tr("SCPI server: %1").arg(m_scpiError));
    }
  }

  // HiSLIP: the same interpreter for VISA, next to the raw socket; another
  // instance takes the next port, as for the raw socket
  const bool hislip = config.enabled && config.hislip && m_scpi->isListening();
  const bool sameHislip = m_hislip->isListening() && m_hislip->address() == address
                          && m_hislip->port() >= config.hislipPort && m_hislip->port() < config.hislipPort + 10;
  if (!hislip)
  {
    m_mdnsHislip->stop();
    m_hislip->stop();
    m_hislipError = config.enabled && config.hislip ? tr("the SCPI server is not running") : QString();
  }
  else if (!sameHislip)
  {
    m_mdnsHislip->stop();
    m_hislipError.clear();
    if (!m_hislip->start(address, config.hislipPort, 10))
    {
      m_hislipError = m_hislip->error();
      Q_EMIT error(tr("HiSLIP server: %1").arg(m_hislipError));
    }
  }

  // VXI-11: one per computer (one portmapper entry), with the system rpcbind
  // or an own portmapper on 111; switched on but not possible, it stays off
  // with a reason, the other two run on
  const bool vxi11 = config.enabled && config.vxi11 && m_scpi->isListening();
  const Vxi11Server::RpcbindMode rpc =
    s_vxi11PortmapperPort ? Vxi11Server::RpcbindMode::Own : s_vxi11RpcOverride.value_or(config.vxi11Rpc);
  const bool sameVxi11 = m_vxi11->isRunning() && m_vxi11->address() == address && m_vxi11->mode() == rpc;
  if (!vxi11)
  {
    m_mdnsVxi11->stop();
    m_vxi11->stop();
    m_vxi11Error = config.enabled && config.vxi11 ? tr("the SCPI server is not running") : QString();
  }
  else if (!sameVxi11)
  {
    m_mdnsVxi11->stop();
    m_vxi11Error.clear();
    if (!m_vxi11->start(address, rpc, 0, 0, s_vxi11PortmapperPort.value_or(Vxi11Server::kPortmapperPort)))
    {
      m_vxi11Error = m_vxi11->error();
      Q_EMIT error(tr("VXI-11 server: %1").arg(m_vxi11Error));
    }
  }

  QMap<QString, QString> txt;
  txt["txtvers"] = "1";
  txt["model"] = m_model;
  txt["version"] = APP_VERSION;
  txt["instance"] = m_instanceId;
  const QString instance = QString("QtDMM %1").arg(m_instanceId == "default"
                                                   ? QHostInfo::localHostName() : m_instanceId);
  if (m_scpi->isListening() && config.mdns && !m_mdns->isActive())
    m_mdns->start("_scpi-raw._tcp", instance, m_scpi->port(), txt);
  else if (!config.mdns)
    m_mdns->stop();
  if (m_hislip->isListening() && config.mdns && !m_mdnsHislip->isActive())
    m_mdnsHislip->start("_hislip._tcp", instance, m_hislip->port(), txt);
  else if (!config.mdns)
    m_mdnsHislip->stop();
  // VXI-11 clients ask the portmapper: the announced port is 111
  if (m_vxi11->isRunning() && config.mdns && !m_mdnsVxi11->isActive())
    m_mdnsVxi11->start("_vxi-11._tcp", instance, Vxi11Server::kPortmapperPort, txt);
  else if (!config.mdns)
    m_mdnsVxi11->stop();
  updateStatus();
}

void ServiceHost::updateStatus()
{
  if (!m_scpi->isListening())
  {
    Q_EMIT statusChanged(m_scpiError.isEmpty() ? tr("The server is not running.")
                                                   : tr("The server could not start: %1.").arg(m_scpiError));
    return;
  }
  const QString where = m_scpi->address() == QHostAddress::LocalHost ? QString("localhost") : QHostInfo::localHostName();
  const int n = m_scpi->clientCount() + m_hislip->sessionCount() + m_vxi11->linkCount();
  const QString clients = n == 1 ? tr("1 client") : tr("%1 clients").arg(n);
  QString text = tr("Listening on %1, port %2, %3 connected.").arg(where).arg(m_scpi->port()).arg(clients);
  if (m_hislip->isListening())
    text += ' ' + tr("HiSLIP on port %1: %2.").arg(m_hislip->port()).arg(hislipResource());
  else if (!m_hislipError.isEmpty())
    text += ' ' + tr("HiSLIP could not start: %1.").arg(m_hislipError);
  if (m_vxi11->isRunning())
    text += ' ' + (m_vxi11->usesSystemRpcbind()
                     ? tr("VXI-11 through the system rpcbind: %1.").arg(vxi11Resource())
                     : tr("VXI-11 with an own portmapper on port %2: %1.").arg(vxi11Resource())
                         .arg(m_vxi11->portmapperPort()));
  else if (!m_vxi11Error.isEmpty())
    text += ' ' + tr("VXI-11 is off: %1.").arg(m_vxi11Error);
  if (m_mdns->isActive())
    text += ' ' + tr("Announced as \"%1\".").arg(m_mdns->instanceName());
  Q_EMIT statusChanged(text);
}

void ServiceHost::applyWebView(const WebViewConfig &config)
{
  const QHostAddress address = config.allInterfaces ? QHostAddress::Any : QHostAddress::LocalHost;
  m_webView->setInterval(config.interval);
  if (!config.enabled || config.token.isEmpty())
  {
    m_webView->stop();
    m_webViewError.clear();
  }
  else if (m_webView->isRunning() && m_webView->address() == address
           && m_webView->port() >= config.port && m_webView->port() < config.port + 10)
    m_webView->setToken(config.token);   // the same server: browsers stay
  else
  {
    m_webView->stop();
    m_webViewError.clear();
    for (int port = config.port; port < config.port + 10 && port <= 65535; ++port)
      if (m_webView->start(address, quint16(port), config.token))
        break;
    if (!m_webView->isRunning())
    {
      m_webViewError = m_webView->error();
      Q_EMIT error(tr("The web view could not start: %1").arg(m_webViewError));
    }
    else
      qInfo().noquote() << "Web view:" << webViewUrl();
  }
  updateStatus();
}

QString ServiceHost::webViewUrl() const
{
  if (!m_webView->isRunning())
    return {};
  if (m_webView->address() == QHostAddress::LocalHost)
    return m_webView->url("localhost").toString();
  const QList<HttpServer::Address> lan = HttpServer::addresses();
  return m_webView->url(lan.isEmpty() ? QHostInfo::localHostName() : lan.first().address).toString();
}

QList<ServiceState> ServiceHost::services() const
{
  ServiceState scpi;
  scpi.id = "scpi";
  scpi.name = "SCPI";
  scpi.running = m_scpi->isListening();
  if (scpi.running)
  {
    const QString where = m_scpi->address() == QHostAddress::LocalHost ? QString("localhost") : QHostInfo::localHostName();
    scpi.port = m_scpi->port();
    scpi.address = QString("%1:%2").arg(where).arg(scpi.port);
    scpi.clients = m_scpi->clientCount();
  }
  else
    scpi.error = m_scpiError;
  ServiceState hislip;
  hislip.id = "hislip";
  hislip.name = "HiSLIP";
  hislip.running = m_hislip->isListening();
  if (hislip.running)
  {
    hislip.port = m_hislip->port();
    hislip.address = hislipResource();
    hislip.clients = m_hislip->sessionCount();
  }
  else
    hislip.error = m_hislipError;
  ServiceState vxi11;
  vxi11.id = "vxi11";
  vxi11.name = "VXI-11";
  vxi11.running = m_vxi11->isRunning();
  if (vxi11.running)
  {
    vxi11.port = m_vxi11->portmapperPort();
    vxi11.address = vxi11Resource();
    vxi11.clients = m_vxi11->linkCount();
  }
  else
    vxi11.error = m_vxi11Error;
  ServiceState web;
  web.id = "webview";
  web.name = tr("Web view");
  web.running = m_webView->isRunning();
  if (web.running)
  {
    web.port = m_webView->port();
    web.address = webViewUrl();
    web.clients = m_webView->viewers();
  }
  else
    web.error = m_webViewError;
  return { scpi, hislip, vxi11, web };
}

QString ServiceHost::vxi11Resource() const
{
  if (!m_vxi11->isRunning())
    return QString();
  const QString host = m_vxi11->address() == QHostAddress::LocalHost ? QString("localhost") : QHostInfo::localHostName();
  return QString("TCPIP::%1::INSTR").arg(host);
}

void ServiceHost::setVxi11RpcOverride(std::optional<Vxi11Server::RpcbindMode> mode)
{
  s_vxi11RpcOverride = mode;
}

void ServiceHost::setVxi11PortmapperPort(std::optional<quint16> port)
{
  s_vxi11PortmapperPort = port;
}

QString ServiceHost::hislipResource() const
{
  if (!m_hislip->isListening())
    return QString();
  const QString host = m_hislip->address() == QHostAddress::LocalHost ? QString("localhost") : QHostInfo::localHostName();
  // the port only when it is not the standard one
  if (m_hislip->port() == HislipServer::kDefaultPort)
    return QString("TCPIP::%1::hislip0::INSTR").arg(host);
  return QString("TCPIP::%1::hislip0,%2::INSTR").arg(host).arg(m_hislip->port());
}
