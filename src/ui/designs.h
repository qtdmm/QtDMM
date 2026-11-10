// Copyright (c) 2026 The QtDMM developers
// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <QBrush>
#include <QColor>
#include <QIcon>
#include <QString>

class QWidget;

/// The colour designs of the main window: the system's look, Silver (shown
/// as "Light": polished aluminium, gradients only), Dark, and the two on a
/// brushed metal texture, BrushedMetal ("Light Metal", Silver's colours) and
/// DarkMetal ("Dark Metal", Dark's colours).
///
/// A design is more than a palette: Fusion ignores gradient brushes in the
/// palette on most widgets, so the gradients come from a style sheet
/// (toolbars, status bar), the views' background brush and what paints
/// itself (the MDI area background, the graph). The LCD tint and the
/// analog meter's style are settings of their own and stay as they are.
namespace Designs
{
  enum Design { System, Silver, Dark, BrushedMetal, DarkMetal };

  /// "system", "silver", "dark", "brushed", "dark-metal" - the settings value.
  QString name(Design d);
  Design fromName(const QString &name);

  /// Sets the application palette, style and style sheet. The first call
  /// remembers the system's palette and style for System.
  void apply(Design d);
  Design current();

  /// Background of the MDI area.
  QBrush areaBrush(Design d);
  /// Background of the views (display, meter, table); Qt::NoBrush = the
  /// palette's window colour.
  QBrush frameBrush(Design d);
  /// The metal designs: the texture of frameBrush() stretched over @p view,
  /// at each size anew (@p on), or no more.
  void fitTexture(QWidget *view, bool on);

  /// Colours the graph uses in design @p d (System: invalid = keep the
  /// colours from the settings).
  struct GraphColors
  {
    QBrush background;   ///< chart background
    QColor grid;
    QColor labels;       ///< axis labels and titles
    QColor data;         ///< curve colour, used while the setting is the default
  };
  GraphColors graphColors(Design d);

  /// The symbols (Settings -> General -> Symbols):
  /// - Coloured: Oxygen (:/icons/qtdmm-oxygen), the default
  /// - Plain: monochrome Breeze (:/icons/qtdmm-breeze, -dark), light or dark
  ///   to match the palette
  /// - SystemIcons: the desktop's icon theme (Linux/BSD, where there is
  ///   one), the plain set filling the gaps - QtDMM's own symbols are
  ///   only there ("System + plain")
  /// - SystemColoured: the same with the coloured set ("System + coloured"),
  ///   for a coloured desktop theme such as Oxygen
  enum IconSet { SystemIcons, Plain, Coloured, SystemColoured };
  /// "system-plain", "plain", "colored", "system-colored" - the settings
  /// value (Icons/set). "system", from before the two, reads as the one
  /// systemThemeColoured() says.
  QString iconSetName(IconSet set);
  IconSet iconSetFromName(const QString &name);
  /// Whether the desktop has an icon theme SystemIcons could use.
  bool systemIconsAvailable();
  /// Whether the desktop's icon theme is a coloured one (Oxygen, Papirus,
  /// Humanity, Tango ...): then the coloured set fills its gaps by default.
  bool systemThemeColoured(const QString &theme);
  /// The desktop's icon theme (empty on Windows and macOS).
  QString systemIconTheme();
  /// Called by apply() with the last choice; widgets repaint with the new
  /// symbols.
  void setIconSet(IconSet set);
  IconSet iconSet();
  /// The built-in set in use ("qtdmm-oxygen", "qtdmm-breeze" or
  /// "qtdmm-breeze-dark"); under SystemIcons the plain one, under
  /// SystemColoured the coloured one, as the fallback.
  QString iconTheme();
  /// A symbol of the plain set (light or dark to match the palette),
  /// whichever set the toolbar has: for lists that should look alike.
  QIcon plainIcon(const QString &name);
}
