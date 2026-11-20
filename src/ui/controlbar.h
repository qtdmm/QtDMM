// Copyright (c) 2026 The QtDMM developers
// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <QList>
#include <QWidget>

#include "device/dmmdecoder.h"
#include "core/reading.h"   // complete for moc: a slot takes it (GCC 16 -Wsfinae-incomplete)

class QToolButton;

/// The meter's keys under the display, for meters QtDMM can remote-control
/// (the UNI-T iDMM protocol: UT60BT, UT61B+/D+/E+, UT161, UT202BT; the iENV
/// meters; the ZOTEK Bluetooth meters). Groups
/// from left to right as on the meter's front - function, range, display,
/// LIGHT and HOLD on the right - that wrap to a second row when the window
/// is narrow; a group without a key of the meter is hidden.
///
/// HOLD, AUTO, REL, MIN/MAX, PEAK, MAX, MIN and AVG show the meter's state as
/// the readings report it, not the last click; RANGE, SELECT, Hz/%, UNIT,
/// °C/°F, FAST/SLOW and LIGHT are plain keys. MIN/MAX and PEAK are left as on the meter: by holding the key
/// (keyPressed("minmax_off")). The UT202BT's A~ and NCV/PEAK keys held send
/// another key (inrush, peak capture: "fn_a_long", "fn_ncv_long"). The keys are enabled while the meter is
/// connected; setProtocol() hides those the meter does not have (PEAK on
/// the UT60BT). MainWindow sends them (MeterController::pressKey()).
class ControlBar : public QWidget
{
  Q_OBJECT
public:
  explicit ControlBar(QWidget *parent = nullptr);

  /// A meter with keys QtDMM can press.
  static bool supported(const DmmDecoder::DMMInfo &info);

  void setConnected(bool connected);
  /// Shows the keys the meter @p model has, read by the decoder of @p format
  /// (DmmDecoder::hasKey()).
  void setProtocol(FrameFormat::DataFormat format, const QString &model);
  /// A key in a colour of its own (a style sheet): darker while pressed or
  /// on, faded while off. HOLD is blue, as on the meters (and a node's HOLD).
  static QString keyColour(const QColor &face, const QColor &text);
  static QColor  holdColour() { return QColor(0x3c, 0x9a, 0xff); }
  /// Holding MIN/MAX or PEAK this long leaves it (ms).
  static constexpr int kLongPressMs = 700;

  QSize sizeHint() const override;
  QSize minimumSizeHint() const override;
  bool hasHeightForWidth() const override { return true; }
  int heightForWidth(int width) const override;

public Q_SLOTS:
  /// The key states the reading reports (HOLD, AUTO); a key the meter
  /// ignores in this mode greyed out (DmmDecoder::keyWorks()).
  void showReading(const Reading &reading);

Q_SIGNALS:
  /// One of DmmDecoder::keyNames() (DmmDecoder::keyRequest()).
  void keyPressed(const QString &key);

protected:
  void resizeEvent(QResizeEvent *event) override;
  void showEvent(QShowEvent *event) override;

private:
  QList<QWidget *> m_groups;
  QList<QToolButton *> m_keys;
  QToolButton *m_hold = nullptr;
  QToolButton *m_auto = nullptr;
  QToolButton *m_rel = nullptr;
  QToolButton *m_minmax = nullptr;
  QToolButton *m_peak = nullptr;
  QToolButton *m_max = nullptr;
  QToolButton *m_min = nullptr;
  QToolButton *m_avg = nullptr;
  std::shared_ptr<DmmDecoder> m_decoder;   ///< setProtocol()
  QString     m_model;
  bool        m_connected = false;
  /// Places the groups in rows for @p width; returns the height used.
  int layoutGroups(int width, bool apply) const;
  /// layoutGroups() for the width now, the height to fit.
  void relayout();
};

/// The handle under a front panel's keys that hides and shows them
/// (ControlBar, NodeBar): a small tab at the panel's lower edge, a double
/// chevron up while open, down while folded; muted until hovered.
class FoldButton : public QWidget
{
  Q_OBJECT
public:
  explicit FoldButton(QWidget *parent);
  void setFolded(bool folded);
  bool isFolded() const { return m_folded; }

Q_SIGNALS:
  void toggled(bool folded);

protected:
  void paintEvent(QPaintEvent *) override;
  void enterEvent(QEnterEvent *) override;
  void leaveEvent(QEvent *) override;
  void mousePressEvent(QMouseEvent *) override;

private:
  bool m_folded = false;
  bool m_hover = false;
};

/// The gear on a front panel's name plate (LcdWidget::plateBand()), drawn
/// like its lettering: the model's colour, the manufacturer's when hovered.
/// A click: clicked().
class PanelGear : public QWidget
{
  Q_OBJECT
public:
  explicit PanelGear(QWidget *parent);
  /// The colours: at rest and hovered.
  void setInk(const QColor &rest, const QColor &hover);

Q_SIGNALS:
  void clicked();

protected:
  void paintEvent(QPaintEvent *) override;
  void enterEvent(QEnterEvent *) override;
  void leaveEvent(QEvent *) override;
  void mouseReleaseEvent(QMouseEvent *) override;

private:
  QColor m_rest = Qt::gray;
  QColor m_hoverInk = Qt::white;
  bool m_hover = false;
};
