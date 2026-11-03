// Copyright (c) 2026 The QtDMM developers
// SPDX-License-Identifier: GPL-3.0-or-later
#include "core/recentfiles.h"

#include <QDir>
#include <QFileInfo>
#include <QSettings>

namespace
{
QString clean(const QString &path)
{
  return QDir::cleanPath(QFileInfo(path).absoluteFilePath());
}
}

RecentFiles::RecentFiles(const QString &file, const QString &key, int max) :
  m_file(file),
  m_key(key),
  m_max(max)
{
}

QStringList RecentFiles::list() const
{
  if (m_file.isEmpty())
    return m_memory;
  return QSettings(m_file, QSettings::IniFormat).value(m_key).toStringList();
}

void RecentFiles::add(const QString &path)
{
  store(added(list(), clean(path), m_max));
}

void RecentFiles::remove(const QString &path)
{
  QStringList l = list();
  l.removeAll(clean(path));
  store(l);
  setFavorite(path, false);
}

QStringList RecentFiles::favorites() const
{
  if (m_file.isEmpty())
    return m_memoryFavorites;
  return QSettings(m_file, QSettings::IniFormat).value(favoritesKey()).toStringList();
}

bool RecentFiles::isFavorite(const QString &path) const
{
  return favorites().contains(clean(path));
}

void RecentFiles::setFavorite(const QString &path, bool on)
{
  QStringList l = favorites();
  const QString p = clean(path);
  if (l.contains(p) == on)
    return;
  if (on)
    l << p;
  else
    l.removeAll(p);
  storeFavorites(l);
}

QStringList RecentFiles::shown(int max) const
{
  const QStringList fav = favorites();
  QStringList others = list();
  others.removeIf([&fav](const QString &f) { return fav.contains(f); });
  return fav + others.mid(0, qMax(0, max));
}

void RecentFiles::clear()
{
  store(QStringList());
}

QStringList RecentFiles::added(QStringList list, const QString &path, int max)
{
  list.removeAll(path);
  list.prepend(path);
  while (list.size() > qMax(0, max))
    list.removeLast();
  return list;
}

void RecentFiles::storeFavorites(const QStringList &list)
{
  if (m_file.isEmpty())
  {
    m_memoryFavorites = list;
    return;
  }
  QSettings s(m_file, QSettings::IniFormat);
  if (list.isEmpty())
    s.remove(favoritesKey());
  else
    s.setValue(favoritesKey(), list);
  s.sync();
}

void RecentFiles::store(const QStringList &list)
{
  if (m_file.isEmpty())
  {
    m_memory = list;
    return;
  }
  QSettings s(m_file, QSettings::IniFormat);
  if (list.isEmpty())
    s.remove(m_key);
  else
    s.setValue(m_key, list);
  s.sync();
}
