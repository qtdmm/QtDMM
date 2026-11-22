// Copyright (c) 2026 The QtDMM developers
// SPDX-License-Identifier: GPL-3.0-or-later

#include "device/metersetup.h"

#include "core/sampletypes.h"
#include "device/protocols.h"
#include "device/victronble.h"
#ifdef QTDMM_WITH_BLE
#include "device/transports/blegatt.h"
#endif

FrameFormat::DataFormat MeterSetup::formatFromSetting(const QVariant &value)
{
  bool isNumber = false;
  const int number = value.toInt(&isNumber);
  if (isNumber)
    return (number >= 0 && number < FrameFormat::EndOfList) ? static_cast<FrameFormat::DataFormat>(number) : FrameFormat::Metex14;
  const FrameFormat::DataFormat df = FrameFormat::fromString(value.toString());
  return df == FrameFormat::Invalid ? FrameFormat::Metex14 : df;
}

// A meter QtDMM connects to over GATT (UT60BT): address only, no key and no
// choice of values - unlike the Victron broadcasts
bool MeterSetup::isGatt(const DmmDecoder::DMMInfo &info)
{
#ifdef QTDMM_WITH_BLE
  // baud 0: the Bluetooth entry of a protocol that also has a cable (UT61E+
  // with the UT-D07B adapter vs. the UT-D09 USB cable)
  return info.baud == 0 && BleGattDevice::profile(info.protocol).has_value();
#else
  Q_UNUSED(info)
  return false;
#endif
}

bool MeterSetup::isBluetooth(const DmmDecoder::DMMInfo &info)
{
#ifdef QTDMM_WITH_BLE
  return info.protocol == FrameFormat::VictronBLE || isGatt(info);
#else
  // no Bluetooth port in this build: still the Bluetooth group for these
  // models, with a hint instead of the serial settings they don't have
  const ProtocolInfo *protocol = protocolInfo(info.protocol);
  return info.baud == 0 && protocol && QLatin1String(protocol->transport) == QLatin1String("Bluetooth LE");
#endif
}

namespace
{
/// The Victron fields as the settings page chooses them: the saved ones if
/// the model has them, else the first as the main value, the model's second
/// one, no third and fourth. "-" is none.
QStringList victronFields(const QString &model, const QVariantMap &keys)
{
  QStringList ids;
  for (const VictronBle::Field &f : VictronBle::fields(VictronBle::readoutTypeForModel(model)))
    ids << QString::fromLatin1(f.id);
  auto saved = [&keys](const char *key) { return keys.value(QString("Port settings/") + key).toString(); };
  const QString main = saved("ble-main");
  const QString second = saved("ble-second");
  const QString third = saved("ble-third");
  const QString fourth = saved("ble-fourth");
  auto orNone = [&ids](const QString &id) { return id == "-" || ids.contains(id) ? id : QString("-"); };
  QStringList out;
  out << (ids.contains(main) ? main : ids.value(0));
  out << (second == "-" || ids.contains(second) ? second : ids.isEmpty() ? QString("-") : ids.value(qMin(1, int(ids.size()) - 1)));
  out << orNone(third) << orNone(fourth);
  return out;
}
}

MeterSetup MeterSetup::fromKeys(const QVariantMap &keys, const QString &sigrokExe)
{
  auto cfgString = [&keys](const QString &key, const QString &def = QString()) { return keys.value(key, def).toString(); };
  auto cfgInt    = [&keys](const QString &key, int def = 0) { return keys.value(key, def).toInt(); };
  auto cfgBool   = [&keys](const QString &key, bool def = false) { return keys.value(key, def).toBool(); };

  MeterSetup s;
  const QString model = cfgString("DMM/model");
  for (const auto &cfg : DmmDecoder::getDeviceConfigurations())
    if (DmmDecoder::sameModel(model, cfg.name))
    {
      s.info = cfg;
      s.knownModel = true;
      if (cfg.baudChoosable() && cfgInt("Port settings/baud") > 0)
        s.info.baud = cfgInt("Port settings/baud");
      if (cfg.unitsChoosable())
        for (int i = 1; i <= kMaxValues; ++i)
          s.info.units << cfgString(QString("DMM/unit-%1").arg(i)).trimmed();
      break;
    }
  if (!s.knownModel)
  {
    // the manual settings: what the keys say, with the page's defaults
    s.info.name = "custom";
    s.info.baud = cfgInt("Port settings/baud");
    s.info.protocol = formatFromSetting(cfgString("DMM/data-format", "Metex14"));
    s.info.bits = cfgInt("Port settings/bits", 7);
    s.info.stopBits = cfgInt("Port settings/stop-bits", 1);
    s.info.parity = cfgInt("Port settings/parity");   // stored as index
    s.info.display = cfgInt("DMM/display", 4000);
    s.info.framesPerReading = cfgInt("DMM/number-of-values", 1);
    s.info.externalSetup = cfgBool("DMM/external-setup", false);
    s.info.rts = cfgBool("DMM/rts", true);
    s.info.dtr = cfgBool("DMM/dtr", false);
  }
  s.info.sigrokExe = sigrokExe;
  s.format = s.info.protocol;
  s.valueCount = s.info.valueCount();

  const QString stored = cfgString("Port settings/device");
  const QString address = cfgString("Port settings/ble-address").section(' ', 0, 0).trimmed();
  const DmmDecoder::DMMInfo &info = s.info;
  // manual settings are a port, whatever their protocol
  const bool known = s.knownModel;
  if (known && isSigrok(info))
  {
    // "SIGROK <driver>:conn=<connection>[:<options>]" - what the Special
    // ports page would take as a custom entry; the driver the model's unless
    // another one was typed for it (then it is in the saved port string)
    QString driver;
    if (stored.section(' ', 0, 0).compare("sigrok", Qt::CaseInsensitive) == 0)
      driver = stored.section(' ', 1).section(':', 0, 0).trimmed();
    QString spec = driver.isEmpty() ? info.sigrokDriver : driver;
    const QString conn = cfgString("Port settings/sigrok-conn").trimmed();
    if (!conn.isEmpty())
      spec += ":conn=" + conn;
    const QString options = cfgString("Port settings/sigrok-options").simplified().remove(' ');
    if (!options.isEmpty())
      spec += ":" + options;
    s.device = "SIGROK " + spec;
  }
  else if (known && isGatt(info))
    s.device = "blegatt " + address;
  else if (known && isBluetooth(info))
  {
    const QStringList fields = victronFields(info.model, keys);
    s.device = QString("ble %1 %2 %3 %4").arg(address, cfgString("Port settings/ble-key").simplified().remove(' '),
                                              fields[0], fields[1]);
    // the third and fourth field only when chosen: the string of before
    const int extra = fields[3] != "-" ? 2 : fields[2] != "-" ? 1 : 0;
    if (extra > 0)
      s.device += QString(" %1 %2").arg(fields[2], fields[3]);
    // main and second value, and the third and fourth chosen
    s.valueCount = info.valueCount() + extra;
  }
  else if (!stored.section(' ', 0, 0).startsWith("ble", Qt::CaseInsensitive))
    s.device = stored;   // a Bluetooth meter's string is no port for a cable meter
  return s;
}
