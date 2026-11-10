// Copyright (c) 2026 The QtDMM developers
// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <functional>

#include <QDialog>
#include <QPointer>
#include <QTimer>
#include <QVariantMap>

class DeviceSettings;
class DeviceSummary;
class MeterConnection;
class QLabel;
class QPushButton;
class QTabWidget;
class QTextBrowser;

/// "Info and settings..." of an entry of My devices (also its double
/// click): the tab Info - the device at a glance, its state now, its
/// maker's website and its help page - the tab Data, its manual's
/// specifications where QtDMM has them (DeviceSpecs), and the tab Settings, the meter's
/// settings as on page 3 of the assistant. OK takes the settings, only
/// when they are complete. The caller gives settings() its ports first,
/// then load()s the keys.
class DeviceSettingsDlg : public QDialog
{
  Q_OBJECT
public:
  enum Tab { InfoTab, DataTab, SettingsTab };

  explicit DeviceSettingsDlg(const QString &name, QWidget *parent = nullptr);

  DeviceSettings *settings() const { return m_settings; }
  void        load(const QVariantMap &keys);
  /// The meter keys as DeviceSettings::keys() gives them.
  QVariantMap keys() const;
  void        showTab(Tab tab);
  Tab         currentTab() const;
  /// What the Info tab says of the device's state, rich text; asked at
  /// once and every second while the dialog is open.
  void        setStatus(const std::function<QString()> &status);
  /// The button Connect at the bottom left: @p connect connects the device,
  /// greyed while @p connected (asked every second as the state).
  void        setConnect(const std::function<bool()> &connected, const std::function<void()> &connect);
  /// The connection whose frames give the sample rate below the state,
  /// asked every second (nullptr while not connected). Counted only while
  /// the dialog is open.
  void        setFrameSource(const std::function<MeterConnection *()> &source);
  /// The sample rate of frames received at @p times (ms, oldest first):
  /// "2.0 Hz", below 1 Hz "0.50 Hz", "-" with fewer than two.
  static QString sampleRate(const QList<qint64> &times);
  QPushButton *connectButton() const { return m_connect; }
  /// The Info tab's text of the state and the links (for the tests).
  QString     statusText() const;
  QString     linksText() const;
  /// Whether the Data tab is there (the model has specifications).
  bool        hasData() const;

Q_SIGNALS:
  /// The link to the help: a page of QtDMM's help ("supported-devices.md").
  void        helpRequested(const QString &page);

private:
  void        updateInfo();
  void        updateStatus();

  QString         m_name;
  QTabWidget     *m_tabs = nullptr;
  QWidget        *m_info = nullptr;
  QTextBrowser   *m_data = nullptr;
  QString         m_dataModel;   ///< whose specifications m_data shows
  DeviceSummary  *m_summary = nullptr;
  QLabel         *m_status = nullptr;
  QLabel         *m_links = nullptr;
  DeviceSettings *m_settings = nullptr;
  QPushButton    *m_ok = nullptr;
  QPushButton    *m_connect = nullptr;
  std::function<bool()> m_connected;
  std::function<QString()> m_statusText;
  QTimer          m_tick;
  std::function<MeterConnection *()> m_frameSource;
  QPointer<MeterConnection> m_counted;   ///< whose frames m_frameTimes holds
  QList<qint64>   m_frameTimes;           ///< when they came, the last 10 s
};
