// Copyright (c) 2026 The QtDMM developers
// SPDX-License-Identifier: GPL-3.0-or-later
#include "core/projectfolder.h"

#include <QDir>
#include <QFileInfo>

namespace
{
QString &folder()
{
  static QString open;
  return open;
}
}

void ProjectFolder::set(const QString &dir)
{
  folder() = dir;
}

QString ProjectFolder::current()
{
  return folder();
}

QString ProjectFolder::proposal(const QString &proposal)
{
  if (folder().isEmpty())
    return proposal;
  const QString name = QFileInfo(proposal).fileName();
  return name.isEmpty() ? folder() : QDir(folder()).filePath(name);
}
