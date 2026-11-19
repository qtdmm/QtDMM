// Copyright (c) 2026 The QtDMM developers
// SPDX-License-Identifier: GPL-3.0-or-later
#include "ui/mnemoniccheck.h"

#include <QAbstractButton>
#include <QAction>
#include <QApplication>
#include <QDockWidget>
#include <QGroupBox>
#include <QLabel>
#include <QMap>
#include <QMdiSubWindow>
#include <QMenu>
#include <QMultiMap>
#include <QScrollArea>
#include <QSet>
#include <QStackedWidget>
#include <QTabWidget>
#include <QToolBar>
#include <QToolButton>

namespace
{
QChar mnemonic(QString text)
{
  text.remove("&&");
  const int i = text.indexOf('&');
  return i >= 0 && i + 1 < text.size() ? text[i + 1].toLower() : QChar();
}

// ... and the letters taken there, to pick a free one
QString conflictLine(const QString &where, QChar c, const QStringList &texts, const QList<QChar> &taken)
{
  QString letters;
  for (QChar t : taken)
    letters += t;
  return QString("%1: Alt+%2 in %3 (taken: %4)").arg(where, QString(c).toUpper(), texts.join(" | "), letters);
}

// the widgets of @p window shown together, grouped by stacked page
void checkWindow(QWidget *window, const QString &tag, QStringList &out)
{
  auto scopeOf = [window](QWidget *w) -> QWidget *
  {
    for (QWidget *p = w; p && p != window; p = p->parentWidget())
      if (p->parentWidget() && qobject_cast<QStackedWidget *>(p->parentWidget()))
        return p;
    return window;
  };
  QMap<QWidget *, QMultiMap<QChar, QString>> scopes;
  for (QWidget *w : window->findChildren<QWidget *>())
  {
    if (w->window() != window || !w->isVisibleTo(window))
      continue;
    if (qobject_cast<QToolButton *>(w) && qobject_cast<QToolBar *>(w->parentWidget()))
      continue;   // toolbar buttons show no letter
    QString text;
    if (auto *b = qobject_cast<QAbstractButton *>(w))
      text = b->text();
    else if (auto *l = qobject_cast<QLabel *>(w); l && l->buddy())
      text = l->text();
    else if (auto *g = qobject_cast<QGroupBox *>(w); g && g->isCheckable())
      text = g->title();
    if (QChar c = mnemonic(text); !c.isNull())
      scopes[scopeOf(w)].insert(c, "'" + text + "'");
  }
  for (auto it = scopes.cbegin(); it != scopes.cend(); ++it)
  {
    // a page is named by the class of the widget in its scroll area
    QString page = it.key() == window ? QString() : QString(it.key()->metaObject()->className());
    if (auto *sa = qobject_cast<QScrollArea *>(it.key()); sa && sa->widget())
      page = sa->widget()->metaObject()->className();
    const QString where = QString(window->metaObject()->className()) + (page.isEmpty() ? QString() : "/" + page) + tag;
    for (QChar c : it.value().uniqueKeys())
      if (it.value().values(c).size() > 1)
        out << conflictLine(where, c, it.value().values(c), it.value().uniqueKeys());
  }
  for (QTabWidget *tabs : window->findChildren<QTabWidget *>())
  {
    QMultiMap<QChar, QString> letters;
    for (int i = 0; i < tabs->count(); ++i)
      if (QChar c = mnemonic(tabs->tabText(i)); !c.isNull())
        letters.insert(c, "'" + tabs->tabText(i) + "'");
    for (QChar c : letters.uniqueKeys())
      if (letters.values(c).size() > 1)
        out << conflictLine(QString(window->metaObject()->className()) + " tabs", c, letters.values(c), letters.uniqueKeys());
  }
}
}

