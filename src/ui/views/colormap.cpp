// Copyright (c) 2026 The QtDMM developers
// SPDX-License-Identifier: GPL-3.0-or-later
#include "ui/views/colormap.h"

#include <QCoreApplication>
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

// Perceptually uniform blue -> green -> yellow scale.
constexpr std::array<Stop, 6> kViridis = {{
  {0.00,  68,   1,  84},
  {0.20,  64,  67, 135},
  {0.40,  42, 120, 142},
  {0.60,  34, 167, 132},
  {0.80, 122, 209,  81},
  {1.00, 253, 231,  37},
}};

// A neutral brightness scale.
constexpr std::array<Stop, 2> kGrayscale = {{
  {0.00,  18,  18,  18},
  {1.00, 248, 248, 248},
}};

// Shades of blue, from navy through blue to pale blue.
constexpr std::array<Stop, 3> kBlue = {{
  {0.00,   5,  22,  64},
  {0.50,  29,  78, 216},
  {1.00, 219, 234, 254},
}};

// Blue/teal values grow into yellow; missing cells use a separate cobalt blue.
constexpr std::array<Stop, 5> kBlueYellow = {{
  {0.00,  26,  44,  71},
  {0.25,   0,  97, 137},
  {0.50,  43, 160, 158},
  {0.75, 202, 190,  76},
  {1.00, 255, 243, 128},
}};
}

QColor ColorMap::color(Scheme scheme, double t)
{
  switch (scheme)
  {
  case Ironbow:    return interpolate(kIronbow, t);
  case Viridis:    return interpolate(kViridis, t);
  case Grayscale:  return interpolate(kGrayscale, t);
  case Blue:       return interpolate(kBlue, t);
  case BlueYellow: return interpolate(kBlueYellow, t);
  case Rainbow:
  default:         return interpolate(kRainbow, t);
  }
}

QColor ColorMap::noDataColor(Scheme scheme)
{
  switch (scheme)
  {
  case Ironbow:    return QColor(0x73, 0x77, 0x7b);
  case Viridis:    return QColor(0x65, 0x6c, 0x74);
  case Grayscale:  return QColor(0x0a, 0x61, 0x82);
  case Blue:       return QColor(0xb4, 0x58, 0x12);
  case BlueYellow: return QColor(0x1d, 0x4e, 0xd8);
  case Rainbow:
  default:         return QColor(0x5e, 0x6a, 0x75);
  }
}

QString ColorMap::name(Scheme scheme)
{
  switch (scheme)
  {
  case Ironbow:    return QCoreApplication::translate("ColorMap", "Ironbow (thermal)");
  case Viridis:    return QCoreApplication::translate("ColorMap", "Viridis");
  case Grayscale:  return QCoreApplication::translate("ColorMap", "Grayscale");
  case Blue:       return QCoreApplication::translate("ColorMap", "Blue (monochrome)");
  case BlueYellow: return QCoreApplication::translate("ColorMap", "Blue-yellow");
  case Rainbow:
  default:         return QCoreApplication::translate("ColorMap", "Rainbow");
  }
}

QList<ColorMap::Scheme> ColorMap::all()
{
  return { Rainbow, Ironbow, Viridis, Grayscale, Blue, BlueYellow };
}
