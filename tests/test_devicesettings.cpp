// Copyright (c) 2026 The QtDMM developers
// SPDX-License-Identifier: GPL-3.0-or-later
//
// The meter's settings widget outside the settings dialog: keys in, keys out,
// "Advanced" closed for a known model and open for manual settings, and the
// group of each kind of meter instead of the port.

#include <QtWidgets>
#include <tuple>

#include "ui/devicesettings.h"
#ifdef QTDMM_WITH_BLE
#include "device/transports/blegatt.h"
#endif
#include "device/dmmdecoder.h"
#include "testutil.h"

static bool shown(DeviceSettings &w, const char *name)
{
  QWidget *child = w.findChild<QWidget *>(name);
  return child && child->isVisibleTo(&w);
}

int main(int argc, char **argv)
{
  if (qEnvironmentVariableIsEmpty("QT_QPA_PLATFORM"))
    qputenv("QT_QPA_PLATFORM", "offscreen");
  QApplication app(argc, argv);

  DeviceSettings w;
  w.setPorts({"/dev/ttyUSB0", "/dev/ttyUSB1"});

  // 1. a known cable meter: keys round trip, only the port row, Advanced closed
  const QVariantMap ut61e {
    {"DMM/model", "Uni-Trend UT61E"}, {"Port settings/device", "/dev/serial/by-id/usb-Prolific-if00-port0"},
    {"Port settings/baud", "19200"}, {"Port settings/bits", "7"}, {"Port settings/stop-bits", "1"},
    {"Port settings/parity", 2}, {"DMM/data-format", "CyrustekES51922"}, {"DMM/display", "22000"},
    {"DMM/rts", false}, {"DMM/dtr", true}, {"DMM/external-setup", false}, {"DMM/number-of-values", 1}};
  w.load(ut61e);
  QVariantMap out = w.keys();
  for (auto it = ut61e.cbegin(); it != ut61e.cend(); ++it)
    check(out.value(it.key()).toString() == it.value().toString(),
          QString("UT61E %1: %2 != %3").arg(it.key(), out.value(it.key()).toString(), it.value().toString()));
  check(w.device() == "/dev/serial/by-id/usb-Prolific-if00-port0", "UT61E keeps a port not in the list: " + w.device());
  check(!w.portParametersShown(), "UT61E: port parameters not shown");
  check(shown(w, "ButtonGroup11") && !w.findChild<QWidget *>("ui_advanced"), "UT61E: port row, no Advanced button");
  check(!shown(w, "ui_advancedBox") && !shown(w, "ui_protocol_title"), "UT61E: port parameters and protocol hidden");

  // a microcontroller: a model, but the baud rate of its sketch to choose
  w.load({{"DMM/model", "Generic Microcontroller (text lines: Serial Plotter, Teleplot)"},
          {"Port settings/device", "/dev/ttyUSB0"}, {"Port settings/baud", "57600"}});
  check(w.portParametersShown() && w.findChild<QComboBox *>("baudRate")->isEnabled()
          && !w.findChild<QComboBox *>("bitsCombo")->isEnabled(),
        "microcontroller: the baud rate shown and free, the rest fixed");
  check(w.speed() == 57600 && w.keys().value("Port settings/baud") == "57600", "microcontroller: 57600 kept");
  w.load({{"DMM/model", "Generic Microcontroller (text lines: Serial Plotter, Teleplot)"}, {"Port settings/device", "/dev/ttyUSB0"}});
  check(w.speed() == 115200, "microcontroller without a saved rate: 115200");

  // 2. manual settings: Advanced open, the model "Manual"
  w.load({{"DMM/model", "Manual"}, {"Port settings/device", "/dev/ttyUSB1"}, {"Port settings/baud", "2400"}});
  check(w.portParametersShown(), "manual: port parameters shown");
  check(shown(w, "ui_serialBox") && shown(w, "ui_protocol") && shown(w, "ui_protocol_title"),
        "manual: port parameters and protocol shown, with its title");
  check(w.keys().value("DMM/model") == "Manual", "manual: model " + w.keys().value("DMM/model").toString());
  check(w.speed() == 2400, "manual: baud");
  // the lines per reading: from the table for a model (a setting saved before
  // 26.3 had the values there), one value per line for manual settings
  w.load({{"DMM/model", "Manual"}, {"DMM/number-of-values", 4}});
  check(w.framesPerReading() == 4 && w.valueCount() == 4, "manual: 4 lines, 4 values");
  w.load({{"DMM/model", "APPA 506B *"}, {"DMM/number-of-values", 2}});
  check(w.framesPerReading() == 1 && w.valueCount() == 2 && w.keys().value("DMM/number-of-values") == 1,
        QString("APPA: one frame, two values, got %1/%2").arg(w.framesPerReading()).arg(w.valueCount()));
  w.load({{"DMM/model", "Manual"}, {"Port settings/device", "/dev/ttyUSB1"}, {"Port settings/baud", "2400"}});
  // a model chosen after manual settings: its fixed parameters go again
  w.findChild<QComboBox *>("ui_vendor")->setCurrentText("Uni-Trend");
  Q_EMIT w.findChild<QComboBox *>("ui_vendor")->activated(w.findChild<QComboBox *>("ui_vendor")->currentIndex());
  check(!w.portParametersShown() && !shown(w, "ui_advancedBox"), "manual, then a model: the fixed parameters hidden");
  {
    // the model list names the kind of instrument in its tooltips
    auto *models = w.findChild<QComboBox *>("ui_model");
    const int ut61e = models->findText("Uni-Trend UT61E");
    const int ut803 = models->findText("Uni-Trend UT803");
    check(ut61e >= 0 && models->itemData(ut61e, Qt::ToolTipRole).toString() == "Handheld multimeter",
          "UT61E tooltip: " + models->itemData(ut61e, Qt::ToolTipRole).toString());
    check(ut803 >= 0 && models->itemData(ut803, Qt::ToolTipRole).toString() == "Bench multimeter",
          "UT803 tooltip: " + models->itemData(ut803, Qt::ToolTipRole).toString());
  }

  // 3. no frames: the groups are sections with a title above
  check(w.findChild<QGroupBox *>("ui_protocol")->title().isEmpty() && w.findChild<QLabel *>("ui_protocol_title"),
        "sections: the title above the group");

  // 4. simulated and calculated values are logic nodes with dialogs of their
  // own (GeneratorNodeDlg, FormulaNodeDlg): no models of QtDMM here
  check(!w.findChild<QComboBox *>("ui_vendor") || w.findChild<QComboBox *>("ui_vendor")->findText("QtDMM") < 0,
        "no vendor QtDMM among the meters");

  // 6. Victron: the Bluetooth group instead of the port, the key only here
  w.load({{"DMM/model", "Victron SmartShunt"}, {"Port settings/device", "ble AA:BB:CC:DD:EE:FF"},
          {"Port settings/ble-address", "AA:BB:CC:DD:EE:FF"}, {"Port settings/ble-key", "00112233445566778899aabbccddeeff"}});
  check(w.isBluetooth() && shown(w, "ui_bleGroup") && !shown(w, "ButtonGroup11"), "Victron: Bluetooth group");
  check(w.keys().value("Port settings/device") == "ble AA:BB:CC:DD:EE:FF", "Victron: place " + w.keys().value("Port settings/device").toString());
  check(w.keys().value("Port settings/ble-key") == "00112233445566778899aabbccddeeff", "Victron: key kept");
  check(w.framesPerReading() == 1 && w.valueCount() == 2, "Victron: one frame, two values");
  // the chosen values of the stored settings, also when the same model was
  // loaded before (another device taken over, the settings dialog reloaded)
  for (const auto &[main, second] : { std::pair{"I", "SOC"}, std::pair{"SOC", "-"}, std::pair{"P", "V"} })
  {
    w.load({{"DMM/model", "Victron SmartShunt"}, {"Port settings/device", "ble AA:BB:CC:DD:EE:FF"},
            {"Port settings/ble-address", "AA:BB:CC:DD:EE:FF"}, {"Port settings/ble-key", "00112233445566778899aabbccddeeff"},
            {"Port settings/ble-main", main}, {"Port settings/ble-second", second}});
    const QString fields = w.device().section(' ', 3);
    check(fields == QString("%1 %2").arg(main, second), QString("Victron: values %1 %2, got %3").arg(main, second, fields));
  }
  // a third and fourth value: in the port string only when chosen, and the
  // number of values follows them
  check(w.valueCount() == 2 && w.keys().value("Port settings/ble-third") == "-", "Victron: no third value at first");
  for (const auto &[third, fourth, tail, count] : { std::tuple{"SOC", "-", "P V SOC -", 3}, std::tuple{"-", "AH", "P V - AH", 4},
                                                   std::tuple{"SOC", "AH", "P V SOC AH", 4}, std::tuple{"-", "-", "P V", 2} })
  {
    w.load({{"DMM/model", "Victron SmartShunt"}, {"Port settings/device", "ble AA:BB:CC:DD:EE:FF"},
            {"Port settings/ble-address", "AA:BB:CC:DD:EE:FF"}, {"Port settings/ble-key", "00112233445566778899aabbccddeeff"},
            {"Port settings/ble-main", "P"}, {"Port settings/ble-second", "V"},
            {"Port settings/ble-third", third}, {"Port settings/ble-fourth", fourth}});
    const QString fields = w.device().section(' ', 3);
    check(fields == tail && w.valueCount() == count,
          QString("Victron: %1 %2 gives \"%3\" and %4 values").arg(third, fourth, fields).arg(w.valueCount()));
    check(w.keys().value("Port settings/ble-third") == third && w.keys().value("Port settings/ble-fourth") == fourth,
          "Victron: third and fourth kept");
  }
  // another Victron model taken over straight after: its own choice, not the
  // one of the model before mapped onto its fields
  w.load({{"DMM/model", "Victron SmartSolar MPPT"}, {"Port settings/device", "ble AA:BB:CC:DD:EE:01"},
          {"Port settings/ble-address", "AA:BB:CC:DD:EE:01"}, {"Port settings/ble-key", "00112233445566778899aabbccddeeff"},
          {"Port settings/ble-main", "PV"}, {"Port settings/ble-second", "V"},
          {"Port settings/ble-third", "YIELD"}, {"Port settings/ble-fourth", "I"}});
  check(w.device().section(' ', 3) == "PV V YIELD I", "Victron: MPPT after SmartShunt, got " + w.device().section(' ', 3));

#ifdef QTDMM_WITH_BLE
  // 6a. APPA over Bluetooth LE: a GATT meter, the address alone; the same
  // meter on the optical cable stays a serial one
  w.load({{"DMM/model", "APPA S1 *"}, {"Port settings/device", "blegatt AA:BB:CC:DD:EE:01"},
          {"Port settings/ble-address", "AA:BB:CC:DD:EE:01 APPA S1"}});
  check(w.isGatt() && shown(w, "ui_bleGroup") && !shown(w, "ui_bleKey") && w.device() == "blegatt AA:BB:CC:DD:EE:01",
        "APPA S1: Bluetooth, address only: " + w.device());
  check(w.framesPerReading() == 1 && w.valueCount() == 2, "APPA S1: one frame, main and sub display");
  w.load({{"DMM/model", "APPA 506B *"}, {"Port settings/device", "/dev/ttyUSB0"}});
  check(!w.isBluetooth() && w.device() == "/dev/ttyUSB0", "APPA 506B on the cable: serial");
  w.load({{"DMM/model", "APPA 506B (Bluetooth) *"}, {"Port settings/ble-address", "AA:BB:CC:DD:EE:02"}});
  check(w.isGatt(), "APPA 506B (Bluetooth): GATT");
  {
    // the 0xFFF0 service is shared (Zotek, OWON): an APPA by its name only
    const auto appa = BleGattDevice::profile(FrameFormat::AppaDmm);
    const QList<QBluetoothUuid> fff0 { QBluetoothUuid(quint16(0xFFF0)) };
    check(appa && appa->matches("APPA S2", fff0) && appa->matches("BENNING MM10-1", {}) && appa->matches("Sefram 7221", {})
          && !appa->matches("OWON B41T+", fff0) && !appa->matches("", fff0),
          "APPA profile: by name, not by the shared service");
    const auto ut60 = BleGattDevice::profile(FrameFormat::UniTiDMM);
    check(ut60 && ut60->matches("", { ut60->service }) && ut60->matches("UT60BT", {}), "UT60BT: by service or name");
    check(!BleGattDevice::profile(FrameFormat::Appa500), "APPA 500 protocol: no Bluetooth");
    // the ZOTEK meters all advertise as "Bluetooth DMM", notify and write on 0xFFF4
    const auto zotek = BleGattDevice::profile(FrameFormat::ZotekBLE);
    check(zotek && zotek->matches("Bluetooth DMM", fff0) && !zotek->matches("APPA S2", fff0)
          && !appa->matches("Bluetooth DMM", fff0) && zotek->notify == QBluetoothUuid(quint16(0xFFF4))
          && zotek->write.value(0) == zotek->notify, "ZOTEK profile: by its name, 0xFFF4");
    // the HoldPeak HP-570C-APP by its service 0xFFB0, whatever its name
    const auto hp = BleGattDevice::profile(FrameFormat::VC820BLE);
    check(hp && hp->matches("", { QBluetoothUuid(quint16(0xFFB0)) }) && !hp->matches("Bluetooth DMM", fff0)
          && hp->notify == QBluetoothUuid(quint16(0xFFB2)), "HP-570C-APP profile: by service 0xFFB0");
    // or by AiCare's manufacturer data (company 0xFFAC) when Qt has no service UUIDs
    check(hp && hp->matches("", {}, { 0xFFAC }) && !hp->matches("", {}, { 0x02E1 }) && !zotek->matches("", {}, { 0xFFAC }),
          "HP-570C-APP profile: by company 0xFFAC");
    check(hp && hp->matches("FS9721-LP3", {}) && !hp->matches("Bluetooth DMM", {}), "HP-570C-APP profile: by its name");
    check(hp && hp->helperScan && !zotek->helperScan && !ut60->helperScan, "the bluetoothctl helper for the HP-570C-APP only");
    check(!BleGattDevice::profile(FrameFormat::VC820Continuous), "VC820 on a cable: no Bluetooth");
  }
#endif

  // 6b. sigrok: the driver of the stored port string, also a typed one
  // (manual settings have no model driver)
  for (const QString &dev : { QString("SIGROK fluke-dmm:conn=/dev/ttyUSB0"), QString("SIGROK uni-t-ut61e:conn=/dev/ttyUSB1") })
  {
    w.load({{"DMM/model", "Manual"}, {"DMM/data-format", "Sigrok"}, {"Port settings/device", dev},
            {"Port settings/sigrok-conn", dev.section("conn=", 1)}});
    check(w.device() == dev, "sigrok manual: " + w.device() + " != " + dev);
  }
  w.load({{"DMM/model", "Keysight 34465A (sigrok) *"}, {"Port settings/device", "SIGROK scpi-dmm:conn=tcp-raw/10.0.0.5/5025"},
          {"Port settings/sigrok-conn", "tcp-raw/10.0.0.5/5025"}});
  check(w.device() == "SIGROK scpi-dmm:conn=tcp-raw/10.0.0.5/5025", "sigrok model: " + w.device());
  w.load({{"DMM/model", "Keysight 34465A (sigrok) *"}, {"Port settings/device", "SIGROK other-dmm:conn=tcp-raw/10.0.0.5/5025"},
          {"Port settings/sigrok-conn", "tcp-raw/10.0.0.5/5025"}});
  check(w.device() == "SIGROK other-dmm:conn=tcp-raw/10.0.0.5/5025", "sigrok typed driver: " + w.device());

  // 7. a missing key gets its default (a known model brings its own instead)
  w.load({{"DMM/model", "Manual"}});
  check(w.keys().value("DMM/rts").toBool() && !w.keys().value("DMM/dtr").toBool(), "default: RTS on, DTR off");
  w.load({{"DMM/model", "Uni-Trend UT61E"}, {"DMM/rts", true}});
  check(!w.rts() && w.dtr(), "UT61E: RTS/DTR of the model");

  if (failed)
    qWarning() << failed << "check(s) failed";
  else
    qInfo() << "all checks passed";
  return failed ? 1 : 0;
}
