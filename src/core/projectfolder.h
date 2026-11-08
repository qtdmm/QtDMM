// Copyright (c) 2026 The QtDMM developers
// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <QString>

/// The open project basket's folder for the save dialogs: while one is open
/// they all start there (BasketWidget sets it), so a file of the project is
/// not put somewhere else by habit.
namespace ProjectFolder
{
/// The basket's folder, empty for none (BasketWidget::setFolder()).
void    set(const QString &folder);
/// The folder set, empty while no basket is open.
QString current();
/// Where a save dialog starts: with a basket open the file name of
/// @p proposal in its folder (the folder itself for an empty @p proposal),
/// else @p proposal as it is.
QString proposal(const QString &proposal);
}
