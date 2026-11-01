// Copyright (c) 2026 The QtDMM developers
// SPDX-License-Identifier: GPL-3.0-or-later
#include "ui/dialogs/devicesettingsdlg.h"

#include <QDesktopServices>
#include <QDialogButtonBox>
#include <QHBoxLayout>
#include <QFrame>
#include <QLabel>
#include <QPushButton>
#include <QTabWidget>
#include <QTextBrowser>
#include <QUrl>
#include <QVBoxLayout>

#include "ui/devicesettings.h"
#include "ui/devicespecs.h"
#include "ui/devicesummary.h"
#include "ui/devicesymbol.h"

DeviceSettingsDlg::DeviceSettingsDlg(const QString &name, QWidget *parent)
  : QDialog(parent),
    m_name(name)
{
  setWindowTitle(name);
  auto *layout = new QVBoxLayout(this);
  m_tabs = new QTabWidget(this);
  layout->addWidget(m_tabs, 1);

  // Info: the device at a glance, its state now, where to read more
  auto *info = m_info = new QWidget(m_tabs);
  auto *infoLayout = new QVBoxLayout(info);
  m_summary = new DeviceSummary(info);
  infoLayout->addWidget(m_summary);
  auto *line = new QFrame(info);
  line->setFrameShape(QFrame::HLine);
  line->setFrameShadow(QFrame::Sunken);
  infoLayout->addWidget(line);
  m_status = new QLabel(info);
  m_status->setObjectName("ui_deviceStatus");
  m_status->setTextFormat(Qt::RichText);
  m_status->setWordWrap(true);
  m_status->setTextInteractionFlags(Qt::TextSelectableByMouse);
  infoLayout->addWidget(m_status);
  infoLayout->addStretch(1);
  m_links = new QLabel(info);
  m_links->setObjectName("ui_deviceLinks");
  m_links->setTextFormat(Qt::RichText);
  m_links->setTextInteractionFlags(Qt::TextBrowserInteraction);
  // the maker's website in the browser, the help page in QtDMM's help
  connect(m_links, &QLabel::linkActivated, this, [this](const QString &link)
  {
    if (link.startsWith(QLatin1String("help:")))
      Q_EMIT helpRequested(link.mid(5));
    else
      QDesktopServices::openUrl(QUrl(link));
  });
  infoLayout->addWidget(m_links);
  m_tabs->addTab(info, tr("&Info"));

  // Data: the manual's specifications, a tab only for the models that have them
  m_data = new QTextBrowser(m_tabs);
  m_data->setObjectName("ui_deviceData");
  m_data->setOpenExternalLinks(true);
  m_tabs->addTab(m_data, tr("&Data"));

  m_settings = new DeviceSettings(m_tabs);
  m_settings->setDescriptionFilesVisible(false);
  m_tabs->addTab(m_settings, tr("&Settings"));

  auto *buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
  m_ok = buttons->button(QDialogButtonBox::Ok);
  // Connect at the bottom left, set up by setConnect()
  m_connect = new QPushButton(QIcon::fromTheme("network-connect"), tr("&Connect"), this);
  m_connect->setObjectName("ui_connect");
  m_connect->setToolTip(tr("Reads it beside the devices connected: recorded with them, in formulas and over SCPI"));
  m_connect->hide();
  connect(buttons, &QDialogButtonBox::accepted, this, &QDialog::accept);
  connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
  connect(m_settings, &DeviceSettings::changed, this, [this]
  {
    m_ok->setEnabled(m_settings->isComplete());
    updateInfo();
  });
  auto *row = new QHBoxLayout;
  row->addWidget(m_connect);
  row->addStretch(1);
  row->addWidget(buttons);
  layout->addLayout(row);

  m_tick.setInterval(1000);
  connect(&m_tick, &QTimer::timeout, this, &DeviceSettingsDlg::updateStatus);
  resize(620, 480);
}

void DeviceSettingsDlg::load(const QVariantMap &keys)
{
  m_settings->load(keys);
  m_ok->setEnabled(m_settings->isComplete());
  updateInfo();
}

QVariantMap DeviceSettingsDlg::keys() const
{
  return m_settings->keys();
}

void DeviceSettingsDlg::showTab(Tab tab)
{
  if (tab == DataTab && !hasData())
    tab = InfoTab;
  m_tabs->setCurrentWidget(tab == InfoTab ? m_info : tab == DataTab ? static_cast<QWidget *>(m_data) : m_settings);
}

DeviceSettingsDlg::Tab DeviceSettingsDlg::currentTab() const
{
  const QWidget *w = m_tabs->currentWidget();
  return w == m_info ? InfoTab : w == m_data ? DataTab : SettingsTab;
}

bool DeviceSettingsDlg::hasData() const
{
  return m_tabs->isTabVisible(m_tabs->indexOf(m_data));
}

void DeviceSettingsDlg::setStatus(const std::function<QString()> &status)
{
  m_statusText = status;
  updateStatus();
  if (m_statusText)
    m_tick.start();
  else
    m_tick.stop();
}

void DeviceSettingsDlg::setConnect(const std::function<bool()> &connected, const std::function<void()> &connect)
{
  m_connected = connected;
  m_connect->disconnect(this);
  QObject::connect(m_connect, &QPushButton::clicked, this, [this, connect]
  {
    connect();
    m_connect->setEnabled(!m_connected());
  });
  m_connect->setVisible(true);
  m_connect->setEnabled(!m_connected());
}

QString DeviceSettingsDlg::statusText() const
{
  return m_status->text();
}

QString DeviceSettingsDlg::linksText() const
{
  return m_links->text();
}

void DeviceSettingsDlg::updateInfo()
{
  m_summary->show(m_settings, m_name);
  const QString vendor = m_settings->dmmInfo().vendor;
  const QString model = m_settings->dmmInfo().name;
  if (model != m_dataModel)
  {
    m_dataModel = model;
    const QJsonObject spec = DeviceSpecs::find(vendor, m_settings->dmmInfo().model);
    m_data->setHtml(spec.isEmpty() ? QString() : DeviceSpecs::html(spec));
    m_tabs->setTabVisible(m_tabs->indexOf(m_data), !spec.isEmpty());
  }
  QStringList links;
  const QString site = DeviceSymbol::website(vendor);
  if (!site.isEmpty())
    links << QString("<a href=\"%1\">%2</a>").arg(site.toHtmlEscaped(), tr("Website of %1").arg(vendor).toHtmlEscaped());
  links << QString("<a href=\"help:supported-devices.md\">%1</a>").arg(tr("Supported devices (help)").toHtmlEscaped());
  m_links->setText(links.join("&nbsp;&nbsp;·&nbsp;&nbsp;"));
}

void DeviceSettingsDlg::updateStatus()
{
  if (m_connected)
    m_connect->setEnabled(!m_connected());
  m_status->setText(m_statusText ? m_statusText() : QString());
}
