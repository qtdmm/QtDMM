// Copyright (c) 2026 The QtDMM developers
// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "ui/settings/settingspage.h"

class QCheckBox;
class QComboBox;
class QDoubleSpinBox;
class QLabel;
class QPushButton;
class QSpinBox;

/// Settings page "Web view": the picture of the window area in a browser,
/// only to look at (WebViewServer). Keys WebView/enabled, WebView/port,
/// WebView/all-interfaces, WebView/interval (ms) and WebView/token, the
/// link's secret: made once, kept until "New link".
class WebViewPrefs : public SettingsPage
{
  Q_OBJECT

public:
  WebViewPrefs(QWidget *parent = nullptr);

  bool enabled() const;
  int port() const;
  bool allInterfaces() const;
  int interval() const;   ///< ms
  /// The link's token; made when the view is switched on without one.
  QString token() const;
  /// The running link, or why not (from ServiceHost).
  void setLink(const QString &url, const QString &error);

public Q_SLOTS:
  void defaultsSLOT() override;
  void factoryDefaultsSLOT() override;
  void applySLOT() override;

private:
  void updateEnabled();

  QCheckBox *m_enabled;
  QSpinBox *m_port;
  QComboBox *m_bind;
  QDoubleSpinBox *m_interval;
  QPushButton *m_newLink;
  QLabel *m_link;
  QPushButton *m_copy;
  mutable QString m_token;
};