QStringList MnemonicCheck::run(QWidget *mainWindow)
{
  QStringList out;
  if (auto *menuAction = mainWindow->findChild<QAction *>("action_Menu"))
    menuAction->trigger();   // the main menu is built on first use
  for (QMenu *m : mainWindow->findChildren<QMenu *>())
    m->hide();
  // every view shown, so their buttons count
  for (QDockWidget *d : mainWindow->findChildren<QDockWidget *>())
    d->show();
  for (QMdiSubWindow *w : mainWindow->findChildren<QMdiSubWindow *>())
    w->show();

  QList<QWidget *> windows = { mainWindow };
  for (QWidget *w : mainWindow->findChildren<QWidget *>())
    if (w->isWindow() && !qobject_cast<QMenu *>(w) && !windows.contains(w))
      windows << w;

  // key sequences: two actions with the same one in one window
  for (QWidget *window : windows)
  {
    QMultiMap<QString, QString> keys;
    for (QAction *a : window->findChildren<QAction *>())
    {
      QWidget *owner = qobject_cast<QWidget *>(a->parent());
      if (owner && owner->window() != window)
        continue;
      for (const QKeySequence &k : a->shortcuts())
        if (!k.isEmpty())
          keys.insert(k.toString(), "'" + a->text() + "'");
    }
    for (const QString &k : keys.uniqueKeys())
    {
      QStringList who = keys.values(k);
      who.removeDuplicates();
      if (who.size() > 1)
        out << QString("%1: key %2 for %3").arg(window->metaObject()->className(), k, who.join(" | "));
    }
  }

  // menus
  QSet<QMenu *> menus;
  for (QMenu *m : mainWindow->findChildren<QMenu *>())
    menus << m;
  for (QMenu *m : menus)
  {
    QMultiMap<QChar, QString> letters;
    for (QAction *a : m->actions())
      if (!a->isSeparator() && a->isVisible())
        if (QChar c = mnemonic(a->text()); !c.isNull())
          letters.insert(c, "'" + a->text() + "'");
    const QString where = "menu" + (m->title().isEmpty() ? QString() : " '" + m->title() + "'");
    for (QChar c : letters.uniqueKeys())
      if (letters.values(c).size() > 1)
        out << conflictLine(where, c, letters.values(c), letters.uniqueKeys());
  }

  // windows: every page of a stacked dialog in turn; the meter page once
  // per kind of port
  for (QWidget *window : windows)
  {
    bool stacked = false;
    for (QStackedWidget *stack : window->findChildren<QStackedWidget *>())
      if (stack->window() == window && stack->count() > 1)
      {
        stacked = true;
        for (int i = 0; i < stack->count(); ++i)
        {
          stack->setCurrentIndex(i);
          checkWindow(window, QString(), out);
        }
      }
    if (!stacked)
      checkWindow(window, QString(), out);

    QWidget *serial = window->findChild<QWidget *>("ButtonGroup11");
    if (!serial || serial->window() != window)
      continue;
    for (QStackedWidget *stack : window->findChildren<QStackedWidget *>())
      for (int i = 0; i < stack->count(); ++i)
        if (stack->widget(i)->isAncestorOf(serial))
          stack->setCurrentIndex(i);
    QWidget *protocol = window->findChild<QWidget *>("ui_protocol");
    // the port parameters and the protocol shown (set by hand); they belong
    // to a cable meter like the port row
    QWidget *serialBox = window->findChild<QWidget *>("ui_serialBox");
    if (QWidget *box = window->findChild<QWidget *>("ui_advancedBox"))
      box->show();
    const QMap<QString, QWidget *> ports { { "serial", serial },
                                           { "ble", window->findChild<QWidget *>("ui_bleGroup") },
                                           { "sigrok", window->findChild<QWidget *>("ui_sigrokGroup") } };
    for (auto port = ports.cbegin(); port != ports.cend(); ++port)
    {
      if (!port.value())
        continue;
      for (QWidget *group : ports)
        if (group)
          group->setVisible(group == port.value());
      if (serialBox)
        serialBox->setVisible(port.value() == serial);
      if (protocol)
        protocol->show();
      checkWindow(window, " [" + port.key() + "]", out);
    }
  }
  out.removeDuplicates();
  return out;
}
