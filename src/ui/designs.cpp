// Copyright (c) 2026 The QtDMM developers
// SPDX-License-Identifier: GPL-3.0-or-later
#include "ui/designs.h"

#include <QApplication>
#include <QIcon>
#include <QWidget>
#include <QLinearGradient>
#include <QPalette>
#include <QStyle>
#include <QStyleFactory>

namespace
{
Designs::Design g_current = Designs::System;
bool g_saved = false;
QPalette g_systemPalette;
QString g_systemStyle;
QString g_systemIconTheme;   ///< the desktop's icon theme, empty on Windows/macOS
Designs::IconSet g_iconSet = Designs::Coloured;

// the first call remembers what the system had, for System and the icons
void saveSystem()
{
  if (g_saved)
    return;
  g_systemPalette = QApplication::palette();
  g_systemStyle = QApplication::style()->name();
  g_systemIconTheme = QIcon::themeName();
  QIcon::setThemeSearchPaths(QStringList(":/icons") + QIcon::themeSearchPaths());
  g_saved = true;
}

void applyIcons()
{
  const QString builtIn = Designs::iconTheme();
  if (g_iconSet == Designs::SystemIcons && Designs::systemIconsAvailable())
  {
    QIcon::setThemeName(g_systemIconTheme);
    QIcon::setFallbackThemeName(builtIn);
  }
  else
  {
    QIcon::setThemeName(builtIn);
    QIcon::setFallbackThemeName(QString());
  }
  // Qt reads each theme once and keeps it, with the fallback of that moment:
  // the window looks symbols up before the settings set one, and hicolor -
  // where Qt adds the fallback, as every desktop theme ends there - stayed
  // without it (QtDMM's own symbols showed only their text under System).
  // New search paths make Qt read the themes again.
  QIcon::setThemeSearchPaths(QIcon::themeSearchPaths());
  // icons from fromTheme() look the theme up again when painted
  for (QWidget *w : QApplication::allWidgets())
    w->update();
}

// top-to-bottom gradient over the whole painted object
QLinearGradient vgrad(std::initializer_list<QPair<double, QColor>> stops)
{
  QLinearGradient g(0, 0, 0, 1);
  g.setCoordinateMode(QGradient::ObjectBoundingMode);
  for (const auto &s : stops)
    g.setColorAt(s.first, s.second);
  return g;
}

QPalette darkPalette()
{
  QPalette p;
  p.setColor(QPalette::Window, QColor(45, 45, 48));
  p.setColor(QPalette::WindowText, QColor(220, 220, 220));
  p.setColor(QPalette::Base, QColor(30, 30, 32));
  p.setColor(QPalette::AlternateBase, QColor(40, 40, 44));
  p.setColor(QPalette::Text, QColor(220, 220, 220));
  p.setColor(QPalette::Button, QColor(55, 55, 60));
  p.setColor(QPalette::ButtonText, QColor(220, 220, 220));
  p.setColor(QPalette::ToolTipBase, QColor(55, 55, 60));
  p.setColor(QPalette::ToolTipText, QColor(220, 220, 220));
  p.setColor(QPalette::Highlight, QColor(0x33, 0x66, 0x99));
  p.setColor(QPalette::HighlightedText, Qt::white);
  p.setColor(QPalette::Light, QColor(80, 80, 86));
  p.setColor(QPalette::Mid, QColor(70, 70, 75));
  p.setColor(QPalette::Dark, QColor(25, 25, 27));
  for (QPalette::ColorRole r : { QPalette::Text, QPalette::WindowText, QPalette::ButtonText })
    p.setColor(QPalette::Disabled, r, QColor(120, 120, 120));
  return p;
}

// polished aluminium: the palette is solid, the gradients come from the
// style sheet and the views
QPalette silverPalette()
{
  QPalette p;
  p.setColor(QPalette::Window, QColor("#dfe2e6"));
  p.setColor(QPalette::Button, QColor("#e9ebee"));
  p.setColor(QPalette::WindowText, QColor("#20242a"));
  p.setColor(QPalette::ButtonText, QColor("#20242a"));
  p.setColor(QPalette::Base, QColor("#fbfbfc"));
  p.setColor(QPalette::AlternateBase, QColor("#eceef1"));
  p.setColor(QPalette::Text, QColor("#20242a"));
  p.setColor(QPalette::Highlight, QColor("#5b7fa6"));
  p.setColor(QPalette::HighlightedText, Qt::white);
  p.setColor(QPalette::Light, QColor("#ffffff"));
  p.setColor(QPalette::Mid, QColor("#9aa1ab"));
  p.setColor(QPalette::Dark, QColor("#7d848e"));
  for (QPalette::ColorRole r : { QPalette::Text, QPalette::WindowText, QPalette::ButtonText })
    p.setColor(QPalette::Disabled, r, QColor("#9aa1ab"));
  return p;
}

const char *kSilverSheet =
  "QToolBar { background: qlineargradient(x1:0, y1:0, x2:0, y2:1,"
  "  stop:0 #ffffff, stop:0.45 #e4e7eb, stop:0.55 #d3d8de, stop:1 #aeb5bf);"
  "  border-bottom: 1px solid #8e96a1; }"
  "QStatusBar { background: qlineargradient(x1:0, y1:0, x2:0, y2:1,"
  "  stop:0 #d9dde2, stop:1 #aab1ba); }"
  "QMdiSubWindow { background: #dfe2e6; }";

const char *kDarkSheet =
  "QToolBar { border-bottom: 1px solid #1c1c1e; }";
}

