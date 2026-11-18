// Copyright (c) 2026 The QtDMM developers
// SPDX-License-Identifier: GPL-3.0-or-later
//
// MeterSetup: how to open a meter from its keys alone. For every model of the
// table it agrees with the settings page (DeviceSettings) on the protocol,
// the port parameters and the port string; the kinds of meter without a
// serial port and the manual settings are checked by their strings.

#include <QtWidgets>

#include "device/dmmdecoder.h"
#include "device/metersetup.h"
#include "ui/devicesettings.h"
#include "testutil.h"

int main(int argc, char **argv)
{
  if (qEnvironmentVariableIsEmpty("QT_QPA_PLATFORM"))
    qputenv("QT_QPA_PLATFORM", "offscreen");
  QApplication app(argc, argv);

  DeviceSettings w;
  w.setPorts({"/dev/ttyUSB0", "/dev/ttyUSB1"});

  // 1. every model: saved keys as an older version or "My devices" has them,
  // through the page and without it. A page of its own each: one that showed
  // another Victron model before keeps its fields when the keys have none
  int models = 0;
  for (const DmmDecoder::DMMInfo &cfg : DmmDecoder::getDeviceConfigurations())
  {
    DeviceSettings fresh;
    fresh.setPorts({"/dev/ttyUSB0", "/dev/ttyUSB1"});
    const QVariantMap saved {
      {"DMM/model", cfg.name}, {"Port settings/device", "/dev/ttyUSB1"}, {"Port settings/baud", "1200"},
      {"Port settings/ble-address", "AA:BB:CC:DD:EE:FF"}, {"Port settings/ble-key", "00112233445566778899aabbccddeeff"},
      {"Port settings/sigrok-conn", "/dev/ttyUSB0"}, {"DMM/calc-expression", "u * i"}, {"DMM/number-of-values", 3}};
    fresh.load(saved);
    const MeterSetup direct = MeterSetup::fromKeys(saved);
    const MeterSetup page = MeterSetup::fromKeys(fresh.keys());
    const QString what = cfg.name + ": ";
    check(direct.knownModel && direct.info.name == cfg.name, what + "model found");
    check(direct.format == cfg.protocol && fresh.format() == cfg.protocol, what + "protocol");
    // a microcontroller keeps the rate saved, the others take the model's
    check(direct.info.baud == (cfg.baudChoosable() ? 1200 : cfg.baud)
            && direct.info.display == fresh.display()
            && direct.info.framesPerReading == fresh.framesPerReading() && direct.info.externalSetup == fresh.externalSetup()
            && direct.info.rts == fresh.rts() && direct.info.dtr == fresh.dtr(),
          what + "port parameters as the page has them");
    check(direct.device == page.device, what + direct.device + " != " + page.device);
    check(direct.valueCount == page.valueCount && direct.valueCount == fresh.valueCount(),
          what + QString("values %1, page %2").arg(direct.valueCount).arg(fresh.valueCount()));
    ++models;
  }
  check(models > 100, QString("all models: %1").arg(models));

  // 2. manual settings: the keys, with the page's defaults
  {
    const MeterSetup s = MeterSetup::fromKeys({{"DMM/model", "Manual"}, {"Port settings/device", "/dev/ttyS0"},
                                               {"Port settings/baud", "2400"}, {"DMM/data-format", "Metex14"},
                                               {"DMM/number-of-values", 4}}, "/opt/sigrok-cli");
    check(!s.knownModel && s.info.name == "custom" && s.info.baud == 2400 && s.format == FrameFormat::Metex14,
          "manual: model, baud, protocol");
    check(s.info.bits == 7 && s.info.stopBits == 1 && s.info.rts && !s.info.dtr && s.info.display == 4000,
          "manual: defaults");
    check(s.device == "/dev/ttyS0" && s.valueCount == 4, "manual: port and values " + s.device);
    check(s.info.sigrokExe == "/opt/sigrok-cli", "sigrok-cli path");
    check(MeterSetup::fromKeys({{"DMM/model", "Manual"}, {"DMM/data-format", 3}}).format == FrameFormat::DataFormat(3),
          "manual: protocol as a number (old files)");
  }

  // 3. a cable meter after a Bluetooth one: no port of the Bluetooth meter
  check(MeterSetup::fromKeys({{"DMM/model", "Uni-Trend UT61E"}, {"Port settings/device", "ble AA:BB:CC:DD:EE:FF"}}).device.isEmpty(),
        "cable meter: no Bluetooth string as its port");

  // 4. the kinds of meter without a port
  check(MeterSetup::fromKeys({{"DMM/model", "QtDMM Calculated value"}, {"DMM/calc-unit", "W"}, {"DMM/calc-expression", "u * i"}}).device
          == "calc W u * i",
        "calculated value");
  {
    const MeterSetup s = MeterSetup::fromKeys({{"DMM/model", "QtDMM Simulated meter"}, {"DMM/virtual-waveform", 7},
                                               {"DMM/virtual-formula", "1 + t"}, {"DMM/virtual-values", 2}});
    check(s.device == QString("calc V/DC 1 + t; A/DC %1").arg(MeterSetup::kVirtualDefaults[0][1]) && s.valueCount == 2,
          "simulated meter: " + s.device);
  }
  {
    const MeterSetup s = MeterSetup::fromKeys({{"DMM/model", "Victron SmartShunt"}, {"Port settings/ble-address", "AA:BB:CC:DD:EE:FF"},
                                               {"Port settings/ble-key", "0011 2233"}});
    check(s.device.startsWith("ble AA:BB:CC:DD:EE:FF 00112233 ") && s.device.count(' ') == 4 && s.valueCount == 2,
          "Victron: address, key, main and second value: " + s.device);
  }

  // 5. sigrok: a typed driver goes into the saved port string and comes back
  {
    const QVariantMap typed {{"DMM/model", "Keysight 34465A (sigrok) *"}, {"Port settings/device", "SIGROK other-dmm:conn=x"},
                             {"Port settings/sigrok-conn", "tcp-raw/10.0.0.5/5025"}, {"Port settings/sigrok-options", "a = 1"}};
    check(MeterSetup::fromKeys(typed).device == "SIGROK other-dmm:conn=tcp-raw/10.0.0.5/5025:a=1", "sigrok: typed driver");
    w.load(typed);
    const QVariantMap again = w.keys();
    check(again.value("Port settings/device").toString().startsWith("SIGROK other-dmm:"),
          "sigrok: the page saves the driver: " + again.value("Port settings/device").toString());
    w.load(again);
    check(w.device() == "SIGROK other-dmm:conn=tcp-raw/10.0.0.5/5025:a=1", "sigrok: driver after reload " + w.device());
    check(MeterSetup::fromKeys({{"DMM/model", "Keysight 34465A (sigrok) *"}}).device == "SIGROK scpi-dmm", "sigrok: the model's driver");
  }

  return failed == 0 ? 0 : 1;
}
