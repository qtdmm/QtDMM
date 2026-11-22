// Copyright (c) 2026 The QtDMM developers
// SPDX-License-Identifier: GPL-3.0-or-later
#include "core/updatecheck.h"

#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonDocument>
#include <QJsonObject>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QProcess>
#include <QRegularExpression>

#include <array>
#include <filesystem>

#ifdef Q_OS_WIN
#include <qt_windows.h>
#include <shellapi.h>
#pragma comment(lib, "shell32.lib")
#endif

namespace
{
const char *const kFeedUrl = "https://qtdmm.github.io/QtDMM/latest.json";

/// The parts of a version in the order they compare: year, number, patch,
/// pre-release rank (dev < alpha < beta < rc < release), its number, the
/// commits after it. Empty for no version.
std::optional<std::array<int, 6>> versionParts(const QString &v)
{
  static const QRegularExpression re(
    R"(^(\d+)\.(\d+)(?:\.(\d+))?(?:-(dev|alpha|beta|rc)(\d*))?(?:\.(\d+))?(?:\+.*)?$)");
  const QRegularExpressionMatch m = re.match(v.trimmed());
  if (!m.hasMatch())
    return std::nullopt;
  static const QStringList ranks = { "dev", "alpha", "beta", "rc" };
  const QString pre = m.captured(4);
  return std::array<int, 6>{ m.captured(1).toInt(), m.captured(2).toInt(), m.captured(3).toInt(),
                             pre.isEmpty() ? int(ranks.size()) : int(ranks.indexOf(pre)),
                             m.captured(5).toInt(), m.captured(6).toInt() };
}

UpdateRelease releaseFrom(const QJsonObject &o)
{
  UpdateRelease r;
  r.version = o.value("version").toString();
  r.date = QDate::fromString(o.value("date").toString(), Qt::ISODate);
  r.notes = QUrl(o.value("notes").toString());
  const QJsonObject files = o.value("files").toObject();
  for (auto it = files.begin(); it != files.end(); ++it)
  {
    const QJsonObject f = it.value().toObject();
    r.files.insert(it.key(), { QUrl(f.value("url").toString()), f.value("sha256").toString(),
                               qint64(f.value("size").toDouble()) });
  }
  return r;
}
}   // namespace

UpdateCheck::UpdateCheck(QObject *parent) : QObject(parent) {}

bool UpdateCheck::enabled()
{
#ifdef QTDMM_NO_UPDATE_CHECK
  return false;
#else
  return true;
#endif
}

UpdateCheck::Install UpdateCheck::installKind()
{
  if (qEnvironmentVariableIsSet("APPIMAGE"))
    return Install::AppImage;
  if (QFile::exists("/.flatpak-info"))
    return Install::Flatpak;
  Install kind = Install::Source;
#if defined(Q_OS_WIN)
  // Inno Setup puts its uninstaller at the top of the installation, the
  // program is in bin/ below it
  const QDir dir(QCoreApplication::applicationDirPath());
  kind = dir.exists("unins000.exe") || dir.exists("../unins000.exe") ? Install::WindowsInstaller : Install::WindowsZip;
#elif defined(Q_OS_MACOS)
  if (QCoreApplication::applicationDirPath().endsWith(".app/Contents/MacOS"))
    kind = Install::MacBundle;
#endif
  return kind;
}

QString UpdateCheck::fileKind(Install kind)
{
  switch (kind)
  {
  case Install::AppImage: return "appimage";
  case Install::Flatpak: return "flatpak";
  case Install::WindowsInstaller: return "windows-installer";
  case Install::WindowsZip: return "windows-zip";
  case Install::MacBundle: return "macos";
  case Install::Source: break;
  }
  return {};
}

int UpdateCheck::compareVersions(const QString &a, const QString &b)
{
  const auto pa = versionParts(a), pb = versionParts(b);
  if (!pa || !pb)
    return int(bool(pa)) - int(bool(pb));
  return *pa < *pb ? -1 : (*pb < *pa ? 1 : 0);
}

bool UpdateCheck::isPreRelease(const QString &version)
{
  const auto p = versionParts(version);
  return p && (*p)[3] < 4;
}

