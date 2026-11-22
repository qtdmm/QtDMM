// Copyright (c) 2026 The QtDMM developers
// SPDX-License-Identifier: GPL-3.0-or-later
#include "ui/designs.h"

#include <QApplication>
#include <QFile>
#include <QIcon>
#include <QWidget>
#include <QLinearGradient>
#include <QPalette>
#include <QEvent>
#include <QPixmap>
#include <QProxyStyle>
#include <qdrawutil.h>
#include <QStyle>
#include <QStyleFactory>
#include <QStyleOption>

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
  if ((g_iconSet == Designs::SystemIcons || g_iconSet == Designs::SystemColoured) && Designs::systemIconsAvailable())
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
  // Qt's default is a half transparent black: invisible on the dark fields
  p.setColor(QPalette::PlaceholderText, QColor(130, 130, 136));
  p.setColor(QPalette::Disabled, QPalette::PlaceholderText, QColor(85, 85, 90));
  // Qt's link blue is for light ground: lighter here (the device info, the help)
  p.setColor(QPalette::Link, QColor(0x6a, 0xb0, 0xf3));
  p.setColor(QPalette::LinkVisited, QColor(0xb0, 0x8c, 0xf0));
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

// Fusion with the designs' menus (below) and, in the dark ones, a light
// edge on check boxes and radio buttons: Fusion edges them with the window
// colour a bit darker, on the dark menus and dialogs an empty one was
// hardly there.
class DesignStyle : public QProxyStyle
{
public:
  explicit DesignStyle(bool dark) : QProxyStyle(QStringLiteral("Fusion")), m_dark(dark) {}
  void drawPrimitive(PrimitiveElement element, const QStyleOption *option, QPainter *painter,
                     const QWidget *widget) const override
  {
    // a context menu stood out from the side bar and the windows by its
    // shadow only, and without a compositor there is none: a background a
    // step away from the window's and a frame. Its items, highlight and text
    // stay Fusion's
    if (element == PE_PanelMenu)
    {
      const QColor window = option->palette.color(QPalette::Window);
      const QColor frame = option->palette.color(m_dark ? QPalette::Light : QPalette::Mid);
      const QBrush back(m_dark ? window.lighter(125) : window.darker(106));
      qDrawPlainRect(painter, option->rect, frame, 1, &back);
      return;
    }
    // a tool button: disabled without a frame, also when checked
    if (element == PE_PanelButtonTool && !(option->state & State_Enabled))
      return;
    // the arrow of a tool button looks the same checked or not: a frame only
    // under the mouse or pressed
    if (element == PE_IndicatorButtonDropDown && (option->state & State_On))
    {
      if (!(option->state & (State_Raised | State_Sunken)) || !(option->state & State_Enabled))
        return;
      QStyleOption copy(*option);
      copy.state &= ~State_On;
      QProxyStyle::drawPrimitive(element, &copy, painter, widget);
      return;
    }
    if (!m_dark || (element != PE_IndicatorCheckBox && element != PE_IndicatorRadioButton))
    {
      QProxyStyle::drawPrimitive(element, option, painter, widget);
      return;
    }
    // the edge is Window.darker(140): #b4b4ba gives #808085. Fusion reads
    // the options of a button (menus pass one as well)
    const auto *button = qstyleoption_cast<const QStyleOptionButton *>(option);
    if (!button)
    {
      QProxyStyle::drawPrimitive(element, option, painter, widget);
      return;
    }
    QStyleOptionButton copy(*button);
    copy.palette.setColor(QPalette::Window, QColor(0xb4, 0xb4, 0xba));
    QProxyStyle::drawPrimitive(element, &copy, painter, widget);
  }

private:
  bool m_dark;
};

bool isDark(Designs::Design d)
{
  return d == Designs::Dark || d == Designs::DarkMetal;
}

/// The texture of a metal design, a null pixmap for the others.
const QPixmap &metalTexture(Designs::Design d)
{
  static const QPixmap light(":/textures/brushed-metal.jpg");
  static const QPixmap dark(":/textures/dark-metal.jpg");
  static const QPixmap none;
  return d == Designs::BrushedMetal ? light : d == Designs::DarkMetal ? dark : none;
}

