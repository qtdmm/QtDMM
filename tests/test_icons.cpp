// Copyright (c) 2026 The QtDMM developers
// SPDX-License-Identifier: GPL-3.0-or-later
// The symbol sets: under "System" the desktop's theme comes first, QtDMM's
// own symbols (and what the desktop lacks) from the plain set - also when
// symbols were looked up before the set was chosen.
#include <QApplication>
#include <QDebug>
#include <QDir>
#include <QIcon>
#include <QImage>
#include <QPainter>
#include <QTemporaryDir>

#include "ui/designs.h"
#include "testutil.h"

#if !defined(Q_OS_WIN) && !defined(Q_OS_MACOS)
#endif

int main(int argc, char **argv)
{
  qputenv("QT_QPA_PLATFORM", "offscreen");
  QApplication app(argc, argv);
  app.setApplicationName("QtDMM");
#if defined(Q_OS_WIN) || defined(Q_OS_MACOS)
  qInfo() << "no desktop icon themes here - skipped";
  return 0;
#else
  // a desktop theme like Breeze or Oxygen: Inherits=hicolor, with a red
  // document-save and none of QtDMM's symbols
  QTemporaryDir tmp;
  QDir(tmp.path()).mkpath("desktop/22");
  QFile index(tmp.filePath("desktop/index.theme"));
  check(index.open(QIODevice::WriteOnly), "index.theme written");
  index.write("[Icon Theme]\nName=desktop\nInherits=hicolor\nDirectories=22\n\n[22]\nSize=22\nType=Fixed\n");
  index.close();
  // hicolor exists on every desktop; without it Qt would take the fallback
  // theme in its place and hide the problem
  QDir(tmp.path()).mkpath("hicolor/22");
  QFile hicolor(tmp.filePath("hicolor/index.theme"));
  check(hicolor.open(QIODevice::WriteOnly), "hicolor.theme written");
  hicolor.write("[Icon Theme]\nName=hicolor\nDirectories=22\n\n[22]\nSize=22\nType=Fixed\n");
  hicolor.close();
  QImage red(22, 22, QImage::Format_ARGB32);
  red.fill(Qt::red);
  red.save(tmp.filePath("desktop/22/document-save.png"));
  QIcon::setThemeSearchPaths({ tmp.path() });
  QIcon::setThemeName("desktop");

  check(Designs::systemIconsAvailable(), "the fake desktop theme counts as a desktop theme");
  // the window looks symbols up before the settings choose a set: Qt reads
  // the themes then, before there is a fallback
  QIcon::fromTheme("qtdmm-dmm").pixmap(22);
  Designs::setIconSet(Designs::SystemIcons);
  const QPixmap own = QIcon::fromTheme("qtdmm-dmm").pixmap(22);
  check(!own.isNull(), "System: QtDMM's own symbol comes from the plain set");
  const QPixmap missing = QIcon::fromTheme("media-record").pixmap(22);
  check(!missing.isNull(), "System: a symbol the desktop lacks comes from the plain set");
  const QImage save = QIcon::fromTheme("document-save").pixmap(22).toImage();
  check(!save.isNull() && save.pixelColor(11, 11) == QColor(Qt::red), "System: the desktop's own symbol wins");

  Designs::setIconSet(Designs::Plain);
  const QImage plainSave = QIcon::fromTheme("document-save").pixmap(22).toImage();
  check(!plainSave.isNull() && plainSave.pixelColor(11, 11) != QColor(Qt::red), "Plain: the built-in set, not the desktop's");
  Designs::setIconSet(Designs::Coloured);
  check(!QIcon::fromTheme("qtdmm-dmm").pixmap(22).isNull(), "Coloured: QtDMM's own symbol");

  // System + coloured: the desktop's first, the coloured set for the rest
  Designs::setIconSet(Designs::SystemColoured);
  check(Designs::iconTheme() == "qtdmm-oxygen", "System + coloured: the coloured set fills the gaps");
  const QImage colouredSave = QIcon::fromTheme("document-save").pixmap(22).toImage();
  check(!colouredSave.isNull() && colouredSave.pixelColor(11, 11) == QColor(Qt::red), "System + coloured: the desktop's own symbol wins");
  check(!QIcon::fromTheme("media-record").pixmap(22).isNull(), "System + coloured: what the desktop lacks");
  Designs::setIconSet(Designs::SystemIcons);
  check(Designs::iconTheme().startsWith("qtdmm-breeze"), "System + plain: the plain set fills the gaps");
  // the names in the settings; "system" from before by the desktop's theme
  check(Designs::iconSetFromName("system-colored") == Designs::SystemColoured
          && Designs::iconSetFromName("system-plain") == Designs::SystemIcons
          && Designs::iconSetName(Designs::SystemColoured) == "system-colored"
          && Designs::iconSetName(Designs::SystemIcons) == "system-plain", "settings names of the two");
  check(Designs::systemThemeColoured("oxygen") && Designs::systemThemeColoured("ePapirus-Dark")
          && !Designs::systemThemeColoured("breeze") && !Designs::systemThemeColoured("Adwaita"),
        "coloured desktop themes: Oxygen, Papirus yes; Breeze, Adwaita no");
  check(Designs::iconSetFromName("system") == Designs::SystemIcons, "old 'system' with a theme not known as coloured: plain");

  // the project's list: folders and file types in every built-in set
  for (Designs::IconSet set : { Designs::Coloured, Designs::Plain })
  {
    Designs::setIconSet(set);
    for (const QString &name : { "folder", "text-csv", "x-office-spreadsheet", "image-x-generic", "application-pdf",
                                 "office-chart-line", "qtdmm-arrange", "text-x-generic" })
      check(!QIcon::fromTheme(name).pixmap(16).isNull(), Designs::iconTheme() + ": " + name);
  }

  if (failed)
    qWarning() << failed << "check(s) failed";
  else
    qInfo() << "all checks passed";
  return failed ? 1 : 0;
#endif
}