std::optional<UpdateRelease> UpdateCheck::offer(const QByteArray &feed, const QString &current, bool beta,
                                                const QString &skipped)
{
  const QJsonObject root = QJsonDocument::fromJson(feed).object();
  if (root.value("format").toInt() != 1)
    return std::nullopt;
  std::optional<UpdateRelease> best;
  for (const char *key : { "stable", "beta" })
  {
    if (!beta && qstrcmp(key, "beta") == 0)
      continue;
    const QJsonValue v = root.value(key);
    if (!v.isObject())
      continue;
    UpdateRelease r = releaseFrom(v.toObject());
    if (compareVersions(r.version, current) <= 0 || r.version == skipped)
      continue;
    if (!best || compareVersions(r.version, best->version) > 0)
      best = std::move(r);
  }
  return best;
}

QStringList UpdateCheck::installerArguments()
{
  return { "/SILENT", "/SP-", "/SUPPRESSMSGBOXES", "/NORESTART", "/CLOSEAPPLICATIONS", "/QTDMMRESTART=1" };
}

bool UpdateCheck::runInstaller(const QString &path)
{
#ifdef Q_OS_WIN
  // ShellExecuteEx, not QProcess: the installer asks for administrator
  // rights (UAC), which CreateProcess refuses
  const std::wstring file = QDir::toNativeSeparators(path).toStdWString();
  const std::wstring args = installerArguments().join(' ').toStdWString();
  SHELLEXECUTEINFOW info = {};
  info.cbSize = sizeof(info);
  info.fMask = SEE_MASK_NOASYNC;
  info.lpVerb = L"open";
  info.lpFile = file.c_str();
  info.lpParameters = args.c_str();
  info.nShow = SW_SHOWNORMAL;
  return ShellExecuteExW(&info);
#else
  Q_UNUSED(path)
  return false;
#endif
}

QString UpdateCheck::replaceAppImage(const QString &downloaded, const QString &target)
{
  const QFileInfo old(target);
  if (!old.isFile())
    return tr("%1 is no file").arg(target);
  // next to it: the rename over it is then one step on the same file system
  const QString part = old.dir().filePath("." + old.fileName() + ".update");
  QFile::remove(part);
  if (!QFile::copy(downloaded, part))
    return tr("Cannot write into %1").arg(old.absolutePath());
  QFile::setPermissions(part, old.permissions() | QFile::ExeOwner | QFile::ReadOwner | QFile::WriteOwner);
  std::error_code error;
  std::filesystem::rename(QFile::encodeName(part).toStdString(), QFile::encodeName(target).toStdString(), error);
  if (error)
  {
    QFile::remove(part);
    return QString::fromStdString(error.message());
  }
  QFile::remove(downloaded);
  return {};
}

bool UpdateCheck::restartAppImage(const QString &path)
{
  QProcessEnvironment env = QProcessEnvironment::systemEnvironment();
  const QString folder = env.value("OWD");
  for (const char *name : { "APPIMAGE", "APPDIR", "ARGV0", "OWD" })
    env.remove(name);
  QProcess process;
  process.setProgram(path);
  process.setProcessEnvironment(env);
  if (!folder.isEmpty())
    process.setWorkingDirectory(folder);
  return process.startDetached();
}

QUrl UpdateCheck::feedUrl()
{
  const QString env = qEnvironmentVariable("QTDMM_UPDATE_URL");
  return env.isEmpty() ? QUrl(kFeedUrl) : QUrl::fromUserInput(env);
}

void UpdateCheck::check()
{
  if (!m_net)
    m_net = new QNetworkAccessManager(this);
  QNetworkRequest request(feedUrl());
  request.setHeader(QNetworkRequest::UserAgentHeader, QString("QtDMM/%1").arg(QCoreApplication::applicationVersion()));
  request.setTransferTimeout(15000);
  QNetworkReply *reply = m_net->get(request);
  connect(reply, &QNetworkReply::finished, this, [this, reply] {
    reply->deleteLater();
    if (reply->error() != QNetworkReply::NoError)
      Q_EMIT failed(reply->errorString());
    else
      Q_EMIT checked(reply->readAll());
  });
}
