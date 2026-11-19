// Copyright (c) 2026 The QtDMM developers
// SPDX-License-Identifier: GPL-3.0-or-later
#include "ui/devicesummary.h"

#include <QHBoxLayout>
#include <QLabel>

#include "device/devicetype.h"
#include "device/protocols.h"
#include "ui/devicesettings.h"
#include "ui/devicesymbol.h"

namespace
{
const QSize kSymbolSize(130, 150);
}

DeviceSummary::DeviceSummary(QWidget *parent) : QWidget(parent)
{
  auto *about = new QHBoxLayout(this);
  about->setContentsMargins(0, 0, 0, 0);
  m_symbol = new QLabel(this);
  m_symbol->setObjectName("ui_deviceSymbol");
  m_symbol->setFixedSize(kSymbolSize);
  m_symbol->setAlignment(Qt::AlignCenter);
  m_info = new QLabel(this);
  m_info->setObjectName("ui_deviceInfo");
  m_info->setTextFormat(Qt::RichText);
  m_info->setWordWrap(true);
  m_info->setTextInteractionFlags(Qt::TextSelectableByMouse);
  about->addStretch();
  about->addWidget(m_symbol);
  about->addSpacing(fontMetrics().averageCharWidth() * 3);
  about->addWidget(m_info, 1);
  about->addStretch();
}

QString DeviceSummary::connection(const DeviceSettings *settings)
{
  // how it is reached: a Bluetooth address, sigrok, or the port with its
  // serial parameters
  const QString device = settings->device();
  if (settings->isBluetooth())
    // the address only: a Victron device string carries its key after it
    return tr("Bluetooth LE, %1").arg(device.section(' ', 1, 1));
  if (settings->isSigrokMeter())
    return tr("sigrok-cli, %1").arg(device.section(' ', 1));
  // "SERIAL /dev/ttyUSB0" -> "/dev/ttyUSB0", "HID 0x1a86:0xe008 /dev/hidraw3" (or
  // HIDHOLTEK, the TFA sensor) -> "USB-HID /dev/hidraw3", without serial parameters
  const QString kind = device.section(' ', 0, 0).toUpper();
  const bool hid = kind.startsWith(QLatin1String("HID"));
  QString text = hid ? tr("USB-HID %1").arg(device.section(' ', -1))
                 : kind == QLatin1String("SERIAL") ? device.section(' ', 1) : device;
  if (settings->speed() > 0 && !hid)
  {
    const char parity = settings->parity() == QSerialPort::EvenParity ? 'E'
                        : settings->parity() == QSerialPort::OddParity ? 'O' : 'N';
    text += QString(", %1 %2%3%4").arg(settings->speed()).arg(int(settings->bits())).arg(parity)
              .arg(settings->stopBits() == QSerialPort::TwoStop ? 2 : 1);
  }
  return text;
}

void DeviceSummary::show(const DeviceSettings *settings, const QString &name)
{
  const DmmDecoder::DMMInfo info = settings->dmmInfo();
  const bool manual = settings->keys().value("DMM/model").toString() == QLatin1String("Manual");
  const DeviceType::Type type = DeviceType::of(info);
  const qreal dpr = devicePixelRatioF();
  m_symbol->setPixmap(manual ? DeviceSymbol::unknown(kSymbolSize, dpr)
                             : DeviceSymbol::pixmap(info.vendor, type, kSymbolSize, dpr));

  QString model = manual ? tr("Manual settings") : info.name;
  if (model.endsWith(" *"))
    model.chop(2);
  const QList<std::pair<QString, QString>> rows = {
    { tr("Name"), name },
    { tr("Model"), model },
    { tr("Type"), manual ? QStringLiteral("-") : DeviceType::displayName(type) },
    { tr("Protocol"), FrameFormat::toString(settings->format()) },
    { tr("Connection"), connection(settings) },
  };
  QString html = "<table cellspacing=\"0\" cellpadding=\"2\">";
  for (const auto &[label, value] : rows)
    html += QString("<tr><td><b>%1</b>&nbsp;&nbsp;</td><td>%2</td></tr>").arg(label.toHtmlEscaped(), value.toHtmlEscaped());
  m_info->setText(html + "</table>");
}
