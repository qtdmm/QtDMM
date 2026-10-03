#include "device/transport.h"
#include "device/transports/serial.h"
#include "device/transports/hidserial.h"
#include "device/transports/hidholtek.h"
#include "device/transports/rfc2217serial.h"
#include "device/transports/sigrok.h"
#include "device/transports/calc.h"
#ifdef QTDMM_WITH_BLE
#include "device/transports/ble.h"
#include "device/transports/blegatt.h"
#endif

#include <QSerialPortInfo>

bool Transport::create(const DmmDecoder::DMMInfo spec, PortType t, QString device)
{
  close();

  switch (t)
  {
    case PortType::Serial: m_port = new SerialDevice(spec, device);      break;
    case PortType::Hid:    m_port = new HIDSerialDevice(spec, device);   break;
    case PortType::HidHoltek: m_port = new HidHoltekDevice(spec, device); break;
    case PortType::Sigrok: m_port = new SigrokDevice(spec,device);       break;
    case PortType::RFC2217:m_port = new RFC2217SerialDevice(spec,device);break;
    case PortType::Calc:   m_port = new CalcDevice(spec, device, m_state); break;
#ifdef QTDMM_WITH_BLE
    case PortType::Ble:    m_port = new BleAdvertisementDevice(spec, device); break;
    case PortType::BleGatt: m_port = new BleGattDevice(spec, device); break;
#endif
    default: return false;
  }

  m_type   = t;
  m_device = device;
  return true;
}

void Transport::close()
{
  if (!m_port) return;
  // forget the port before closing it: close() may emit signals whose
  // handlers ask port() and must not see the dying device
  QIODevice *port = m_port;
  m_port   = Q_NULLPTR;
  m_type   = PortType::None;
  m_device = "";
  port->close();
  port->deleteLater();
}

int Transport::error()
{
  switch (m_type)
  {
    case PortType::Serial: return static_cast<SerialDevice *>(m_port)->error();
    case PortType::Hid:    return 0;
    case PortType::Sigrok: return 0;
    case PortType::RFC2217:return 0;
    default:               return 0;
  }
}

Transport::PortType Transport::str2portType(const QString str)
{
  if (str.toLower() == "serial")  return PortType::Serial;
  if (str.toLower() == "hid")     return PortType::Hid;
  if (str.toLower() == "hidholtek") return PortType::HidHoltek;
  if (str.toLower() == "sigrok")  return PortType::Sigrok;
  if (str.toLower() == "rfc2217") return PortType::RFC2217;
  if (str.toLower() == "calc")    return PortType::Calc;
  if (str.toLower() == "ble")     return PortType::Ble;
  if (str.toLower() == "blegatt") return PortType::BleGatt;

  return PortType::None;
}

bool Transport::init()
{
  switch (m_type)
  {
    case PortType::Serial: return static_cast<SerialDevice *>(m_port)->init();
    case PortType::Hid:    return true;
    case PortType::Sigrok: return  static_cast<SigrokDevice *>(m_port)->init();
    default:               return true;
  }
}

QStringList Transport::availablePorts()
{
  QStringList portlist;
  SerialDevice::availablePorts(portlist);
  HIDSerialDevice::availablePorts(portlist);
  HidHoltekDevice::availablePorts(portlist);
  RFC2217SerialDevice::availablePorts(portlist);
  SigrokDevice::availablePorts(portlist);

  return portlist;
}
