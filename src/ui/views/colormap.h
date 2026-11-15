// Copyright (c) 2026 The QtDMM developers
// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <QColor>
#include <QList>
#include <QString>

/// Maps a normalized value 0..1 to a color. Add new schemes in colormap.cpp.
class ColorMap
{
public:
  enum Scheme { Rainbow, Ironbow, Viridis, Grayscale, Blue, BlueYellow, SchemeCount };

  static QColor color(Scheme scheme, double t);   // t is clamped to 0..1
  /// The neutral fill for cells without samples; distinct from the value scale.
  static QColor noDataColor(Scheme scheme);
  static QString name(Scheme scheme);             // translated display name
  /// The scheme's name in settings and workspaces ("ironbow"), not translated.
  static QString key(Scheme scheme);
  /// The scheme of a key(); Rainbow for an unknown one.
  static Scheme fromKey(const QString &key);
  static QList<Scheme> all();
};

