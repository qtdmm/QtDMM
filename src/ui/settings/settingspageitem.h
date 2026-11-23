//======================================================================
// File:		configitem.h
// Author:	Matthias Toussaint
// Created:	Sat Oct 19 17:36:58 CEST 2002
//----------------------------------------------------------------------
// This file is part of QtDMM.
//
// QtDMM is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
//
// QtDMM is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with QtDMM.  If not, see <http://www.gnu.org/licenses/>.
//----------------------------------------------------------------------
// Copyright (c) 2002 Matthias Toussaint
// SPDX-License-Identifier: GPL-3.0-or-later
//======================================================================

#pragma once

#include <QtGui>
#include <QtWidgets>

/// An entry in the settings dialog's category list, carrying its page id.
class SettingsPageItem : public QListWidgetItem
{
public:
  SettingsPageItem(int id, const QIcon &icon, const QString &label, QListWidget *parent);
  int id() const { return m_id; }

protected:
  int	m_id;

};

