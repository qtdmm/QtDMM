// Copyright (c) 2026 The QtDMM developers
// SPDX-License-Identifier: GPL-3.0-or-later
#include "service/photoserver.h"

#include "core/photostore.h"

#include <QFile>
#include <QTcpSocket>

PhotoServer::PhotoServer(QObject *parent) : HttpServer(kMaxClients, parent)
{
}

bool PhotoServer::start(const QHostAddress &address, quint16 port)
{
  // a new token each time: a link of before no longer works
  return HttpServer::start(address, port, newToken());
}

QByteArray PhotoServer::page()
{
  QFile file(":/photo/upload.html");
  if (!file.open(QIODevice::ReadOnly))
    return {};
  QByteArray html = file.readAll();
  // the words, as the program speaks; HTML-escaped
  const QList<std::pair<const char *, QString>> words = {
    // the language the words come in: the one of the translation loaded,
    // English without one (the system's language may have none)
    { "{{LANG}}", tr("en", "the language code (BCP 47) of this translation, for the phone's page") },
    { "{{TITLE}}", tr("Photo for QtDMM") },
    { "{{TAKE}}", tr("Take a photo") },
    { "{{GALLERY}}", tr("Choose from the gallery") },
    { "{{HINT}}", tr("Each photo goes straight into the open project in QtDMM, with a note in the graph. "
                     "Keep this page open for more.") },
    { "{{SENDING}}", tr("Sending...") },
    { "{{SENT}}", tr("Sent") },
    { "{{FAILED}}", tr("Not sent") },
    { "{{GONE}}", tr("QtDMM no longer takes photos: the dialog was closed. Open it again and scan the new code.") },
  };
  for (const auto &[key, text] : words)
    html.replace(key, text.toHtmlEscaped().toUtf8());
  return html;
}

QByteArray PhotoServer::forbidden() const
{
  return tr("This link no longer works. Scan the code in QtDMM again.").toUtf8();
}

QByteArray PhotoServer::tooLarge() const
{
  return tr("The photo is larger than %1 MB.").arg(kMaxBytes / (1024 * 1024)).toUtf8();
}

void PhotoServer::handle(QTcpSocket *socket, const Request &c)
{
  if (c.method == "GET" && c.path.isEmpty())
  {
    Q_EMIT visited();
    answer(socket, 200, "text/html; charset=utf-8", page());
  }
  else if (c.path == "upload")
  {
    if (c.method != "POST")
      answer(socket, 405, "text/plain", "POST a photo");
    else if (!c.type.startsWith("image/") || !PhotoStore::isPhoto(c.body))
      answer(socket, 415, "text/plain; charset=utf-8", tr("QtDMM cannot read this picture (try JPEG).").toUtf8());
    else
    {
      answer(socket, 200, "text/plain", "ok");
      Q_EMIT received(c.body);
    }
  }
  else
    answer(socket, 404, "text/plain", "Not found");
}