// the texture stretched over a view, anew at each size: the light across
// it once, as on a sheet of its own (filling ignores a brush's transform,
// so the picture itself is scaled)
class TextureFit : public QObject
{
public:
  using QObject::QObject;
  static void fit(QWidget *view)
  {
    const QPixmap &texture = metalTexture(Designs::current());
    if (texture.isNull() || view->size().isEmpty())
      return;
    QPalette pal = view->palette();
    pal.setBrush(QPalette::Window, QBrush(texture.scaled(view->size(), Qt::IgnoreAspectRatio,
                                                         Qt::SmoothTransformation)));
    view->setPalette(pal);
  }

protected:
  bool eventFilter(QObject *watched, QEvent *event) override
  {
    if (event->type() == QEvent::Resize)
      fit(static_cast<QWidget *>(watched));
    return false;
  }
};

const char *kSilverSheet =
  "QToolBar { background: qlineargradient(x1:0, y1:0, x2:0, y2:1,"
  "  stop:0 #ffffff, stop:0.45 #e4e7eb, stop:0.55 #d3d8de, stop:1 #aeb5bf);"
  "  border-bottom: 1px solid #8e96a1; }"
  "QStatusBar { background: qlineargradient(x1:0, y1:0, x2:0, y2:1,"
  "  stop:0 #d9dde2, stop:1 #aab1ba); }"
  "QMdiSubWindow { background: #dfe2e6; }";

// the metal designs: the side bar's colour from their mock-ups
const char *kBrushedSheet = "DeviceSidebar { background: #e9ecee; }";
const char *kDarkMetalSheet = "DeviceSidebar { background: #151a1f; }";

// Fusion draws the frame of a text field a bit darker than the window - on
// the dark fields it vanished, so they get a light edge of their own (not
// inside spin and combo boxes, those draw their own frame)
const char *kDarkSheet =
  "QToolBar { border-bottom: 1px solid #1c1c1e; }"
  "QLineEdit { border: 1px solid #626268; border-radius: 2px; padding: 1px 2px; }"
  "QLineEdit:focus { border-color: #4d88c2; }"
  "QLineEdit:disabled { border-color: #48484d; }"
  "QAbstractSpinBox QLineEdit, QComboBox QLineEdit { border: none; padding: 0; }";
}

QString Designs::name(Design d)
{
  switch (d)
  {
    case Silver: return "silver";
    case Dark: return "dark";
    case BrushedMetal: return "brushed";
    case DarkMetal: return "dark-metal";
    default: return "system";
  }
}

Designs::Design Designs::fromName(const QString &name)
{
  if (name == "silver")
    return Silver;
  if (name == "dark")
    return Dark;
  if (name == "brushed")
    return BrushedMetal;
  if (name == "dark-metal")
    return DarkMetal;
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
    // a palette without roles set: the desktop's, also when it switches
    // between light and dark later
    QApplication::setPalette(QPalette());
    g_systemPalette = QApplication::palette();
  }
  else
  {
    // native styles (Windows, macOS) take no palette; Fusion does
    // QApplication owns the style, the proxy its base style
    QApplication::setStyle(new DesignStyle(isDark(d)));
    QApplication::setPalette(isDark(d) ? darkPalette() : silverPalette());
    if (isDark(d))
      qApp->setStyleSheet(QString(kDarkSheet) + (d == DarkMetal ? kDarkMetalSheet : ""));
    else
      qApp->setStyleSheet(QString(kSilverSheet) + (d == BrushedMetal ? kBrushedSheet : ""));
  }
  // Qt does not always hand the new palette down to every widget: the
  // docked side bar kept the last design's now and then (System after Dark
  // on KDE). Each widget takes it from its parent afresh
  QEvent change(QEvent::ApplicationPaletteChange);
  const QWidgetList widgets = QApplication::allWidgets();
  for (QWidget *w : widgets)
    QCoreApplication::sendEvent(w, &change);
  applyIcons();
}

