/*
	Copyright (C) 2015 Frank Büttner frank@familie-büttner.de

	This program is free software: you can redistribute it and/or modify
	it under the terms of the GNU General Public License as published by
	the Free Software Foundation, either version 3 of the License, or
	(at your option) any later version.

	This program is distributed in the hope that it will be useful,
	but WITHOUT ANY WARRANTY; without even the implied warranty of
	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
	GNU General Public License for more details.

	You should have received a copy of the GNU General Public License
	along with this program.  If not, see <http://www.gnu.org/licenses/>
*/

#include "core/settings.h"

#include <QFileInfo>
#include <QCoreApplication>

Settings::Settings(QObject *parent) : QObject(parent)
{
  // Linux/macOS keep QSettings' native format (~/.config/QtDMM/QtDMM.conf).
  // On Windows the native format is the registry, which has no file name -
  // the first-start check, the per-instance config files and the instance
  // list all rely on one - so it gets an ini file (%APPDATA%\QtDMM\QtDMM.ini).
#ifdef Q_OS_WIN
  m_qsettings = new QSettings(QSettings::IniFormat, QSettings::UserScope,
                              QCoreApplication::organizationName(),
                              QCoreApplication::applicationName(), this);
#else
  m_qsettings = new QSettings(this);
#endif
  QFile file(m_qsettings->fileName());
  m_fileExists = file.exists();
  m_tmpConfig = new QHash<QString, QVariant>;
  m_filename = m_qsettings->fileName();
}

Settings::~Settings()
{
  delete m_tmpConfig;
}

Settings::Settings(const QString &instanceId, const QString &configPath, QObject *parent)
  : Settings(parent)
{
  QFileInfo fileInfo(m_qsettings->fileName());
  m_configPath = fileInfo.absolutePath();
  m_configBaseFileName = fileInfo.baseName();
  m_configFileNameSuffix = fileInfo.suffix();
  m_instanceId = instanceId.isEmpty() ? "default" : instanceId;

  if (!instanceId.isEmpty() || !configPath.isEmpty())
  {
    m_configPath = configPath.isEmpty() ? fileInfo.absolutePath() : configPath;
    m_configBaseFileName = fileInfo.baseName();
    m_configFileNameSuffix = fileInfo.suffix();

    QString instanceFileName = m_configBaseFileName+(instanceId.isEmpty() ? "" : "_"+instanceId)+"."+m_configFileNameSuffix;
    instanceFileName.prepend(m_configPath+"/");

    delete m_qsettings;
    m_qsettings = new QSettings(instanceFileName, QSettings::IniFormat, this);
    QFile file(m_qsettings->fileName());
    m_fileExists = file.exists();
    m_filename = m_qsettings->fileName();
  }
}

QStringList Settings::getConfigInstances()
{
  QDir dir(m_configPath);
  if (!dir.exists())
    return {};
  QString pattern = QString("%1*%2")
                      .arg(m_configBaseFileName)
                      .arg(m_configFileNameSuffix.isEmpty() ? "" : "." + m_configFileNameSuffix);

  QStringList result;
  result << m_instanceId;
  for (const QString &file : dir.entryList({ pattern }, QDir::Files, QDir::Name))
  {
    QString base = QFileInfo(file).completeBaseName();

    if (base.startsWith(m_configBaseFileName))
      base = base.mid(m_configBaseFileName.length());

    if (base.startsWith("_"))
      base.remove(0, 1);

    result << (base.isEmpty() ? "default" : base);
  }

  result.removeDuplicates();
  result.sort(Qt::CaseInsensitive);
  if (result.contains("default"))
  {
    result.removeAll("default");
    result.prepend("default");
  }

  return result;
}

int Settings::getInt(const QString &name, const int &def) const
{
  return m_qsettings->value(name, def).toInt();
}

void Settings::setInt(const QString &name, const int &value)
{
  m_tmpConfig->insert(name, value);
}

QColor Settings::getColor(const QString &name, const QColor &def) const
{
  QVariant value(m_qsettings->value(name));
  if (!value.isNull())
    return m_qsettings->value(name).value<QColor>();
  return def;
}

void Settings::setColor(const QString &name, const QColor &value)
{
  m_tmpConfig->insert(name, value);
}

QString Settings::getString(const QString &name, const QString &def) const
{
  return m_qsettings->value(name, def).toString();
}

void Settings::setString(const QString &name, const QString &value)
{
  m_tmpConfig->insert(name, value);
}

bool Settings::getBool(const QString &name, const bool &def) const
{
  return m_qsettings->value(name, def).toBool();
}

void Settings::setBool(const QString &name, const bool &value)
{
  m_tmpConfig->insert(name, value);
}

void Settings::save()
{
  for (auto parameter : m_tmpConfig->keys())
    m_qsettings->setValue(parameter, m_tmpConfig->value(parameter));
  m_tmpConfig->clear();
}

void Settings::clear()
{
  m_tmpConfig->clear();
}

void Settings::remove(const QString &name)
{
  m_tmpConfig->remove(name);
  m_qsettings->remove(name);
}

QStringList Settings::keysIn(const QString &group) const
{
  QStringList keys;
  m_qsettings->beginGroup(group);
  for (const QString &key : m_qsettings->childKeys())
    keys << group + '/' + key;
  m_qsettings->endGroup();
  return keys;
}

void Settings::sync()
{
  m_qsettings->sync();
}


QStringList Settings::customPorts() const
{
  QStringList ports;
  for (int i = 0; i < 10; i++)
  {
    const QString dev = getString(QString("Port settings/custom_device%1").arg(i)).trimmed();
    if (!dev.isEmpty())
      ports << dev;
  }
  return ports;
}

bool Settings::isMeterKey(const QString &key)
{
  // the meter belongs to the instance (and which of My devices it is,
  // DMM/my-device); the list of custom ports and the path of sigrok-cli are
  // the same for all of them
  return key.startsWith("DMM/")
         || (key.startsWith("Port settings/")
             && !key.startsWith("Port settings/custom_device")
             && key != "Port settings/sigrok_exe");
}

QString Settings::configDir() const
{
  return m_configPath.isEmpty() ? QFileInfo(m_filename).absolutePath() : m_configPath;
}

QVariantMap Settings::meterKeys(bool staged) const
{
  QVariantMap keys;
  for (const QString &key : m_qsettings->allKeys())
    if (isMeterKey(key))
      keys.insert(key, m_qsettings->value(key));
  if (staged)
    for (auto it = m_tmpConfig->cbegin(); it != m_tmpConfig->cend(); ++it)
      if (isMeterKey(it.key()))
        keys.insert(it.key(), it.value());
  return keys;
}

void Settings::setValues(const QVariantMap &keys)
{
  for (auto it = keys.cbegin(); it != keys.cend(); ++it)
    m_tmpConfig->insert(it.key(), it.value());
}

