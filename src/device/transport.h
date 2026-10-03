#pragma once
#include <QtCore>
#include <QIODevice>

#include "device/dmmdecoder.h"

class SharedStateManager;

/// Owns the QIODevice a meter is talked to through and hides which kind it is.
///
/// The concrete devices live in src/device/transports/: a real serial port
/// (SerialDevice), a USB HID cable (HIDSerialDevice), a Holtek HID sensor
/// (HidHoltekDevice), a serial port shared
/// over the network (RFC2217SerialDevice), sigrok-cli as a data source
/// (SigrokDevice) and a formula over other instances' readings
/// (CalcDevice). MeterConnection only ever sees the QIODevice returned by port().
class Transport : public QObject
{
    Q_OBJECT
public:
    /// The kinds of port; the settings file stores them by name (str2portType()).
    enum class PortType { None, Serial, Hid, HidHoltek, Sigrok, RFC2217, Calc, Ble, BleGatt };

    explicit Transport(QObject *parent = Q_NULLPTR) : QObject(parent) {}
    /// Creates the device object for @p type (closing any previous one). The
    /// port is not opened yet; call port()->open() and then init().
    bool create(const DmmDecoder::DMMInfo spec, PortType type, QString device);
    /// The instance coordinator a CalcDevice reads its inputs from.
    void setStateManager(SharedStateManager *state) { m_state = state; }
    /// Closes and deletes the current device, if any.
    void close();

    /// The current device, or null before create()/after close().
    QIODevice*  port()     const { return m_port;  }
    PortType    portType() const { return m_type; }
    /// Everything that can be connected to, across all port types, in the
    /// form the DMM settings page offers ("/dev/ttyUSB0", "hid:...", ...).
    static QStringList   availablePorts();
    /// Last error of the device as a QSerialPort::SerialPortError; only the
    /// serial device reports errors this way, the others return 0.
    int         error();
    /// Applies the DMMInfo line settings (baud, parity, DTR/RTS) after open().
    bool        init();
    bool        isOpen() const { return m_port != Q_NULLPTR && m_port->isOpen(); };

    /// "serial", "hid", "hidholtek", "sigrok", "rfc2217", "calc" or "ble" (case-insensitive) to PortType.
    static PortType str2portType(const QString str);
private:
    QIODevice *m_port   = Q_NULLPTR;
    PortType   m_type   = PortType::None;
    QString    m_device;
    SharedStateManager *m_state = Q_NULLPTR;
};
