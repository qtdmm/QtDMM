// Copyright (c) 2026 The QtDMM developers
// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <QString>
#include <QStringList>

/// The files used last, newest first, in an ini file of their own that all
/// instances share (workspaces.conf next to devices.conf). Each read and
/// write goes to the file, so another instance's change shows the next time.
/// A window shows as many as it likes of them (Sidebar/recent-workspaces).
/// Favourites are kept apart: shown first, not counted, never dropped.
class RecentFiles
{
public:
  /// @p file the ini file, empty keeps the list in memory only (tests);
  /// @p key the list's key in it; at most @p max are kept.
  explicit RecentFiles(const QString &file, const QString &key = QStringLiteral("Workspaces/recent"),
                       int max = 30);

  /// The files, newest first.
  QStringList list() const;
  /// @p path to the front, once (absolute and clean, as the list keeps them).
  void        add(const QString &path);
  /// @p path off the list, a favourite too.
  void        remove(const QString &path);
  /// The list emptied; the favourites stay.
  void        clear();

  /// The favourites, in the order they became one.
  QStringList favorites() const;
  bool        isFavorite(const QString &path) const;
  /// @p path a favourite (@p on) or no more.
  void        setFavorite(const QString &path, bool on);
  /// What to show: the favourites, then up to @p max of the others, newest first.
  QStringList shown(int max) const;

  /// @p list with @p path in front and no second time, at most @p max long.
  static QStringList added(QStringList list, const QString &path, int max);

private:
  void        store(const QStringList &list);
  void        storeFavorites(const QStringList &list);
  QString     favoritesKey() const { return m_key.section('/', 0, 0) + QStringLiteral("/favorites"); }

  QString     m_file;
  QString     m_key;
  int         m_max;
  QStringList m_memory;   ///< without a file
  QStringList m_memoryFavorites;
};
