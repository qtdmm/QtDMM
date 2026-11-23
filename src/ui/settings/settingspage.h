//======================================================================
// File:		prefwidget.h
// Author:	Matthias Toussaint
// Created:	Sat Oct 19 14:22:16 CEST 2002
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

class Settings;
class QFormLayout;

/// Base class of the pages in the settings dialog (SettingsDialog).
///
/// A page owns a group of settings keys. It loads them into its widgets in
/// defaultsSLOT(), writes the widgets back with applySLOT() and resets to the
/// built-in values with factoryDefaultsSLOT(). SettingsDialog lists the pages by
/// label() and icon() and calls the three slots for all pages at once.
class SettingsPage : public QWidget
{
  Q_OBJECT
public:
  SettingsPage(QWidget *parent = nullptr);
  /// Category name shown in the dialog's list.
  QString label() const { return m_label; }
  /// One line under the page's title: what the page is for.
  QString description() const { return m_description; }
  /// The page's symbol: from the plain set, whichever symbols the toolbar
  /// has, so the list looks alike.
  QIcon   icon() const;
  /// Page id = SettingsDialog::PageType, also the index in the page stack.
  void    setId(int id) { m_id = id; }
  int     id() const { return m_id; }
  void    setCfg(Settings *cfg) { m_cfg = cfg; }

public Q_SLOTS:
  /// Loads the stored settings (or their defaults) into the widgets.
  virtual void defaultsSLOT() = 0;
  /// Resets the widgets to the built-in defaults.
  virtual void factoryDefaultsSLOT() = 0;
  /// Writes the widgets' values to the Settings (staged until save()).
  virtual void applySLOT() = 0;

protected:
  /// The page's form on @p parent (default: the page): labels right-aligned,
  /// the fields at their own width.
  QFormLayout *createForm(QWidget *parent = nullptr);
  /// A section title, bold and without a frame; space above it separates
  /// it from the section before.
  QLabel  *addSection(QFormLayout *form, const QString &title);
  /// The same with a widget as the title (a check box that switches the
  /// section on).
  void     addSection(QFormLayout *form, QWidget *title);
  /// All labels of @p form as wide as the widest, shown or not: the
  /// fields stay put when rows come and go.
  static void alignLabels(QFormLayout *form);
  /// Shows or hides the row of @p field with its label.
  static void showRow(QFormLayout *form, QWidget *field, bool show);
  /// Fields side by side in one form row (a value and its unit, a pair).
  static QWidget *row(std::initializer_list<QWidget *> widgets, bool stretch = true);

  Settings *m_cfg;
  QString   m_label;
  QString   m_description;
  QString   m_iconName;   ///< icon theme name, see icon()
  int       m_id;
};

