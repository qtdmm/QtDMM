// Copyright (c) 2026 The QtDMM developers
// SPDX-License-Identifier: GPL-3.0-or-later
//
// QcpGraph: mouse, wheel and keys.
#include "ui/views/qcpgraph.h"

#include "recording/recordingstore.h"
#include "ui/views/qcp.h"

#include <QKeyEvent>
#include <QRubberBand>

void QcpGraph::setupInput()
{
  // the keys are the graph's: a click into the plot gives it the focus
  setFocusPolicy(Qt::StrongFocus);
  connect(m_plot, &QCustomPlot::mousePress, this, &QcpGraph::mousePressed);
  connect(m_plot, &QCustomPlot::mouseMove, this, &QcpGraph::mouseMoved);
  connect(m_plot, &QCustomPlot::mouseRelease, this, &QcpGraph::mouseReleased);
  connect(m_plot, &QCustomPlot::mouseWheel, this, &QcpGraph::wheelTurned);
}

// A drag with the left or middle button moves the time, with Shift and the
// left button it draws a frame to zoom on (as the graph on QtCharts). A
// click - no more than a slip of the mouse - sets the time cursor A, with
// Shift B; a press on a cursor's line drags that cursor, one on a reference
// line that line (a cursor goes before a line where they meet).
void QcpGraph::mousePressed(QMouseEvent *event)
{
  if (!m_store || !m_plot->axisRect()->rect().contains(event->pos()))
    return;
  const bool shift = event->modifiers() & Qt::ShiftModifier;
  if (const int cursor = cursorAt(event->pos()); event->button() == Qt::LeftButton && !shift && cursor >= 0)
  {
    m_drag = Drag::Cursor;
    m_dragCursor = cursor;
  }
  else if (const int level = levelAt(event->pos()); event->button() == Qt::LeftButton && !shift && level > -2)
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
    // a cursor's line and a reference line can be dragged
    m_plot->setCursor(cursorAt(event->pos()) >= 0 ? Qt::SplitHCursor : levelAt(event->pos()) > -2 ? Qt::SplitVCursor : Qt::ArrowCursor);
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
  else if (m_drag == Drag::Level)
    dragLevel(m_dragLevel, event->pos().y());
  else if (m_drag == Drag::Cursor)
    setTimeCursor(m_dragCursor, qRound64(m_plot->xAxis->pixelToCoord(event->pos().x()) * 1000));
}

void QcpGraph::mouseReleased(QMouseEvent *event)
{
  const Drag drag = m_drag;
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
    case Qt::Key_Escape:
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
