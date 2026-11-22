// Copyright (c) 2026 The QtDMM developers
// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <QDateTime>
#include <QImage>
#include <QString>

/// The photos of a measurement setup in the open project: in its folder
/// "photos" (ProjectFolder::Photos), named by the time they were taken
/// ("photo_2026-11-22_14-03-12.jpg"). A photo is kept as it came, its EXIF
/// data too; the turn the camera wrote there is applied when it is shown
/// (load()). A mark in the graph points to it by its path in the project
/// (RecordingStore::Mark::photo).
namespace PhotoStore
{
/// Whether @p data is a picture Qt reads; @p suffix gets its usual suffix
/// ("jpg", "png" ...).
bool    isPhoto(const QByteArray &data, QString *suffix = nullptr);
/// Saves @p data, a picture taken at @p when, in the photos of @p project.
/// Returns its full path, empty when it is no picture or was not written
/// (@p error says why).
QString save(const QString &project, const QByteArray &data, const QDateTime &when, QString *error = nullptr);
/// Copies the picture @p file into the photos of @p project, named by @p when.
QString addFile(const QString &project, const QString &file, const QDateTime &when, QString *error = nullptr);
/// The path of @p photo in @p project ("photos/photo_....jpg"); @p photo
/// itself when it lies outside.
QString relative(const QString &project, const QString &photo);
/// The full path of a photo's @p path in @p project (relative() back).
QString absolute(const QString &project, const QString &path);
/// The picture, turned as the camera said (EXIF orientation); null when unreadable.
QImage  load(const QString &path);
/// Moves the photo @p path to the trash, or deletes it where there is
/// none. False when it stays (@p error says why); @p trashed gets its path
/// in the trash, empty when it was deleted.
bool    remove(const QString &path, QString *trashed = nullptr, QString *error = nullptr);

/// A photo in a project: its full path and when it was taken.
struct Photo
{
  QString   path;
  QDateTime when;
};
/// The newest photo of @p project (in "photos", or in the project itself
/// when files are not sorted by kind), by the time in its name; an empty
/// path when there is none.
Photo   newest(const QString &project);
}
