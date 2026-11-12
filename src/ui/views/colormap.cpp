// Copyright (c) 2026 The QtDMM developers
// SPDX-License-Identifier: GPL-3.0-or-later
#include "ui/views/colormap.h"

#include <QObject>
#include <algorithm>
#include <array>

namespace
{
struct Stop { double pos; int r, g, b; };

template <size_t N>
QColor interpolate(const std::array<Stop, N> &stops, double t)
{
  t = std::clamp(t, 0.0, 1.0);
  for (size_t i = 1; i < N; ++i)
  {
    if (t <= stops[i].pos)
    {
      const Stop &a = stops[i - 1], &b = stops[i];
      const double f = (t - a.pos) / (b.pos - a.pos);
      return QColor(qRound(a.r + (b.r - a.r) * f),
                    qRound(a.g + (b.g - a.g) * f),
                    qRound(a.b + (b.b - a.b) * f));
    }
  }
  return QColor(stops[N - 1].r, stops[N - 1].g, stops[N - 1].b);
}

// "Full color": blue -> cyan -> green -> yellow -> red (classic jet-like)
constexpr std::array<Stop, 5> kRainbow = {{
  {0.00,   0,   0, 255},
  {0.25,   0, 255, 255},
  {0.50,   0, 255,   0},
  {0.75, 255, 255,   0},
  {1.00, 255,   0,   0},
}};

// FLIR-like "Ironbow": black -> dark blue -> purple -> red/orange -> yellow -> white
constexpr std::array<Stop, 7> kIronbow = {{
  {0.00,   0,   0,   0},
  {0.17,  32,   0, 120},
  {0.38, 140,  20, 140},
  {0.55, 210,  50,  60},
  {0.72, 250, 130,  10},
  {0.88, 255, 220,  60},
  {1.00, 255, 255, 230},
}};
}

QColor ColorMap::color(Scheme scheme, double t)
{
  switch (scheme)
  {
  case Ironbow: return interpolate(kIronbow, t);
  case Rainbow:
  default:      return interpolate(kRainbow, t);
  }
}

QString ColorMap::name(Scheme scheme)
{
  switch (scheme)
  {
  case Ironbow: return QObject::tr("Ironbow (thermal)");
  case Rainbow:
  default:      return QObject::tr("Rainbow");
  }
}

QList<ColorMap::Scheme> ColorMap::all()
{
  return { Rainbow, Ironbow };
}
