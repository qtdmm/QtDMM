// Copyright (c) 2026 The QtDMM developers
// SPDX-License-Identifier: GPL-3.0-or-later
#include "device/devicetype.h"

#include <QCoreApplication>
#include <QHash>

namespace DeviceType
{

// No default: a new protocol without a kind is a compiler warning (an error
// in the -Werror builds).
Type ofProtocol(FrameFormat::DataFormat protocol)
{
  switch (protocol)
  {
    case FrameFormat::Metex14:
    case FrameFormat::PeakTech10:
    case FrameFormat::Voltcraft14Continuous:
    case FrameFormat::M9803RContinuous:
    case FrameFormat::VC820Continuous:
    case FrameFormat::CyrustekES51986:
    case FrameFormat::VC940Continuous:
    case FrameFormat::QM1537Continuous:
    case FrameFormat::RS22812Continuous:
    case FrameFormat::VC870Continuous:
    case FrameFormat::DO3122Continuous:
    case FrameFormat::CyrustekES51922:
    case FrameFormat::DTM0660:
    case FrameFormat::CyrustekES51962:
    case FrameFormat::BrymenBM25x:
    case FrameFormat::BrymenBM86x:
    case FrameFormat::BrymenBM52x:
    case FrameFormat::BrymenBM82x:
    case FrameFormat::FlukeQM:
    case FrameFormat::UniTiDMM:
    case FrameFormat::UniTUT61Plus:
    case FrameFormat::KeysightU12xx:
    case FrameFormat::AppaDmm:
    case FrameFormat::Appa500:
    case FrameFormat::ZotekBLE:
    case FrameFormat::OwonBLE:
    case FrameFormat::OwonBLE15:
    case FrameFormat::UniTUT181A:
    case FrameFormat::UniTUT171:
    case FrameFormat::VoltcraftVC880:
    case FrameFormat::VoltcraftVC890:
    case FrameFormat::Eevblog121GW:
    case FrameFormat::BrymenBM78xBT:
      return HandheldDmm;
    case FrameFormat::Voltcraft15Continuous:
    case FrameFormat::Sigrok:
    case FrameFormat::GDM703Continuous:
    case FrameFormat::Fluke45:
    case FrameFormat::UniTUT8802:
    case FrameFormat::UniTUT8803:
      return BenchDmm;
    case FrameFormat::UniTUT32x:
    case FrameFormat::MastechMS6514:
      return Thermometer;
    case FrameFormat::UniTUT382:
    case FrameFormat::BenetechGM1020:
      return Luxmeter;
    case FrameFormat::UniTUT202BT:
    case FrameFormat::VC820BLE:
      return ClampMeter;
    case FrameFormat::TfaAirControl:
    case FrameFormat::UniTiENV:   // UNI-T's environment meters; the luxmeter and the sound level meter below
      return Environment;
    case FrameFormat::VictronBLE:
      return BatteryMonitor;
    case FrameFormat::Invalid:
    case FrameFormat::EndOfList:
      return Generic;
  }
  return Generic;
}

namespace
{
struct Exception
{
  const char *vendor;
  const char *model;
  Type type;
};

// The models that are not what their protocol's meters are. The model
// without its " *" (not confirmed).
const Exception kExceptions[] = {
  { "Metex", "Universal System 9140", BenchDmm },
  { "Metex", "universal system 9160", BenchDmm },
  { "Voltcraft", "MS-9140", BenchDmm },
  { "Voltcraft", "MS-9160", BenchDmm },
  { "PeakTech", "4015A", BenchDmm },
  { "Voltcraft", "VC 630", BenchDmm },
  { "Voltcraft", "VC 650", BenchDmm },
  { "Voltcraft", "VC 670", BenchDmm },
  { "Tenma", "72-1016", BenchDmm },
  { "Uni-Trend", "UT803", BenchDmm },
  { "Uni-Trend", "UT804", BenchDmm },
  { "APPA", "208", BenchDmm },
  { "APPA", "208B", BenchDmm },
  { "APPA", "208B (Bluetooth)", BenchDmm },
  { "APPA", "172B", ClampMeter },
  { "APPA", "173B", ClampMeter },
  { "APPA", "175B", ClampMeter },
  { "APPA", "177B", ClampMeter },
  { "APPA", "179B", ClampMeter },
  { "APPA", "A17N", ClampMeter },
  { "APPA", "sFlex-10A", ClampMeter },
  { "APPA", "sFlex-18A", ClampMeter },
  { "BENNING", "CM 9-2", ClampMeter },
  { "BENNING", "CM 10-1", ClampMeter },
  { "BENNING", "CM 10-PV", ClampMeter },
  { "BENNING", "CM 12", ClampMeter },
  { "KPS", "DCM7000BT", ClampMeter },
  { "KPS", "DCM8000BT", ClampMeter },
  { "Megger", "DCM1500S", ClampMeter },
  { "Megger", "DPM1000", ClampMeter },
  { "Sefram", "MW3516BF", ClampMeter },
  { "Sefram", "MW3526BF", ClampMeter },
  { "Sefram", "MW3536BF", ClampMeter },
  { "ZOTEK", "ZT-5BQ", ClampMeter },
  { "ANENG", "ST207", ClampMeter },
  { "ZOTEK", "ZT-5566SE", BenchDmm },
  { "ANENG", "AN999S", BenchDmm },
  { "OWON", "CM2100B", ClampMeter },
  { "Generic", "DTM0660 4000 count", Generic },
  { "Generic", "DTM0660 6000 count", Generic },
  { "Generic", "DTM0660 8000 count", Generic },
  { "sigrok", "SCPI DMM (any scpi-dmm model)", Generic },
  { "Uni-Trend", "UT383BT", Luxmeter },
  { "Uni-Trend", "UT353BT", SoundLevel },
  { "Victron", "SmartSolar MPPT", SolarCharger },
  { "Victron", "BlueSolar MPPT", SolarCharger },
  { "Victron", "Phoenix Inverter Smart", Inverter },
  { "QtDMM", "Simulated meter", Virtual },
  { "QtDMM", "Calculated value", Virtual },
};

QString key(const QString &vendor, QString model)
{
  if (model.endsWith(" *"))
    model.chop(2);
  return vendor + '\n' + model.trimmed();
}
}

Type of(const DmmDecoder::DMMInfo &info)
{
  static const QHash<QString, Type> exceptions = []
  {
    QHash<QString, Type> h;
    for (const Exception &e : kExceptions)
      h.insert(key(QString::fromUtf8(e.vendor), QString::fromUtf8(e.model)), e.type);
    return h;
  }();
  return exceptions.value(key(info.vendor, info.model), ofProtocol(info.protocol));
}

QList<std::pair<QString, QString>> exceptions()
{
  QList<std::pair<QString, QString>> list;
  for (const Exception &e : kExceptions)
    list.append({ QString::fromUtf8(e.vendor), QString::fromUtf8(e.model) });
  return list;
}

QString id(Type type)
{
  static const char *const ids[] = { "handheld_dmm", "bench_dmm", "clamp_meter", "thermometer", "luxmeter",
                                     "environment", "sound_level", "anemometer", "battery_monitor",
                                     "solar_charger", "inverter", "virtual", "generic" };
  return QString::fromLatin1(ids[type]);
}

QString displayName(Type type)
{
  switch (type)
  {
    case HandheldDmm:    return QCoreApplication::translate("DeviceType", "Handheld multimeter");
    case BenchDmm:       return QCoreApplication::translate("DeviceType", "Bench multimeter");
    case ClampMeter:     return QCoreApplication::translate("DeviceType", "Clamp meter");
    case Thermometer:    return QCoreApplication::translate("DeviceType", "Thermometer");
    case Luxmeter:       return QCoreApplication::translate("DeviceType", "Luxmeter");
    case Environment:    return QCoreApplication::translate("DeviceType", "Air quality meter");
    case SoundLevel:     return QCoreApplication::translate("DeviceType", "Sound level meter");
    case Anemometer:     return QCoreApplication::translate("DeviceType", "Anemometer");
    case BatteryMonitor: return QCoreApplication::translate("DeviceType", "Battery monitor");
    case SolarCharger:   return QCoreApplication::translate("DeviceType", "Solar charge controller");
    case Inverter:       return QCoreApplication::translate("DeviceType", "Inverter");
    case Virtual:        return QCoreApplication::translate("DeviceType", "Virtual meter");
    case Generic:        return QCoreApplication::translate("DeviceType", "Generic");
  }
  return {};
}

}
