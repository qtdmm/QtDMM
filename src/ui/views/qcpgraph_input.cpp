// Copyright (c) 2026 The QtDMM developers
// SPDX-License-Identifier: GPL-3.0-or-later
//
// QcpGraph: mouse, wheel and keys.
#include "ui/views/qcpgraph.h"

#include "recording/recordingstore.h"
#include "ui/views/qcp.h"

#include <QKeyEvent>
#include <QRubberBand>
#include <QToolTip>

void QcpGraph::setupInput()
{
  // the keys are the graph's: a click into the plot gives it the focus
  setFocusPolicy(Qt::StrongFocus);
  connect(m_plot, &QCustomPlot::mousePress, this, &QcpGraph::mousePressed);
  connect(m_plot, &QCustomPlot::mouseMove, this, &QcpGraph::mouseMoved);
  connect(m_plot, &QCustomPlot::mouseRelease, this, &QcpGraph::mouseReleased);
  connect(m_plot, &QCustomPlot::mouseDoubleClick, this, &QcpGraph::mouseDoubleClicked);
  connect(m_plot, &QCustomPlot::mouseWheel, this, &QcpGraph::wheelTurned);
}

// A drag with the left or middle button moves the time, with Shift and the
// left button it draws a frame to zoom on (as the graph on QtCharts). A
// click - no more than a slip of the mouse - sets the time cursor A, with
// Shift B; a press on a cursor's line drags that cursor, one on a reference
// line that line (a cursor goes before a line where they meet). A click on
// a mark's flag tells its note; a note is dragged by its flag or its line.
void QcpGraph::mousePressed(QMouseEvent *event)
{
  if (!m_store)
    return;
  const bool shift = event->modifiers() & Qt::ShiftModifier;
  const bool inPlot = m_plot->axisRect()->rect().contains(event->pos());
  const int cursor = inPlot ? cursorAt(event->pos()) : -1;
  // the flags lie below the plot
  if (const int mark = markAt(event->pos()); event->button() == Qt::LeftButton && !shift && cursor < 0 && mark >= 0)
  {
    m_drag = Drag::Mark;
    m_dragStore = m_marks.at(mark).store;
    m_dragIndex = m_marks.at(mark).index;
    m_dragFrom = m_dragT = m_marks.at(mark).t - m_marks.at(mark).shift;
    m_dragName = m_marks.at(mark).name;
    m_moved = false;
    m_pressPos = event->pos();
    return;
  }
  if (!inPlot)
    return;
  if (event->button() == Qt::LeftButton && !shift && cursor >= 0)
  {
    m_drag = Drag::Cursor;
    m_dragCursor = cursor;
  }
  else if (const int level = levelAt(event->pos()); event->button() == Qt::LeftButton && !shift && level != kNoLine)
  {
    m_drag = Drag::Level;
    m_dragLevel = level;
  }
  else if (event->button() == Qt::LeftButton && shift)
    m_drag = Drag::Frame;
  else if (event->button() == Qt::LeftButton || event->button() == Qt::MiddleButton)
    m_drag = Drag::Pan;
  else
    return;
  m_moved = false;
  m_pressPos = event->pos();
  m_pressStart = windowStart();
}

void QcpGraph::mouseMoved(QMouseEvent *event)
{
  if (m_drag == Drag::None)
  {
    // a cursor's line, a note and a reference line can be dragged; a flag tells its note
    const int mark = markAt(event->pos());
    const int flag = mark >= 0 && markFlagRect(mark).contains(event->pos()) ? mark : -1;
    m_plot->setCursor(flag >= 0 ? Qt::PointingHandCursor : cursorAt(event->pos()) >= 0 || mark >= 0 ? Qt::SplitHCursor
                      : levelAt(event->pos()) != kNoLine ? Qt::SplitVCursor : Qt::ArrowCursor);
    // outside the plot the crosshair's clock time is no longer true: a flag's note, or nothing
    if (!m_plot->axisRect()->rect().contains(event->pos()))
      m_plot->setToolTip(flag >= 0 ? markTip(flag) : QString());
    if (flag != m_tipMark)
    {
      m_tipMark = flag;
      if (flag >= 0)
        QToolTip::showText(event->globalPosition().toPoint(), markTip(flag), m_plot);
      else
        QToolTip::hideText();
    }
    return;
  }
  if ((event->pos() - m_pressPos).manhattanLength() >= 3)
    m_moved = true;
  if (!m_moved)
    return;
  if (m_drag == Drag::Pan)
  {
    const double width = m_plot->axisRect()->width();
    if (width <= 0)
      return;
    const double secondsPerPixel = m_windowSeconds / width;
    showFrom(m_pressStart + qRound64((m_pressPos.x() - event->pos().x()) * secondsPerPixel * 1000));
  }
  else if (m_drag == Drag::Frame)
  {
    if (!m_zoomBand)
      m_zoomBand = new QRubberBand(QRubberBand::Rectangle, m_plot);
    m_zoomBand->setGeometry(QRect(m_pressPos, event->pos()).normalized().intersected(m_plot->axisRect()->rect()));
    m_zoomBand->show();
  }
  else if (m_drag == Drag::Mark)
  {
    // alarm and trigger marks tell when something happened: they stay
    const int mark = markOf(m_dragStore, draggedIndex());
    if (!markIsNote(mark) || !m_marksEditable)
      return;
    QToolTip::hideText();
    const int moved = moveMark(mark, qRound64(m_plot->xAxis->pixelToCoord(event->pos().x()) * 1000));
    if (moved >= 0)
    {
      m_dragIndex = m_marks.at(moved).index;
      m_dragT = m_marks.at(moved).t - m_marks.at(moved).shift;
    }
  }
  else if (m_drag == Drag::Level)
    dragLevel(m_dragLevel, event->pos().y());
  else if (m_drag == Drag::Cursor)
    setTimeCursor(m_dragCursor, qRound64(m_plot->xAxis->pixelToCoord(event->pos().x()) * 1000));
}

