// Copyright (c) 2026 The QtDMM developers
// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <QBrush>
#include <QPointer>
#include <QColor>
#include <QVector>
#include <QWidget>

#include <cmath>

class RecordingStore;
struct RawPoint;

/// The overview strip below the graph: all the store keeps, the window the
/// graph shows between two brackets.
///
/// Each pixel column draws the minimum to the maximum of its readings, so
/// a spike stays in sight in a recording of hours. An overload is a band
/// along the top, a gap (the meter fell silent) is hatched, the marks are
/// dots on the bottom edge.
///
/// A drag inside the brackets (or a click beside them) moves the window, a
/// drag on a bracket makes it wider or narrower; brought to the right end
/// the window follows the newest reading again. The strip only asks
/// (moveRequested(), widthRequested(), endRequested()), the graph decides.
///
/// The columns are a power of two milliseconds wide, counted from the start
/// of the recording: a new reading changes its column only, and the columns
/// are counted anew only when the strip outgrows twice its width.
class GraphMinimap : public QWidget
{
  Q_OBJECT
public:
  /// The readings of one column.
  struct Column
  {
    double min = std::nan("");   ///< of the values in it; NaN when none
    double max = std::nan("");
    double last = std::nan("");  ///< the value that holds at its end; NaN = a gap
    bool   any = false;          ///< a reading falls into it
    bool   overload = false;     ///< one of them was an overload
    bool   gap = false;          ///< the meter fell silent in it
  };

  /// The strip's height in pixels.
  static constexpr int kHeight = 30;
  /// The strip's scale spans at least this part of the value: a steady value
  /// stays a line, whatever its last digit does.
  static constexpr double kLeastSpan = 0.02;
  /// The narrowest window a bracket drag asks for (ms).
  static constexpr qint64 kMinWidthMs = 10000;

  explicit GraphMinimap(QWidget *parent = nullptr);

  /// The store it draws (the graph's, or another meter's whose value is
  /// on the left axis); the same one again changes nothing.
  void             setStore(RecordingStore *store);
  /// The window the graph shows, in ms since the start of the recording.
  void             setView(qint64 start, qint64 width);
  qint64           viewStart() const { return m_viewStart; }
  qint64           viewWidth() const { return m_viewWidth; }
  /// Background and lettering of the graph's design; the brackets are
  /// drawn in the lettering colour.
  void             setColors(const QBrush &background, const QColor &lettering);
  void             setCurveColor(const QColor &curve);
  /// The value it draws (Reading::id): 0 the main value, else the store's
  /// extra() series of that value, one after the other.
  void             setValue(int id);
  int              value() const { return m_value; }

  /// @name What the strip spans and draws, public for the tests
  /// @{
  /// From what the store keeps to the newest reading, or to the end of
  /// the window when that lies further.
  qint64           from() const;
  qint64           to() const;
  qint64           columnMs() const { return m_columnMs; }
  /// The column that holds @p t, nullptr outside the strip.
  const Column    *column(qint64 t) const;
  int              columnCount() const { return int(m_columns.size()); }
  /// How often the columns were counted anew.
  int              rebuilds() const { return m_rebuilds; }
  double           xOf(qint64 t) const;
  qint64           timeAt(double x) const;
  /// @}

Q_SIGNALS:
  /// A drag or a click: the window should start at @p start.
  void             moveRequested(qint64 start);
  /// A bracket dragged: the window should be @p width long from @p start;
  /// @p atEnd when its right bracket is at the right end of the strip.
  void             widthRequested(qint64 start, qint64 width, bool atEnd);
  /// The window brought to the right end: it follows the newest reading.
  void             endRequested();

protected:
  void             paintEvent(QPaintEvent *) override;
  void             resizeEvent(QResizeEvent *) override;
  void             mousePressEvent(QMouseEvent *) override;
  void             mouseMoveEvent(QMouseEvent *) override;
  void             mouseReleaseEvent(QMouseEvent *) override;

private:
  enum Drag
  {
    NoDrag,
    Move,
    LeftBracket,
    RightBracket
  };

  QPointer<RecordingStore> m_store;   ///< another meter's may close while shown
  int              m_value = 0;         ///< setValue()
  QVector<Column>  m_columns;
  qint64           m_firstColumn = 0;   ///< the index of m_columns[0], counted from the start
  qint64           m_columnMs = 1;
  int              m_rebuilds = 0;
  qint64           m_viewStart = 0;
  qint64           m_viewWidth = 0;
  QBrush           m_background;
  QColor           m_lettering;
  QColor           m_curve;
  Drag             m_drag = NoDrag;
  qint64           m_grip = 0;          ///< Move: from the window's start to the mouse (ms)

  /// The column width the strip needs now: a power of two, at least as
  /// wide as the strip's span over its pixels.
  qint64           neededColumnMs() const;
  /// Counts the columns anew when the needed width changed.
  void             ensureColumns();
  void             rebuild();
  /// The newest reading went into the store.
  void             onAppended();
  /// A reading in extra(@p index): into its column when it is of value().
  void             onExtraAppended(int index);
  /// A reading into its column, the strip grown or counted anew as needed.
  void             addLast(const RawPoint &p);
  /// The columns before what the store keeps go.
  void             dropFront();
  /// The pixels within which the right bracket counts as at the end.
  qint64           endTolerance() const;
  Drag             dragAt(double x) const;
};
