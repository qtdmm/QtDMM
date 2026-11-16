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
// left button it draws a frame to zoom on (as the graph on QtCharts).
void QcpGraph::mousePressed(QMouseEvent *event)
{
  if (!m_store || !m_plot->axisRect()->rect().contains(event->pos()))
    return;
  if (event->button() == Qt::LeftButton && (event->modifiers() & Qt::ShiftModifier))
    m_drag = Drag::Frame;
  else if (event->button() == Qt::LeftButton || event->button() == Qt::MiddleButton)
    m_drag = Drag::Pan;
  else
    return;
  m_pressPos = event->pos();
  m_pressStart = windowStart();
}

void QcpGraph::mouseMoved(QMouseEvent *event)
{
  if (m_drag == Drag::Pan)
  {
    const double width = m_plot->axisRect()->width();
    if (width <= 0 || (event->pos() - m_pressPos).manhattanLength() < 3)
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
}

void QcpGraph::mouseReleased(QMouseEvent *event)
{
  const Drag drag = m_drag;
  m_drag = Drag::None;
  if (drag != Drag::Frame)
    return;
  if (m_zoomBand)
    m_zoomBand->hide();
  const QRect frame = QRect(m_pressPos, event->pos()).normalized();
  // a frame of a few pixels is a slip, no zoom
  if (frame.width() >= 5 && frame.height() >= 5)
    zoomToRect(frame);
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
      break;
    default:
      break;
  }
  QcpPlotBase::keyPressEvent(event);
}
