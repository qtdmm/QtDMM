// Copyright (c) 2026 The QtDMM developers
// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <QList>
#include <QString>

#include <utility>

#include "device/dmmdecoder.h"

/// What kind of instrument a model is - a handheld multimeter, a bench
/// meter, a thermometer, a battery monitor - for the model list, the device
/// list of the handbook and later the device's symbol. Most protocols speak
/// for one kind; the models that differ are listed one by one (deviceType()).
/// tests/generate_docs.py reads both tables from devicetype.cpp for the
/// column *Type* of docs/user/supported-devices.md.
namespace DeviceType
{
enum Type
{
  HandheldDmm,
  BenchDmm,
  ClampMeter,
  Thermometer,
  Luxmeter,
  Environment,     ///< CO2, temperature, humidity
  SoundLevel,
  Anemometer,
  Tachometer,      ///< revolutions (optical or contact), a measuring wheel's speed and length
  Microcontroller, ///< a board printing its values as text lines (Serial Plotter, Teleplot)
  BatteryMonitor,
  SolarCharger,
  Inverter,
  Virtual,         ///< QtDMM's own: generators and formulas (logic nodes)
  Generic          ///< not one model: any sigrok SCPI meter, bare DTM0660 boards
};

/// The kind of @p info: its model's entry in the exceptions, else its
/// protocol's.
Type of(const DmmDecoder::DMMInfo &info);
/// The kind a protocol's meters are unless listed otherwise.
Type ofProtocol(FrameFormat::DataFormat protocol);
/// The models listed one by one, as (vendor, model) - for the test that
/// each one still exists.
QList<std::pair<QString, QString>> exceptions();
/// "handheld_dmm" - stable, for files, symbols and the website.
QString id(Type type);
/// "Handheld multimeter", translated.
QString displayName(Type type);
}
