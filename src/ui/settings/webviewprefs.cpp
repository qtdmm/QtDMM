// Copyright (c) 2026 The QtDMM developers
// SPDX-License-Identifier: GPL-3.0-or-later
#include "ui/settings/webviewprefs.h"

#include <QApplication>
#include <QCheckBox>
#include <QClipboard>
#include <QComboBox>
#include <QDoubleSpinBox>
#include <QFormLayout>
#include <QLabel>
#include <QMessageBox>
#include <QPushButton>
#include <QSpinBox>

#include "core/settings.h"
#include "service/webviewserver.h"

WebViewPrefs::WebViewPrefs(QWidget *parent) : SettingsPage(parent)
{
  m_label = tr("Web view");
  m_description = tr("Shows the windows of QtDMM in a browser, to look at only: a second screen, a colleague, a kiosk.");
  m_iconName = "image-x-generic";

  QFormLayout *form = createForm();
  addSection(form, tr("Web view"));
  m_enabled = new QCheckBox(tr("&Enable the web view"), this);
  m_enabled->setToolTip(tr("A browser that opens the link sees the picture of the window area (the devices, graphs "
                           "and nodes, without sidebar and toolbar), new every interval. Nobody can operate "
                           "QtDMM through it."));
  form->addRow(QString(), m_enabled);
  m_port = new QSpinBox(this);
  m_port->setRange(1, 65535);
  m_port->setValue(WebViewServer::kDefaultPort);
  m_port->setToolTip(tr("When it is taken (another QtDMM instance), the next free one is used; the link names it."));
  auto *portLabel = new QLabel(tr("&Port:"), this);
  portLabel->setBuddy(m_port);
  form->addRow(portLabel, m_port);
  m_bind = new QComboBox(this);
  m_bind->addItem(tr("This computer only (localhost)"));
  m_bind->addItem(tr("All network interfaces"));
  m_bind->setToolTip(tr("Whoever has the link sees the picture. The link travels without encryption (HTTP): "
                        "open it to the network only where you trust it, not to the internet."));
  auto *bindLabel = new QLabel(tr("&Listen on:"), this);
  bindLabel->setBuddy(m_bind);
  form->addRow(bindLabel, m_bind);
  m_interval = new QDoubleSpinBox(this);
  m_interval->setDecimals(1);
  m_interval->setRange(WebViewServer::kMinInterval / 1000.0, WebViewServer::kMaxInterval / 1000.0);
  m_interval->setSingleStep(0.1);
  m_interval->setSuffix(" s");
  m_interval->setValue(WebViewServer::kDefaultInterval / 1000.0);
  m_interval->setToolTip(tr("How often the browser asks for a new picture. A picture is made only while somebody "
                            "watches, once for all of them."));
  auto *intervalLabel = new QLabel(tr("&New picture every:"), this);
  intervalLabel->setBuddy(m_interval);
  form->addRow(intervalLabel, m_interval);

  addSection(form, tr("Link"));
  m_link = new QLabel(this);
  m_link->setWordWrap(true);
  m_link->setTextInteractionFlags(Qt::TextSelectableByMouse);
  form->addRow(QString(), m_link);
  m_copy = new QPushButton(QIcon::fromTheme("edit-copy"), tr("&Copy link"), this);
  m_newLink = new QPushButton(tr("New lin&k"), this);
  m_newLink->setToolTip(tr("A new secret in the link: every link given out so far stops working at once."));
  form->addRow(QString(), row({ m_copy, m_newLink }));
  auto *hint = new QLabel(tr("The link holds a secret that stays the same: keep it as a bookmark or on a kiosk screen. "
                             "Whoever has it can watch; \"New link\" ends all links given out. Without a screen: "
                             "<code>QTDMM:WEB:URL?</code> over SCPI."),
                          this);
  hint->setWordWrap(true);
  form->addRow(QString(), hint);

  connect(m_enabled, &QCheckBox::toggled, this, &WebViewPrefs::updateEnabled);
  connect(m_copy, &QPushButton::clicked, this, [this] { QApplication::clipboard()->setText(m_link->text()); });
  connect(m_newLink, &QPushButton::clicked, this, [this]
  {
    if (QMessageBox::question(this, tr("New link"),
                              tr("Every link given out so far stops working when you apply the settings. "
                                 "Make a new one?"))
        != QMessageBox::Yes)
      return;
    m_token = WebViewServer::newToken();
    m_link->setText(tr("A new link, in use after Apply."));
    m_copy->setEnabled(false);
  });
  updateEnabled();
}

bool WebViewPrefs::enabled() const { return m_enabled->isChecked(); }
int WebViewPrefs::port() const { return m_port->value(); }
bool WebViewPrefs::allInterfaces() const { return m_bind->currentIndex() == 1; }
int WebViewPrefs::interval() const { return qRound(m_interval->value() * 1000); }

QString WebViewPrefs::token() const
{
  if (m_token.isEmpty() && enabled())
    m_token = WebViewServer::newToken();
  return m_token;
}

void WebViewPrefs::setLink(const QString &url, const QString &error)
{
  m_link->setText(!url.isEmpty()     ? url
                  : !error.isEmpty() ? tr("The web view could not start: %1.").arg(error)
                                     : tr("The web view is off."));
  m_copy->setEnabled(!url.isEmpty());
}

void WebViewPrefs::updateEnabled()
{
  const bool on = m_enabled->isChecked();
  m_port->setEnabled(on);
  m_bind->setEnabled(on);
  m_interval->setEnabled(on);
  m_newLink->setEnabled(on);
}

void WebViewPrefs::defaultsSLOT()
{
  m_enabled->setChecked(m_cfg->getBool("WebView/enabled", false));
  m_port->setValue(m_cfg->getInt("WebView/port", WebViewServer::kDefaultPort));
  m_bind->setCurrentIndex(m_cfg->getBool("WebView/all-interfaces", false) ? 1 : 0);
  m_interval->setValue(m_cfg->getInt("WebView/interval", WebViewServer::kDefaultInterval) / 1000.0);
  m_token = m_cfg->getString("WebView/token");
  updateEnabled();
}

void WebViewPrefs::factoryDefaultsSLOT()
{
  // the token stays: a reset of the settings is no reason to end the links
  m_enabled->setChecked(false);
  m_port->setValue(WebViewServer::kDefaultPort);
  m_bind->setCurrentIndex(0);
  m_interval->setValue(WebViewServer::kDefaultInterval / 1000.0);
  updateEnabled();
}

void WebViewPrefs::applySLOT()
{
  m_cfg->setBool("WebView/enabled", enabled());
  m_cfg->setInt("WebView/port", port());
  m_cfg->setBool("WebView/all-interfaces", allInterfaces());
  m_cfg->setInt("WebView/interval", interval());
  m_cfg->setString("WebView/token", token());
}
