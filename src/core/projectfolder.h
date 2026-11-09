// Copyright (c) 2026 The QtDMM developers
// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <QString>

/// The open project basket's folder for the save dialogs: while one is open
/// they all start there (BasketWidget sets it), so a file of the project is
/// not put somewhere else by habit.
///
/// Files go into a folder of their kind in it (benches, recordings, images,
/// analysis, documents) while that is on (setByType(), default): made with
/// the first file of the kind, named in English on disk; project.json keeps
/// the name in each language for the project view ("folders").
namespace ProjectFolder
{
enum Kind
{
  Root,         ///< the project folder itself
  Benches,      ///< .qtdmm-workspace
  Recordings,   ///< recordings, readings, any CSV
  Images,       ///< pictures of the graph, the Poincaré plot
  Analysis,     ///< LabPlot projects and their CSV
  Documents     ///< PDF, log, anything else
};

/// The basket's folder, empty for none (BasketWidget::setFolder()).
void    set(const QString &folder);
/// The folder set, empty while no basket is open.
QString current();
/// Files into folders of their kind (Basket/by-type, default on).
void    setByType(bool on);
bool    byType();
/// The kind a file added to a project is filed under, by its suffix.
Kind    kindOf(const QString &file);
/// The folder's name on disk ("recordings"), empty for Root.
QString folderName(Kind kind);
/// The folder for @p kind in the project @p project: made when missing and
/// entered in project.json with its names. The project itself while
/// byType() is off, for Root and when it cannot be made.
QString dirFor(const QString &project, Kind kind);
/// Where a save dialog starts: with a basket open the file name of
/// @p proposal in its folder (the folder itself for an empty @p proposal),
/// else @p proposal as it is.
QString proposal(const QString &proposal);

/// A save dialog's proposal in the folder of its kind: the folder is made for
/// the dialog and removed again when it stays empty (cancelled, or the file
/// went elsewhere). Keep it until the file is written.
class Proposal
{
public:
  Proposal(const QString &proposal, Kind kind);
  ~Proposal();
  Proposal(const Proposal &) = delete;
  Proposal &operator=(const Proposal &) = delete;
  /// The path to propose.
  QString path() const { return m_path; }
  /// The file chosen in the dialog, written later: its folder stays.
  void    keep(const QString &chosen);

private:
  QString m_path;
  QString m_made;   ///< the folder made for the dialog, empty: none
};
}
