// Copyright (c) 2026 The QtDMM developers
// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <QJsonObject>
#include <QString>

/// The specification data of a meter as its manual has it - resolution and
/// accuracy per range, input impedance, overload protection, notes - for the
/// Data tab of a device's info. Most comes from dmm-tools, which transcribed
/// it from the manuals (tools/update_device_specs.py writes assets/specs);
/// a file whose "source" is not dmm-tools was written by hand from a manual
/// (hp570c-app.json, cmt3503.json, ut363bt.json, ut383bt.json). The resources have them as ":/specs/<id>.json"; only
/// some meters have it.
namespace DeviceSpecs
{
/// The data of @p vendor's @p model (DMMInfo::vendor and name, as addConfig
/// has them: " *" and a "(...)" suffix such as the Bluetooth adapter do not
/// count); an empty object when there is none.
QJsonObject find(const QString &vendor, const QString &model);
/// @p spec as rich text: a table per function, the source at the end.
QString html(const QJsonObject &spec);
}