QString Designs::name(Design d)
{
  switch (d)
  {
    case Silver: return "silver";
    case Dark: return "dark";
    default: return "system";
  }
}

Designs::Design Designs::fromName(const QString &name)
{
  if (name == "silver")
    return Silver;
  if (name == "dark")
    return Dark;
  return System;
}

Designs::Design Designs::current()
{
  return g_current;
}

void Designs::apply(Design d)
{
  saveSystem();
  g_current = d;
  if (d == System)
  {
    qApp->setStyleSheet(QString());
    QApplication::setStyle(QStyleFactory::create(g_systemStyle));
    QApplication::setPalette(g_systemPalette);
  }
  else
  {
    // native styles (Windows, macOS) take no palette; Fusion does
    QApplication::setStyle(QStyleFactory::create("Fusion"));
    QApplication::setPalette(d == Dark ? darkPalette() : silverPalette());
    qApp->setStyleSheet(d == Silver ? kSilverSheet : kDarkSheet);
  }
  applyIcons();
}

QString Designs::iconTheme()
{
  // System falls back on the plain set: its monochrome symbols sit better
  // among a desktop theme's than the coloured ones
  if (g_iconSet == Coloured)
    return "qtdmm-oxygen";   // good on light and dark alike
  // by the window colour, so System on a dark desktop gets the dark set
  return QApplication::palette().color(QPalette::Window).lightness() < 128 ? "qtdmm-breeze-dark" : "qtdmm-breeze";
}

QString Designs::iconSetName(IconSet set)
{
  switch (set)
  {
    case SystemIcons: return "system";
    case Plain:       return "plain";
    default:          return "colored";
  }
}

Designs::IconSet Designs::iconSetFromName(const QString &name)
{
  if (name == "system")
    return SystemIcons;
  if (name == "plain")
    return Plain;
  return Coloured;
}

bool Designs::systemIconsAvailable()
{
#if defined(Q_OS_WIN) || defined(Q_OS_MACOS)
  return false;   // no desktop icon themes there
#else
  saveSystem();
  // "hicolor" is the bare freedesktop fallback, not a theme of the desktop
  return !g_systemIconTheme.isEmpty() && g_systemIconTheme != "hicolor";
#endif
}

void Designs::setIconSet(IconSet set)
{
  saveSystem();
  g_iconSet = set;
  applyIcons();
}

Designs::IconSet Designs::iconSet()
{
  return g_iconSet;
}

QBrush Designs::areaBrush(Design d)
{
  switch (d)
  {
    case Silver:
      return vgrad({ { 0, QColor("#c3c9d1") }, { 0.5, QColor("#9aa2ac") }, { 1, QColor("#6f7781") } });
    case Dark:
      return QColor(30, 30, 32);
    default:
      // the windows' own colour: the gaps between them show no darker
      // strips that meet in steps at the corners
      return g_saved ? g_systemPalette.window().color() : QApplication::palette().window().color();
  }
}

QBrush Designs::frameBrush(Design d)
{
  if (d == Silver)
    return vgrad({ { 0, QColor("#fdfdfe") }, { 0.3, QColor("#e3e6ea") }, { 0.55, QColor("#cdd2d8") },
                   { 1, QColor("#9ea6b0") } });
  return Qt::NoBrush;
}

Designs::GraphColors Designs::graphColors(Design d)
{
  GraphColors c;
  if (d == Silver)
  {
    c.background = vgrad({ { 0, QColor("#ffffff") }, { 0.6, QColor("#eceef1") }, { 1, QColor("#d2d7de") } });
    c.grid = QColor("#b8bec6");
    c.labels = QColor("#20242a");
  }
  else if (d == Dark)
  {
    c.background = QColor(36, 36, 40);
    c.grid = QColor(80, 80, 86);
    c.labels = QColor(210, 210, 210);
    c.data = QColor(0x56, 0xb4, 0xe9);   // sky blue: the default blue is hard to see on dark grey
  }
  return c;
}
