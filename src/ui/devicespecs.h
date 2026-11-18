// Copyright (c) 2026 The QtDMM developers
// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <QColor>
#include <QJsonObject>
#include <QString>

struct Reading;

/// The specification data of a meter as its manual has it - resolution and
/// accuracy per range, input impedance, overload protection, notes - for the
/// Data tab of a device's info. Most comes from dmm-tools, which transcribed
/// it from the manuals (tools/update_device_specs.py writes assets/specs);
/// a file whose "source" is not dmm-tools was written by hand from a manual
/// (hp570c-app.json, cmt3503.json, ut333bt.json, ut353bt.json, ut363bt.json, ut383bt.json). The resources have them as ":/specs/<id>.json"; only
/// some meters have it.
namespace DeviceSpecs
{
/// The data of @p vendor's @p model (DMMInfo::vendor and name, as addConfig
/// has them: " *" and a "(...)" suffix such as the Bluetooth adapter do not
/// count); an empty object when there is none.
QJsonObject find(const QString &vendor, const QString &model);
/// Where a reading is in the data: the index of its table in "tables" and
/// of its range in that table's "ranges", -1 when not found.
struct Match
{
  int table = -1;
  int row = -1;
  bool isValid() const { return table >= 0; }
  bool operator==(const Match &o) const { return table == o.table && row == o.row; }
};
/// The table and range of @p spec that @p reading is measured in: the table
/// by its heading's words (quantity, AC/DC), the range by the reading's full
/// scale; several tables of the quantity (other input impedance or fuse)
/// are told apart by the range. Without a full scale only the table, the
/// first that fits; nothing when no heading fits.
Match match(const QJsonObject &spec, const Reading &reading);
/// Whether match() knows what a table @p heading measures (for the tests:
/// a new data file with a heading of other words is noticed).
bool knowsHeading(const QString &heading);
/// @p spec as rich text: a table per function, the source at the end. A
/// note naming a range of its table ("600Ω: ...") stands below that range.
/// @p current is marked in @p mark and has the anchor "current".
QString html(const QJsonObject &spec, const Match &current = {}, const QColor &mark = QColor());
}
