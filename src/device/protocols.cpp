// Copyright (c) 2026 The QtDMM developers
// SPDX-License-Identifier: GPL-3.0-or-later
#include "device/protocols.h"

#include <QCoreApplication>

#include "decoders.h"

namespace
{
template <class T>
std::shared_ptr<DmmDecoder> make(FrameFormat::DataFormat df)
{
  return std::make_shared<T>(df);
}
}

// tests/generate_docs.py parses the rows below: keep one row per line,
// { FrameFormat::<Id>, "<Name>", "<description>", "<chip>", "<transport>", make<...> }.
const std::vector<ProtocolInfo> &protocols()
{
  static const std::vector<ProtocolInfo> table = {
    { FrameFormat::Metex14,               "Metex14",               QT_TRANSLATE_NOOP("Protocols", "14 bytes ASCII, polling (Metex/Voltcraft)"),                    "Metex KS57C2016",       "",                         make<DecoderAscii> },
    { FrameFormat::PeakTech10,            "PeakTech10",            QT_TRANSLATE_NOOP("Protocols", "11 bytes ASCII, continuous (PeakTech 451)"),                   "",                      "",                         make<DecoderAscii> },
    { FrameFormat::Voltcraft14Continuous, "Voltcraft14Continuous", QT_TRANSLATE_NOOP("Protocols", "14 bytes ASCII, continuous (Voltcraft)"),                      "",                      "",                         make<DecoderAscii> },
    { FrameFormat::Voltcraft15Continuous, "Voltcraft15Continuous", QT_TRANSLATE_NOOP("Protocols", "15 bytes ASCII, continuous (Voltcraft)"),                      "",                      "",                         make<DecoderAscii> },
    { FrameFormat::M9803RContinuous,      "M9803RContinuous",      QT_TRANSLATE_NOOP("Protocols", "11 bytes binary, continuous (M9803R)"),                        "",                      "",                         make<DecoderM9803R> },
    { FrameFormat::VC820Continuous,       "VC820Continuous",       QT_TRANSLATE_NOOP("Protocols", "14 bytes binary, continuous (VC820)"),                         "FS9721 LP3",            "",                         make<DecoderVC820> },
    { FrameFormat::CyrustekES51986,       "CyrustekES51986",       QT_TRANSLATE_NOOP("Protocols", "11 bytes binary, continuous (CyrustekES51986)"),               "ES51986",               "",                         make<DecoderCyrusTekES51986> },
    { FrameFormat::VC940Continuous,       "VC940Continuous",       QT_TRANSLATE_NOOP("Protocols", "11 bytes binary, continuous (VC940)"),                         "",                      "",                         make<DecoderVC940> },
    { FrameFormat::QM1537Continuous,      "QM1537Continuous",      QT_TRANSLATE_NOOP("Protocols", "14 bytes ASCII/binary, continuous (QM1537)"),                  "FS9922-DMM4",           "",                         make<DecoderQM1537> },
    { FrameFormat::RS22812Continuous,     "RS22812Continuous",     QT_TRANSLATE_NOOP("Protocols", "9 bytes binary, continuous (RS 22-812)"),                      "",                      "",                         make<DecoderRS22812> },
    { FrameFormat::VC870Continuous,       "VC870Continuous",       QT_TRANSLATE_NOOP("Protocols", "23 bytes ASCII, continuous (VC870)"),                          "",                      "",                         make<DecoderVC870> },
    { FrameFormat::DO3122Continuous,      "DO3122Continuous",      QT_TRANSLATE_NOOP("Protocols", "22 bytes binary, continuous (DO3122)"),                        "",                      "",                         make<DecoderDO3122> },
    { FrameFormat::CyrustekES51922,       "CyrustekES51922",       QT_TRANSLATE_NOOP("Protocols", "14 bytes half-ASCII, UNI-T UT61E (CyrustekES51922)"),          "ES51922",               "",                         make<DecoderCyrusTekES51922> },
    { FrameFormat::DTM0660,               "DTM0660",               QT_TRANSLATE_NOOP("Protocols", "DTM0660"),                                                     "DTM0660",               "",                         make<DecoderDTM0660> },
    { FrameFormat::CyrustekES51962,       "CyrustekES51962",       QT_TRANSLATE_NOOP("Protocols", "11 bytes binary, continuous (CyrustekES51962)"),               "ES51962",               "",                         make<DecoderCyrusTekES51962> },
    { FrameFormat::Sigrok,                "Sigrok",                QT_TRANSLATE_NOOP("Protocols", "variable bytes ASCII, Sigrok"),                                "",                      "sigrok-cli",               make<DecoderAscii> },
    { FrameFormat::GDM703Continuous,      "GDM703Continuous",      QT_TRANSLATE_NOOP("Protocols", "26 bytes ASCII, continuous, two values (Voltcraft GDM 703)"),  "WENS98A",               "",                         make<DecoderGDM703> },
    { FrameFormat::BrymenBM25x,           "BrymenBM25x",           QT_TRANSLATE_NOOP("Protocols", "15 bytes binary, continuous (Brymen BM25x)"),                  "Brymen BM25x",          "",                         make<DecoderBrymenBM25x> },
    { FrameFormat::BrymenBM86x,           "BrymenBM86x",           QT_TRANSLATE_NOOP("Protocols", "24 bytes binary, polled over BU-86X (Brymen BM86x)"),          "Brymen BM86x (BU-86X)", "USB-HID (BU-86X)",         make<DecoderBrymenBM86x> },
    { FrameFormat::BrymenBM52x,           "BrymenBM52x",           QT_TRANSLATE_NOOP("Protocols", "24 bytes binary, polled over BU-86X (Brymen BM52x)"),          "Brymen BM52x (BU-86X)", "USB-HID (BU-86X)",         make<DecoderBrymenBM52x> },
    { FrameFormat::BrymenBM82x,           "BrymenBM82x",           QT_TRANSLATE_NOOP("Protocols", "24 bytes binary, polled over BU-86X (Brymen BM82x)"),          "Brymen BM82x (BU-86X)", "USB-HID (BU-86X)",         make<DecoderBrymenBM52x> },
    { FrameFormat::FlukeQM,               "FlukeQM",               QT_TRANSLATE_NOOP("Protocols", "variable bytes ASCII, polled with QM (Fluke 18x/28x)"),        "",                      "",                         make<DecoderFlukeQM> },
    { FrameFormat::Fluke45,               "Fluke45",               QT_TRANSLATE_NOOP("Protocols", "variable bytes ASCII, polled (Fluke 45 bench meter)"),           "",                      "",                         make<DecoderFluke45> },
    { FrameFormat::VictronBLE,            "VictronBLE",            QT_TRANSLATE_NOOP("Protocols", "Bluetooth LE advertisements, encrypted (Victron Instant Readout)"),  "",                 "Bluetooth LE",             make<DecoderVictronBLE> },
    { FrameFormat::UniTiDMM,              "UniTiDMM",              QT_TRANSLATE_NOOP("Protocols", "19 bytes binary, polled over Bluetooth LE (UNI-T UT60BT)"),   "",                      "Bluetooth LE",             make<DecoderUniTiDMM> },
    { FrameFormat::UniTUT61Plus,          "UniTUT61Plus",          QT_TRANSLATE_NOOP("Protocols", "19 bytes binary, polled (UNI-T UT61B+/D+/E+, USB or Bluetooth adapter)"), "", "Bluetooth LE", make<DecoderUniTiDMM> },
    { FrameFormat::UniTUT202BT,           "UniTUT202BT",           QT_TRANSLATE_NOOP("Protocols", "19 bytes binary, polled over Bluetooth LE (UNI-T UT202BT clamp)"), "", "Bluetooth LE", make<DecoderUniTiDMM> },
    { FrameFormat::UniTiENV,              "UniTiENV",              QT_TRANSLATE_NOOP("Protocols", "19 bytes, polled over Bluetooth LE, lx or dB (UNI-T UT383BT, UT353BT)"), "", "Bluetooth LE", make<DecoderUniTiENV> },
    { FrameFormat::TfaAirControl,         "TfaAirControl",         QT_TRANSLATE_NOOP("Protocols", "text lines from HID records, CO2/temperature/humidity (TFA AIRCO2NTROL)"), "Holtek",  "USB-HID (Holtek)",         make<DecoderTfaAirControl> },
    { FrameFormat::KeysightU12xx,         "KeysightU12xx",         QT_TRANSLATE_NOOP("Protocols", "variable bytes ASCII, polled with CONF?/FETC? (Agilent/Keysight U12xx)"), "",                      "",                         make<DecoderKeysightU12xx> },
    { FrameFormat::UniTUT32x,             "UniTUT32x",             QT_TRANSLATE_NOOP("Protocols", "19 bytes ASCII, continuous after a start byte (UNI-T UT32x thermometers)"), "",            "",                         make<DecoderUniTUT32x> },
    { FrameFormat::MastechMS6514,         "MastechMS6514",         QT_TRANSLATE_NOOP("Protocols", "18 bytes binary, continuous, two values (MASTECH MS6514 thermometer)"), "",                "",                         make<DecoderMastechMS6514> },
    { FrameFormat::AppaDmm,               "AppaDmm",               QT_TRANSLATE_NOOP("Protocols", "17 bytes binary, polled (APPA 506/208, BENNING MM 12; APPA S, 150B, 170B over Bluetooth LE)"), "", "Bluetooth LE", make<DecoderAppa> },
    { FrameFormat::Appa500,               "Appa500",               QT_TRANSLATE_NOOP("Protocols", "59 bytes binary, polled (APPA 503/505, Voltcraft VC-930/950)"), "",                     "",                         make<DecoderAppa> },
    { FrameFormat::UniTUT382,             "UniTUT382",             QT_TRANSLATE_NOOP("Protocols", "33 bytes, continuous, lx or fc (UNI-T UT382 luxmeter)"), "",                          "",                         make<DecoderUniTUT382> },
    { FrameFormat::BenetechGM1020,        "BenetechGM1020",        QT_TRANSLATE_NOOP("Protocols", "8 bytes binary, continuous after a start command, lx and °C (Benetech GM1020)"), "",  "",                         make<DecoderBenetechGM1020> },
    { FrameFormat::ZotekBLE,              "ZotekBLE",              QT_TRANSLATE_NOOP("Protocols", "10 to 19 bytes, scrambled LCD segments, streamed over Bluetooth LE (ZOTEK, ANENG)"), "", "Bluetooth LE", make<DecoderZotek> },
    { FrameFormat::VC820BLE,              "VC820BLE",              QT_TRANSLATE_NOOP("Protocols", "14 bytes binary, streamed over Bluetooth LE (FS9721, HoldPeak HP-570C-APP)"), "FS9721 LP3", "Bluetooth LE", make<DecoderVC820> },
    { FrameFormat::OwonBLE,               "OwonBLE",               QT_TRANSLATE_NOOP("Protocols", "6 bytes binary, streamed over Bluetooth LE (OWON B35T+, B41T+, OW18E, CM2100B)"), "", "Bluetooth LE", make<DecoderOwon> },
    { FrameFormat::OwonBLE15,             "OwonBLE15",             QT_TRANSLATE_NOOP("Protocols", "15 bytes binary, streamed over Bluetooth LE (OWON CMS101, OW65B-OW69B, Voltcraft VC871-VC925 PV)"), "", "Bluetooth LE", make<DecoderOwon15> },
    { FrameFormat::UniTUT181A,            "UniTUT181A",            QT_TRANSLATE_NOOP("Protocols", "AB CD frames of any length with float values, streamed once asked (UNI-T UT181A, UT-D09 cable or UT-D07B adapter)"), "", "Bluetooth LE", make<DecoderUT181A> },
  };
  return table;
}

const ProtocolInfo *protocolInfo(FrameFormat::DataFormat id)
{
  for (const ProtocolInfo &p : protocols())
    if (p.id == id)
      return &p;
  return nullptr;
}

QString FrameFormat::toString(DataFormat format)
{
  const ProtocolInfo *p = protocolInfo(format);
  return p ? QString::fromLatin1(p->name) : QStringLiteral("Invalid");
}

FrameFormat::DataFormat FrameFormat::fromString(const QString &str)
{
  for (const ProtocolInfo &p : protocols())
    if (str == QLatin1String(p.name))
      return p.id;
  return Invalid;
}

std::shared_ptr<DmmDecoder> DmmDecoder::getInstance(FrameFormat::DataFormat df)
{
  const ProtocolInfo *p = protocolInfo(df);
  if (!p)
  {
    if (df != FrameFormat::Invalid)
      qWarning() << "invalid decoder ID" << df;
    return nullptr;
  }
  return p->create(df);
}

std::shared_ptr<DmmDecoder> DmmDecoder::getInstance(QString df)
{
  return getInstance(FrameFormat::fromString(df));
}