QString Designs::iconTheme()
{
  // System falls back on the plain set: its monochrome symbols sit better
  // among a desktop theme's than the coloured ones
  if (g_iconSet == Coloured || g_iconSet == SystemColoured)
    return "qtdmm-oxygen";   // good on light and dark alike
  // by the window colour, so System on a dark desktop gets the dark set
  return QApplication::palette().color(QPalette::Window).lightness() < 128 ? "qtdmm-breeze-dark" : "qtdmm-breeze";
}

QIcon Designs::plainIcon(const QString &name)
{
  const bool dark = QApplication::palette().color(QPalette::Window).lightness() < 128;
  const QString file = QString(":/icons/%1/22/%2.svg").arg(dark ? "qtdmm-breeze-dark" : "qtdmm-breeze", name);
  return QFile::exists(file) ? QIcon(file) : QIcon::fromTheme(name);
}

QString Designs::iconSetName(IconSet set)
{
  switch (set)
  {
    case SystemIcons:    return "system-plain";
    case SystemColoured: return "system-colored";
    case Plain:          return "plain";
    default:             return "colored";
  }
}

Designs::IconSet Designs::iconSetFromName(const QString &name)
{
  if (name == "system")   // before System + plain/coloured: by the desktop's theme
    return systemThemeColoured(systemIconTheme()) ? SystemColoured : SystemIcons;
  if (name == "system-plain")
    return SystemIcons;
  if (name == "system-colored")
    return SystemColoured;
  if (name == "plain")
    return Plain;
  return Coloured;
}

bool Designs::systemThemeColoured(const QString &theme)
{
  // the ones that draw in colour; Breeze, Adwaita, Yaru ... are plain
  static const QStringList coloured = { "oxygen", "kde_classic", "papirus", "epapirus", "humanity",
                                        "tango", "gnome", "elementary", "faenza", "numix" };
  const QString t = theme.toLower();
  for (const QString &c : coloured)
    if (t.startsWith(c))
      return true;
  return false;
}

QString Designs::systemIconTheme()
{
  saveSystem();
  return g_systemIconTheme;
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
    case BrushedMetal:
      // plain: the area paints in pieces, and a gradient over each piece
      // drew stripes across the background
      return QColor("#9aa2ac");
    case Dark:
    case DarkMetal:
      return QColor(30, 30, 32);
    default:
      // the windows' own colour: the gaps between them show no darker
      // strips that meet in steps at the corners
      return g_saved ? g_systemPalette.window().color() : QApplication::palette().window().color();
  }
}

void Designs::fitTexture(QWidget *view, bool on)
{
  static TextureFit *filter = new TextureFit(qApp);
  view->removeEventFilter(filter);
  if (!on)
    return;
  view->installEventFilter(filter);
  TextureFit::fit(view);
}

QBrush Designs::frameBrush(Design d)
{
  if (d == BrushedMetal || d == DarkMetal)
    return QBrush(metalTexture(d));
  if (d == Silver)
    return vgrad({ { 0, QColor("#fdfdfe") }, { 0.3, QColor("#e3e6ea") }, { 0.55, QColor("#cdd2d8") },
                   { 1, QColor("#9ea6b0") } });
  return Qt::NoBrush;
}

Designs::GraphColors Designs::graphColors(Design d)
{
  GraphColors c;
  if (d == Silver || d == BrushedMetal)
  {
    c.background = vgrad({ { 0, QColor("#ffffff") }, { 0.6, QColor("#eceef1") }, { 1, QColor("#d2d7de") } });
    c.grid = QColor("#b8bec6");
    c.labels = QColor("#20242a");
  }
  else if (isDark(d))
  {
    c.background = QColor(36, 36, 40);
    c.grid = QColor(80, 80, 86);
    c.labels = QColor(210, 210, 210);
    c.data = QColor(0x56, 0xb4, 0xe9);   // sky blue: the default blue is hard to see on dark grey
  }
  return c;
}
