// Copyright (c) 2026 The QtDMM developers
// SPDX-License-Identifier: GPL-3.0-or-later
//
// WorkspaceFile: version 2 (JSON) written and read back to the same window
// keys, devices and picture; version 1 (ini) still read; a file of a newer
// QtDMM or no workspace refused; the keys it does not know kept.
#include <QBuffer>
#include <QCoreApplication>
#include <QDebug>
#include <QFile>
#include <QImage>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSettings>
#include <QTemporaryDir>

#include "core/workspacefile.h"
#include "testutil.h"

int main(int argc, char **argv)
{
  QCoreApplication app(argc, argv);
  QTemporaryDir dir;
  auto path = [&dir](const char *name) { return dir.filePath(QString::fromLatin1(name)); };
  auto writeText = [](const QString &file, const QByteArray &text)
  {
    QFile f(file);
    return f.open(QIODevice::WriteOnly) && f.write(text) == text.size();
  };

  // 1. version 2: written, read back the same
  {
    WorkspaceFile ws;
    ws.name = "Akku-Test";
    ws.description = "Entladekurve\nmit Shunt";
    ws.width = 1400;
    ws.height = 900;
    ws.windowDevice = "b";
    MyDevice b;
    b.id = "b";
    b.name = "Sim B";
    b.keys = { { "DMM/model", "QtDMM Simulated meter" }, { "DMM/data-format", "12" } };
    b.alarms = R"([{"name":"High","condition":"Above","a":2}])";
    MyDevice a;
    a.id = "a";
    a.name = "Sim A";
    a.keys = { { "DMM/model", "QtDMM Simulated meter" }, { "Port settings/ble-key", "00112233445566778899aabbccddeeff" } };
    a.limitTest = R"({"enabled":true,"lower":1,"upper":2})";
    ws.devices = { b, a };
    ws.keys = { { "Windows/arrange", "left" }, { "Windows/layout", "V(0.5:display,0.5:graph)" },
                { "Windows/title-bars-hidden", true }, { "Display/show", true }, { "MainWindow/show-graph", false },
                { "Windows/graph-values", "0 1 0@device:a" }, { "Windows/lcd-secondary-values", "" },
                { "Windows/geometry-graph", "0 0 400 300" }, { "Windows/open-devices", "a" },
                { "Windows/panel-a-value", "0" }, { "Windows/panel-a-folded", false },
                { "Windows/geometry-panel-a", "10 10 200 100" }, { "Windows/future-key", "kept" } };
    ws.thumbnail = QByteArray("\x89PNG\r\n\x1a\nfake", 12);
    QString error;
    check(ws.write(path("v2.qtdmm-workspace"), &error), "v2 written: " + error);
    QFile f(path("v2.qtdmm-workspace"));
    const QJsonObject root = f.open(QIODevice::ReadOnly) ? QJsonDocument::fromJson(f.readAll()).object() : QJsonObject();
    check(root.value("version").toInt() == 2 && root.value("layout").toObject().value("arrange") == "left"
            && root.value("views").toObject().value("graph").toObject().value("values").toArray().size() == 3
            && root.value("devices").toArray().size() == 2
            && root.value("devices").toArray().at(1).toObject().value("panel").toObject().value("geometry") == "10 10 200 100"
            && root.value("devices").toArray().at(0).toObject().value("alarms").isArray()
            && root.value("other").toObject().value("Windows/future-key") == "kept",
          "v2: sorted into layout, views, devices and other");
    const std::optional<WorkspaceFile> back = WorkspaceFile::read(path("v2.qtdmm-workspace"), &error);
    check(back.has_value(), "v2 read: " + error);
    if (back)
    {
      check(back->name == ws.name && back->description == ws.description && back->width == 1400 && back->height == 900
              && back->windowDevice == "b" && back->thumbnail == ws.thumbnail && back->version == 2,
            "v2: name, description, size, window device, picture");
      check(back->keys.size() == ws.keys.size(), QString("v2: %1 keys back of %2").arg(back->keys.size()).arg(ws.keys.size()));
      for (auto it = ws.keys.cbegin(); it != ws.keys.cend(); ++it)
        check(back->keys.value(it.key()).toString() == it.value().toString(),
              QString("v2: %1 = %2, got %3").arg(it.key(), it.value().toString(), back->keys.value(it.key()).toString()));
      check(back->keys.contains("Windows/lcd-secondary-values") && back->keys.value("Windows/lcd-secondary-values").toString().isEmpty(),
            "v2: an empty list stays (none chosen), not missing (automatic)");
      // the rules as JSON (the order of an object's members is the file's)
      auto same = [](const QString &x, const QString &y) { return QJsonDocument::fromJson(x.toUtf8()) == QJsonDocument::fromJson(y.toUtf8()); };
      check(back->devices.size() == 2 && back->device("b") && back->device("b")->keys == b.keys
              && same(back->device("b")->alarms, b.alarms) && same(back->device("a")->limitTest, a.limitTest)
              && back->device("a")->alarms.isEmpty(),
            "v2: the copies of the devices with their rules");
      check(back->device("a") && !back->device("a")->keys.contains("Port settings/ble-key")
              && back->device("a")->keys.value("DMM/model") == "QtDMM Simulated meter",
            "v2: a device's key stays out of the file");
    }
  }

  // 2. version 1 (26.2 - 26.4 before U6): the ini's keys, the size
  {
    {
      QSettings ini(path("v1.qtdmm-workspace"), QSettings::IniFormat);
      ini.setValue("Workspace/version", 1);
      ini.setValue("Workspace/size", "1200 800");
      ini.setValue("Windows/arrange", "fixed");
      ini.setValue("Windows/open-devices", "a");
      ini.setValue("Display/show", true);
    }
    QString error;
    const std::optional<WorkspaceFile> v1 = WorkspaceFile::read(path("v1.qtdmm-workspace"), &error);
    check(v1 && v1->version == 1 && v1->width == 1200 && v1->height == 800 && v1->name == "v1"
            && v1->keys.value("Windows/arrange") == "fixed" && v1->keys.value("Windows/open-devices") == "a"
            && !v1->keys.contains("Workspace/version") && v1->windowDevice.isEmpty(),
          "v1: the keys, the size, the file's name: " + error);
  }

  // 3. refused: a newer QtDMM's, no workspace, broken JSON
  {
    QString error;
    check(writeText(path("new.qtdmm-workspace"), R"({"format":"qtdmm-workspace","version":5,"minReader":99})")
            && !WorkspaceFile::read(path("new.qtdmm-workspace"), &error) && error.contains("99"),
          "minReader 99: refused, says why: " + error);
    check(writeText(path("newer.qtdmm-workspace"), R"({"format":"qtdmm-workspace","version":3,"minReader":2,"name":"N"})")
            && WorkspaceFile::read(path("newer.qtdmm-workspace")).has_value(),
          "a newer version a reader of 2 understands: read");
    check(writeText(path("other.json"), R"({"name":"x"})") && !WorkspaceFile::read(path("other.json"), &error),
          "JSON without the format: no workspace: " + error);
    check(writeText(path("broken.qtdmm-workspace"), R"({"format":"qtdmm-workspace",)")
            && !WorkspaceFile::read(path("broken.qtdmm-workspace"), &error) && !error.isEmpty(),
          "broken JSON: refused: " + error);
    check(writeText(path("text.qtdmm-workspace"), "hello") && !WorkspaceFile::read(path("text.qtdmm-workspace"), &error),
          "no ini workspace either: " + error);
    check(!WorkspaceFile::read(path("missing.qtdmm-workspace"), &error), "a missing file: " + error);
  }

  // 4. oversized files and images are refused before they can consume
  //    unbounded memory when loaded or shown in a tooltip
  {
    QString error;
    check(writeText(path("large.qtdmm-workspace"), QByteArray(33 * 1024 * 1024, ' '))
            && !WorkspaceFile::read(path("large.qtdmm-workspace"), &error) && !error.isEmpty(),
          "oversized workspace file is refused: " + error);

    QImage image(5000, 1, QImage::Format_RGB32);
    QByteArray png;
    QBuffer buffer(&png);
    buffer.open(QIODevice::WriteOnly);
    image.save(&buffer, "PNG");
    const QJsonObject oversizedThumbnail{ { "format", "qtdmm-workspace" }, { "version", 2 }, { "minReader", 2 },
                                           { "thumbnail", QString::fromLatin1(png.toBase64()) } };
    check(writeText(path("large-thumbnail.qtdmm-workspace"),
                    QJsonDocument(oversizedThumbnail).toJson(QJsonDocument::Compact))
            && !WorkspaceFile::read(path("large-thumbnail.qtdmm-workspace"), &error)
            && error.contains("dimensions"),
          "oversized thumbnail dimensions are refused: " + error);
  }

  if (failed)
  {
    qWarning() << failed << "workspace file check(s) failed";
    return 1;
  }
  qInfo() << "All workspace file tests passed.";
  return 0;
}