void QcpGraph::mouseReleased(QMouseEvent *event)
{
  const Drag drag = m_drag;
  if (drag == Drag::Mark)
  {
    // a click: its note (a tooltip goes with the press)
    if (const int mark = markOf(m_dragStore, draggedIndex()); !m_moved && mark >= 0)
      QToolTip::showText(event->globalPosition().toPoint(), markTip(mark), m_plot);
    endMarkDrag(false);
    return;
  }
  m_drag = Drag::None;
  if (m_zoomBand)
    m_zoomBand->hide();
  const qint64 pressed = qRound64(m_plot->xAxis->pixelToCoord(m_pressPos.x()) * 1000);
  if (drag == Drag::Frame)
  {
    const QRect frame = QRect(m_pressPos, event->pos()).normalized();
    // a frame of a few pixels is a slip, no zoom; none at all is a click: B, without A the first cursor is A
    if (frame.width() >= 5 && frame.height() >= 5)
      zoomToRect(frame);
    else if (!m_moved)
      setTimeCursor(m_cursorT[0] >= 0 ? 1 : 0, pressed);
  }
  else if (drag == Drag::Pan && !m_moved && event->button() == Qt::LeftButton)
    setTimeCursor(0, pressed);
}

// A mark's flag: its title and text
void QcpGraph::mouseDoubleClicked(QMouseEvent *event)
{
  if (const int mark = markAt(event->pos()); event->button() == Qt::LeftButton && mark >= 0)
    editMark(mark);
}

void QcpGraph::wheelTurned(QWheelEvent *event)
{
  const bool out = event->angleDelta().x() < 0 || event->angleDelta().y() < 0;
  zoomBy(out ? 1 / 1.1 : 1.1);
}

void QcpGraph::pan(double fraction)
{
  showFrom(windowStart() + qRound64(m_windowSeconds * 1000.0 * fraction));
}

void QcpGraph::keyPressEvent(QKeyEvent *event)
{
  const bool shift = event->modifiers() & Qt::ShiftModifier;
  switch (event->key())
  {
    case Qt::Key_Plus:
    case Qt::Key_Equal:
      zoomBy(1.25);
      return;
    case Qt::Key_Minus:
      zoomBy(1 / 1.25);
      return;
    case Qt::Key_0:
      zoomFit();
      return;
    case Qt::Key_Left:
      pan(shift ? -0.5 : -0.1);
      return;
    case Qt::Key_Right:
      pan(shift ? 0.5 : 0.1);
      return;
    case Qt::Key_PageUp:
      pan(-1.0);
      return;
    case Qt::Key_PageDown:
      pan(1.0);
      return;
    case Qt::Key_Home:
      if (m_store)
        showFrom(m_store->origin());
      return;
    case Qt::Key_End:
      followEnd();
      return;
    case Qt::Key_N:
      // the plain key: with Alt it is a menu's letter
      if (event->modifiers() != Qt::NoModifier)
        break;
      askNote();
      return;
    case Qt::Key_Escape:
      if (m_drag == Drag::Mark)
      {
        endMarkDrag(true);
        return;
      }
      if (m_drag == Drag::Frame)
      {
        m_drag = Drag::None;
        if (m_zoomBand)
          m_zoomBand->hide();
        return;
      }
      if (m_zoomed)
      {
        endZoom(true);
        return;
      }
      if (m_cursorT[0] >= 0 || m_cursorT[1] >= 0)
      {
        clearTimeCursors();
        return;
      }
      break;
    default:
      break;
  }
  QcpPlotBase::keyPressEvent(event);
}
