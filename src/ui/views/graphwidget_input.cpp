//======================================================================
// File:		graphwidget_input.cpp (from dmmgraph.cpp)
// Author:	Matthias Toussaint
// Created:	Tue Apr 10 17:45:35 CEST 2001
//----------------------------------------------------------------------
// This file is part of QtDMM.
//
// QtDMM is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
//
// QtDMM is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with QtDMM.  If not, see <http://www.gnu.org/licenses/>.
//----------------------------------------------------------------------
// Copyright (c) 2001 Matthias Toussaint
//======================================================================
//
// GraphWidget, part: mouse, wheel and keys on the chart, and the right-click menu.
// The class is in graphwidget.h, its other parts in graphwidget*.cpp.

#include <QtGui>
#include <QtWidgets>
#include <cmath>

#include "ui/views/graphwidget.h"
#include "ui/views/graphwidget_p.h"
#include "core/siprefix.h"
#include "ui/dialogs/notedlg.h"

// m_chartView is a child widget covering the whole graph area, so mouse/wheel
// events over it are delivered to its viewport, not to GraphWidget's own
// mousePressEvent/etc. overrides. An event filter on the viewport is the
// standard way to intercept them while keeping all interaction state/logic on
// GraphWidget itself (it already owns everything these handlers need).
bool GraphWidget::eventFilter(QObject *watched, QEvent *event)
{
  if (watched == m_chartView && event->type() == QEvent::KeyPress)
    return handleChartKey(static_cast<QKeyEvent *>(event)) || QWidget::eventFilter(watched, event);

  // the menu from the keyboard (Shift+F10, the menu key): where the mouse
  // is over the plot, else in its middle; the mouse opens it on its press
  if ((watched == m_chartView || watched == m_chartView->viewport()) && event->type() == QEvent::ContextMenu
      && static_cast<QContextMenuEvent *>(event)->reason() == QContextMenuEvent::Keyboard)
  {
    QWidget *vp = m_chartView->viewport();
    const QRect plot = m_chartView->mapFromScene(m_chart->plotArea()).boundingRect();
    QPoint pos = vp->mapFromGlobal(QCursor::pos());
    if (!plot.contains(pos))
      pos = plot.center();
    showMenu(pos, vp->mapToGlobal(pos));
    return true;
  }

  if (watched == m_chartView->viewport())
  {
    switch (event->type())
    {
      case QEvent::MouseButtonPress:
        handleChartMousePress(static_cast<QMouseEvent *>(event));
        return true;
      case QEvent::MouseButtonDblClick:
      {
        // a mark's flag: its title and text
        auto *ev = static_cast<QMouseEvent *>(event);
        const int mark = ev->button() == Qt::LeftButton ? markAt(ev->position().toPoint()) : -1;
        if (mark >= 0)
        {
          editMark(mark);
          return true;
        }
        handleChartMousePress(ev);
        return true;
      }
      case QEvent::MouseMove:
        handleChartMouseMove(static_cast<QMouseEvent *>(event));
        return true;
      case QEvent::MouseButtonRelease:
        handleChartMouseRelease(static_cast<QMouseEvent *>(event));
        return true;
      case QEvent::Wheel:
        handleChartWheel(static_cast<QWheelEvent *>(event));
        return true;
      case QEvent::Leave:
        hideCrosshair();
        break;
      default:
        break;
    }
  }

  return QWidget::eventFilter(watched, event);
}

void GraphWidget::handleChartMousePress(QMouseEvent *ev)
{
  QPoint pos(qRound(ev->position().x()), qRound(ev->position().y()));
  QPoint globalPos(qRound(ev->globalPosition().x()), qRound(ev->globalPosition().y()));

  const int mark = markAt(pos);
  if (ev->button() == Qt::LeftButton && mark >= 0)
  {
    // a click on a flag tells its note (on release: a tooltip goes with
    // the release), it sets no cursor
    m_mouseDown = false;
    m_clickCandidate = false;
    m_pressedMark = mark;
    m_pressPos = pos;   // a note dragged from here (dragMark())
    return;
  }
  if (ev->button() == Qt::RightButton)
  {
    m_mouseDown = false;
    m_clickCandidate = false;
    showMenu(pos, globalPos);
    return;
  }
  if (ev->button() == Qt::LeftButton)
  {
    m_mouseDown = true;
    m_mousePan = false;
    // not on a line: a click sets a time cursor (on release)
    m_pressPos = pos;
    m_pressShift = ev->modifiers() & Qt::ShiftModifier;
    m_clickCandidate = m_cursorMode == NoCursor;
  }
  else if (ev->button() == Qt::MiddleButton)
  {
    m_mouseDown = false;
    m_mousePan = true;
    m_mpos = pos;
  }
}

void GraphWidget::showMenu(const QPoint &pos, const QPoint &globalPos)
{
  m_popup->clear();
  const int mark = markAt(pos);
  if (mark >= 0)
  {
    QAction *edit = m_popup->addAction(tr("&Edit note..."));
    edit->setProperty("ID", IDEditMark);
    edit->setProperty("mark", mark);
    QAction *remove = m_popup->addAction(tr("&Remove mark"));
    remove->setProperty("ID", IDRemoveMark);
    remove->setProperty("mark", mark);
    m_popup->popup(globalPos);
    return;
  }
  // on an axis: its range (the left one's is in the settings)
  if (const int side = axisSideAt(pos); side >= 0)
  {
    if (side == 0)
    {
      QAction *a = m_popup->addAction(tr("Range of the left axis..."));
      a->setProperty("ID", IDConfigure);
    }
    else
    {
      // which one: the right axes can be close together
      m_popup->addSection(axisMenuTitle(side));
      addAxisEntries(m_popup, side);
    }
    m_popup->popup(globalPos);
    return;
  }
  // an entry of the menu; popupSLOT() acts on its "ID"
  auto item = [this](const QString &text, int id)
  {
    QAction *a = m_popup->addAction(text);
    a->setProperty("ID", id);
    return a;
  };

  if (!m_windowActions.isEmpty())
  {
    m_popup->addActions(m_windowActions);
    m_popup->addSeparator();
  }
  if (m_store->isRunning())
    item(tr("Stop recording"), IDStopRecorder);
  else
    item(tr("Start recording"), IDStartRecorder);
  if (m_store->state() == RecordingStore::View)
    item(tr("Live"), IDLive);
  item(tr("Reset &zoom"), IDResetZoom)->setEnabled(m_zoomed);
  item(tr("Clear graph"), IDClearGraph);
  m_popup->addSeparator();
  // a note where the menu was opened, or at the newest reading
  const QPointF scenePos = m_chartView->mapToScene(pos);
  if (m_chart->plotArea().contains(scenePos))
    item(tr("Note here..."), IDNoteHere)
      ->setProperty("t", qMax<qint64>(0, qRound64(m_chart->mapToValue(scenePos, m_dataSeries).x() * 1000)));
  item(tr("Note now..."), IDNoteNow)->setShortcut(QKeySequence(Qt::Key_N));
  // the time cursors where the menu was opened (a click sets A, Shift+click B)
  if (m_chart->plotArea().contains(scenePos))
  {
    const qint64 t = qRound64(m_chart->mapToValue(scenePos, m_dataSeries).x() * 1000);
    for (int which : { 0, 1 })
    {
      QAction *a = item(which == 0 ? tr("Cursor A here") : tr("Cursor B here"), IDCursorHere);
      a->setProperty("which", which);
      a->setProperty("t", t);
    }
  }
  m_popup->addSeparator();

  item(tr("Configure..."), IDConfigure);
  item(tr("Copy image"), IDCopyImage);
  item(tr("Export image..."), IDExportImage);
  if (m_printAction)
    m_popup->addAction(m_printAction);

  // this graph's colours: the default from the settings page, or one of
  // the variants for this graph only
  QMenu *colours = m_popup->addMenu(tr("Graph &colours"));
  auto variant = [this, colours](const QString &text, int v)
  {
    QAction *a = colours->addAction(text);
    a->setProperty("ID", IDColorVariant);
    a->setProperty("variant", v);
    a->setCheckable(true);
    a->setChecked(m_variantOverride == v);
  };
  variant(tr("&Default: %1").arg(variantTitle(m_defaultVariant)), -1);
  colours->addSeparator();
  for (ColorVariant v : { Neutral, ScopeBlue, PhosphorGreen, PhosphorAmber, ChartRecorder, Custom })
    variant(variantTitle(v), v);

  // where the reference line stands (it can also be dragged)
  // the right axes' ranges, also from the keyboard
  for (int side = 1; side <= 2; ++side)
    if (!m_right[size_t(side - 1)].unit.isEmpty())
      addAxisEntries(m_popup->addMenu(axisMenuTitle(side)), side);
  // what the graph shows besides its curves; the buttons beside the time
  // buttons switch the same
  QMenu *view = m_popup->addMenu(tr("&View"));
  auto toggle = [view](const QString &text, int id, bool on)
  {
    QAction *a = view->addAction(text);
    a->setProperty("ID", id);
    a->setCheckable(true);
    a->setChecked(on);
  };
  toggle(tr("&Statistics"), IDStats, m_statsOn);
  toggle(tr("Over&view"), IDMinimap, m_minimapOn);
  toggle(tr("Min/max &band"), IDEnvelope, m_envelopeOn);
  fillReferenceMenu(view->addMenu(tr("&Reference line")));
  view->addSeparator();
  toggle(tr("Bu&ttons"), IDViewButtons, m_viewButtonsOn);
  if (m_cursorT[0] >= 0 || m_cursorT[1] >= 0)
    item(tr("Remove time cursors"), IDClearCursors);

  if (!m_store->isRunning())
  {
    m_popup->addSeparator();
    item(tr("Import data..."), IDImportData);
  }

  m_popup->popup(globalPos);
}

void GraphWidget::handleChartMouseMove(QMouseEvent *ev)
{
  QPoint pos(qRound(ev->position().x()), qRound(ev->position().y()));

  if (m_mousePan)
  {
    QRectF plot = m_chart->plotArea();
    double range = m_xAxis->max() - m_xAxis->min();
    if (plot.width() <= 0 || range <= 0)
      return;

    double pixelsPerSecond = plot.width() / range;
    double dxSeconds = (m_mpos.x() - pos.x()) / pixelsPerSecond;
    double dxTenths = dxSeconds * 10;

    if (fabs(dxTenths) >= 1)
    {
      int sv = qMax(0, scrollbar->value());
      scrollbar->setValue(qBound(0, sv + qRound(dxTenths), scrollbar->maximum()));
      m_mpos = pos;
    }
    return;
  }

  if (m_pressedMark >= 0)
  {
    // a flag pressed on: a note follows the mouse along the time axis
    if (ev->buttons() & Qt::LeftButton)
      dragMark(pos);
    return;
  }

  if (m_clickCandidate && (pos - m_pressPos).manhattanLength() > 3)
  {
    m_clickCandidate = false;
    // Shift + drag: a frame to zoom on (without Shift a drag does what it did)
    m_zoomDragging = m_pressShift && (ev->buttons() & Qt::LeftButton)
                     && m_chart->plotArea().contains(m_chartView->mapToScene(m_pressPos));
  }
  if (m_zoomDragging)
  {
    const QRectF plot = m_chart->plotArea();
    const QPointF a = m_chartView->mapToScene(m_pressPos);
    QPointF b = m_chartView->mapToScene(pos);
    b.setX(qBound(plot.left(), b.x(), plot.right()));
    b.setY(qBound(plot.top(), b.y(), plot.bottom()));
    if (!m_zoomBand)
    {
      m_zoomBand = new QGraphicsRectItem;   // the scene owns it
      m_zoomBand->setZValue(100);
      m_chart->scene()->addItem(m_zoomBand);
    }
    const QColor c = palette().color(QPalette::Highlight);
    m_zoomBand->setPen(QPen(c, 1, Qt::DashLine));
    m_zoomBand->setBrush(QColor(c.red(), c.green(), c.blue(), 50));
    m_zoomBand->setRect(QRectF(a, b).normalized());
    m_zoomBand->show();
    return;
  }

  if (m_mouseDown && m_cursorMode != NoCursor)
  {
    QPointF scenePos = m_chartView->mapToScene(pos);
    double value = m_chart->mapToValue(scenePos, m_cursorMode == Integration ? m_intLine->first() : m_dataSeries).y();

    switch (m_cursorMode)
    {
      case Trigger:
        if (mode() == Raising) m_store->setRaisingThreshold(value);
        else                   m_store->setFallingThreshold(value);
        break;
      case Integration:
        m_store->setIntegrationThreshold(value);
        break;
      case Reference:
        dragReference(scenePos.y());
        return;
      case TimeCursorA:
      case TimeCursorB:
        setTimeCursor(m_cursorMode == TimeCursorB, qRound64(m_chart->mapToValue(scenePos, m_dataSeries).x() * 1000));
        return;
      case NoCursor:
        break;
    }

    updateThresholdLinePositions();
    Q_EMIT thresholdChanged(m_cursorMode, value);
    return;
  }

  // Pure hover: hit-test the draggable threshold lines (trigger, then
  // integration - first match wins when lines overlap) and,
  // failing that, drive the crosshair.
  if (!m_mouseDown && !m_mousePan)
  {
    QPointF scenePos = m_chartView->mapToScene(pos);
    const int tolerance = 3;

    auto near = [&](QGraphicsLineItem *item)
    {
      return item->isVisible() && fabs(scenePos.y() - item->line().y1()) < tolerance;
    };

    const double refY = referenceY();
    if (near(m_triggerLine))
      m_cursorMode = Trigger;
    else if (near(m_integrationLine))
      m_cursorMode = Integration;
    else if (std::isfinite(refY) && fabs(scenePos.y() - refY) < tolerance)
      m_cursorMode = Reference;
    else if (m_cursorLine[1]->isVisible() && fabs(scenePos.x() - m_cursorLine[1]->line().x1()) < tolerance)
      m_cursorMode = TimeCursorB;
    else if (m_cursorLine[0]->isVisible() && fabs(scenePos.x() - m_cursorLine[0]->line().x1()) < tolerance)
      m_cursorMode = TimeCursorA;
    else
      m_cursorMode = NoCursor;

    // over a note's flag: it can be dragged
    if (m_cursorMode == NoCursor && markDraggable(markAt(pos)))
    {
      m_chartView->viewport()->setCursor(Qt::SplitHCursor);
      hideCrosshair();
      return;
    }

    m_chartView->viewport()->setCursor(m_cursorMode == NoCursor ? Qt::ArrowCursor
                                       : m_cursorMode == TimeCursorA || m_cursorMode == TimeCursorB ? Qt::SplitHCursor
                                       : Qt::SplitVCursor);

    if (m_cursorMode != NoCursor || !m_crosshair)
    {
      hideCrosshair();
      return;
    }

    QRectF plot = m_chart->plotArea();
    double x = qBound(plot.left(), scenePos.x(), plot.right());

    m_crosshairVLine->setLine(x, plot.top(), x, plot.bottom());
    m_crosshairVLine->setVisible(true);

    double xValue = m_chart->mapToValue(QPointF(x, scenePos.y()), m_dataSeries).x();
    const qint64 t = qRound64(xValue * 1000);
    const RecordingSeries &series = m_store->series();
    // the reading whose value holds there; the next one if it is nearer
    int idx = series.holding(t);
    if (idx + 1 < series.count() && (idx < 0 || (!series.at(idx).gap() && !series.at(idx + 1).gap()
                                                 && series.at(idx + 1).t - t < t - series.at(idx).t)))
      idx++;
    const bool inside = idx >= 0 && idx < series.count() && t <= qMax(m_store->duration(), series.last().t);

    QString text = m_store->startDateTime().addMSecs(inside ? series.at(idx).t : t).time().toString("HH:mm:ss.zzz");

    if (inside && series.at(idx).gap())
    {
      // a gap: say why there is no value
      m_crosshairHLine->setVisible(false);
      const Quality q = series.at(idx).quality;
      text += "   " + (q == Quality::Overload ? QStringLiteral("OL") : q == Quality::Fault ? tr("fault") : tr("no value"));
    }
    else if (inside)
    {
      double val = series.at(idx).value;
      QPointF scenePoint = m_chart->mapToPosition(QPointF(series.at(idx).t / 1000.0, val), m_dataSeries);
      m_crosshairHLine->setLine(plot.left(), scenePoint.y(), plot.right(), scenePoint.y());
      m_crosshairHLine->setVisible(true);
      QString unit;
      text += "   " + QString("%1 %2").arg(formatEngineeringValue(val, &unit)).arg(unit);
    }
    else
      m_crosshairHLine->setVisible(false);

    // fixed above the plot, left-aligned so only its end moves with the text
    m_cursorLabel->setText(text);
    m_cursorLabel->adjustSize();
    const QPoint topLeft = m_chartView->mapFromScene(plot.topLeft());
    m_cursorLabel->move(topLeft.x() + 4, 3);
    m_cursorLabel->show();
    m_cursorLabel->raise();
  }
}

void GraphWidget::handleChartMouseRelease(QMouseEvent *ev)
{
  if (m_pressedMark >= 0 && ev->button() == Qt::LeftButton)
  {
    if (!m_markDragging && markAt(ev->position().toPoint()) == m_pressedMark)
      showMarkNote(m_pressedMark, ev->globalPosition().toPoint());
    endMarkDrag(false);
    return;
  }
  if (m_zoomDragging && ev->button() == Qt::LeftButton)
  {
    const QRectF frame = m_zoomBand ? m_zoomBand->rect() : QRectF();
    endZoomDrag();
    m_mouseDown = false;
    // a frame of a few pixels is a slip, no zoom
    if (frame.width() >= 5 && frame.height() >= 5)
      zoomToRect(frame);
    return;
  }
  if (m_clickCandidate && ev->button() == Qt::LeftButton)
  {
    const QPointF scenePos = m_chartView->mapToScene(m_pressPos);
    if (m_chart->plotArea().contains(scenePos))
    {
      const qint64 t = qRound64(m_chart->mapToValue(scenePos, m_dataSeries).x() * 1000);
      // Shift+click sets B; without A the first cursor is A
      setTimeCursor(m_pressShift && m_cursorT[0] >= 0 ? 1 : 0, t);
    }
  }
  m_clickCandidate = false;
  m_mouseDown = false;
  m_mousePan = false;
}

void GraphWidget::handleChartWheel(QWheelEvent *ev)
{
  leaveAll();
  if (ev->angleDelta().x() < 0 || ev->angleDelta().y() < 0)
    Q_EMIT zoomOut(1.1);
  else
    Q_EMIT zoomIn(1.1);
}

QString GraphWidget::formatEngineeringValue(double value, QString *unit) const
{
  QString prefix;
  const QString text = SiPrefix::format(value, m_store->unit(), &prefix);
  if (unit)
    *unit = prefix + m_store->unit();
  return text;
}

void GraphWidget::popupSLOT(QAction *action)
{
  switch (action->property("ID").toInt())
  {
    case IDConnect:
      Q_EMIT connectDMM(true);
      break;
    case IDDisconnect:
      Q_EMIT connectDMM(false);
      break;
    case IDStopRecorder:
      Q_EMIT recordRequested(false);
      break;
    case IDStartRecorder:
      Q_EMIT recordRequested(true);
      break;
    case IDLive:
      Q_EMIT liveRequested();
      break;
    case IDClearGraph:
      Q_EMIT clearRequested();
      break;
    case IDConfigure:
      Q_EMIT configure();
      break;
    case IDImportData:
      Q_EMIT importData();
      break;
    case IDCopyImage:
      copyImageSLOT();
      break;
    case IDExportImage:
      exportImageSLOT();
      break;
    case IDColorVariant:
      setColorVariant(m_defaultVariant, action->property("variant").toInt());
      Q_EMIT colorVariantChanged(m_variantOverride);
      break;
    case IDReference:
      setReference(ReferenceKind(action->property("kind").toInt()), action->property("value").toDouble());
      break;
    case IDClearCursors:
      clearTimeCursors();
      break;
    case IDMinimap:
      setMinimapVisible(action->isChecked());
      break;
    case IDEnvelope:
      setEnvelopeVisible(action->isChecked());
      break;
    case IDNoteHere:
      askNote(action->property("t").toLongLong());
      break;
    case IDNoteNow:
      askNote();
      break;
    case IDEditMark:
      editMark(action->property("mark").toInt());
      break;
    case IDRemoveMark:
      m_store->removeMark(action->property("mark").toInt());
      break;
    case IDCursorHere:
      setTimeCursor(action->property("which").toInt(), action->property("t").toLongLong());
      break;
    case IDResetZoom:
      endZoom(true);
      break;
    case IDStats:
      setStatsVisible(action->isChecked());
      break;
    case IDViewButtons:
      setViewButtonsVisible(action->isChecked());
      break;
    case IDAxisAuto:
      clearFixedRange(action->property("unit").toString());
      break;
    case IDAxisFixed:
      askFixedRange(action->property("unit").toString());
      break;
  }
}

QString GraphWidget::axisMenuTitle(int side) const
{
  const QString unit = SiPrefix::displayText(m_right[size_t(side - 1)].unit);
  return side == 1 ? tr("Right axis [%1]").arg(unit) : tr("Third axis [%1]").arg(unit);
}

void GraphWidget::fillReferenceMenu(QMenu *reference)
{
  reference->clear();
  auto refEntry = [this, reference](const QString &text, ReferenceKind kind, double value = 0)
  {
    QAction *a = reference->addAction(text);
    a->setProperty("ID", IDReference);
    a->setProperty("kind", int(kind));
    a->setProperty("value", value);
    a->setCheckable(true);
    a->setChecked(m_referenceKind == kind && (kind != RefAlarm || m_reference == value));
    return a;
  };
  auto valueText = [this](double v)
  {
    QString unit;
    const QString number = formatEngineeringValue(v, &unit);
    return QString("%1 %2").arg(number, unit).trimmed();
  };
  refEntry(tr("At &0"), RefZero);
  refEntry(tr("&Mean of the window"), RefMean);
  refEntry(tr("M&inimum of the window"), RefMin);
  refEntry(tr("Ma&ximum of the window"), RefMax);
  if (!m_alarmThresholds.isEmpty())
    reference->addSeparator();
  for (double a : m_alarmThresholds)
    refEntry(tr("Alarm threshold %1").arg(valueText(a)), RefAlarm, a);
  if (m_referenceKind == RefValue)
  {
    reference->addSeparator();
    refEntry(tr("At %1 (dragged)").arg(valueText(m_reference)), RefValue, m_reference)->setEnabled(false);
  }
  reference->addSeparator();
  refEntry(tr("&Off"), RefOff);
}

void GraphWidget::addAxisEntries(QMenu *menu, int side)
{
  const QString unit = m_right[size_t(side - 1)].unit;
  const bool fixed = hasFixedRange(unit);
  QAction *automatic = menu->addAction(tr("&Automatic"));
  automatic->setProperty("ID", IDAxisAuto);
  automatic->setProperty("unit", unit);
  automatic->setCheckable(true);
  automatic->setChecked(!fixed);
  QAction *range = menu->addAction(fixed ? tr("&Fixed: %1 .. %2 %3...")
                                             .arg(QLocale().toString(m_fixedRanges.value(unit).first),
                                                  QLocale().toString(m_fixedRanges.value(unit).second),
                                                  SiPrefix::displayText(unit))
                                         : tr("&Fixed range..."));
  range->setProperty("ID", IDAxisFixed);
  range->setProperty("unit", unit);
  range->setCheckable(true);
  range->setChecked(fixed);
}

void GraphWidget::askFixedRange(const QString &unit)
{
  // the range kept, else the one shown now
  std::pair<double, double> r = m_fixedRanges.value(unit, { 0.0, 1.0 });
  if (!hasFixedRange(unit))
    for (const SideAxis &a : m_right)
      if (a.unit == unit)
        r = { a.axis->min(), a.axis->max() };
  QDialog dlg(this);
  dlg.setWindowTitle(tr("Fixed range [%1]").arg(SiPrefix::displayText(unit)));
  auto *form = new QFormLayout(&dlg);
  auto spin = [&](double v)
  {
    auto *s = new QDoubleSpinBox(&dlg);
    s->setDecimals(6);
    s->setRange(-1e12, 1e12);
    s->setSuffix(" " + SiPrefix::displayText(unit));
    s->setValue(v);
    return s;
  };
  QDoubleSpinBox *min = spin(r.first);
  QDoubleSpinBox *max = spin(r.second);
  min->setObjectName("ui_axisMin");
  max->setObjectName("ui_axisMax");
  form->addRow(tr("M&inimum:"), min);
  form->addRow(tr("M&aximum:"), max);
  auto *buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, &dlg);
  connect(buttons, &QDialogButtonBox::accepted, &dlg, &QDialog::accept);
  connect(buttons, &QDialogButtonBox::rejected, &dlg, &QDialog::reject);
  form->addRow(buttons);
  auto check = [=] { buttons->button(QDialogButtonBox::Ok)->setEnabled(max->value() > min->value()); };
  connect(min, &QDoubleSpinBox::valueChanged, &dlg, check);
  connect(max, &QDoubleSpinBox::valueChanged, &dlg, check);
  check();
  if (dlg.exec() == QDialog::Accepted)
    setFixedRange(unit, min->value(), max->value());
}

void GraphWidget::endZoomDrag()
{
  m_zoomDragging = false;
  if (m_zoomBand)
    m_zoomBand->hide();
}

void GraphWidget::zoomToRect(const QRectF &frame)
{
  const QRectF plot = m_chart->plotArea();
  if (plot.height() <= 0)
    return;
  if (!m_zoomed)
  {
    m_zoomBefore.start = windowStart();
    m_zoomBefore.seconds = m_windowSeconds;
    m_zoomBefore.all = m_followAll;
    m_zoomBefore.atEnd = m_store->state() == RecordingStore::Live ? !m_liveHold : following();
  }
  // each y axis: the frame's height in its own scale
  for (int side = 0; side < 3; ++side)
  {
    const QValueAxis *axis = yAxisOf(side);
    const double span = axis->max() - axis->min();
    m_yZoom[size_t(side)] = { axis->min() + (plot.bottom() - frame.bottom()) / plot.height() * span,
                              axis->min() + (plot.bottom() - frame.top()) / plot.height() * span };
  }
  m_zoomed = true;
  setYRange(m_scaleMin, m_scaleMax);
  updateRightAxis();
  const double t0 = m_chart->mapToValue(frame.topLeft(), m_dataSeries).x();
  const double t1 = m_chart->mapToValue(frame.bottomRight(), m_dataSeries).x();
  showSpan(qRound64(t0 * 1000), qRound64((t1 - t0) * 1000), false);
  updateThresholdLinePositions();
}

void GraphWidget::endZoom(bool restoreWindow)
{
  if (!m_zoomed)
    return;
  m_zoomed = false;
  setYRange(m_scaleMin, m_scaleMax);
  updateRightAxis();
  updateThresholdLinePositions();
  if (!restoreWindow)
    return;
  if (m_zoomBefore.all)
    requestAll(true);
  else
    showSpan(m_zoomBefore.start, qint64(m_zoomBefore.seconds) * 1000, m_zoomBefore.atEnd);
}

bool GraphWidget::handleChartKey(QKeyEvent *ev)
{
  const bool shift = ev->modifiers() & Qt::ShiftModifier;

  if (m_markDragging && ev->key() == Qt::Key_Escape)
  {
    endMarkDrag(true);
    return true;
  }
  if (m_zoomDragging && ev->key() == Qt::Key_Escape)
  {
    endZoomDrag();
    return true;
  }

  switch (ev->key())
  {
    case Qt::Key_Plus:
    case Qt::Key_Equal:
      zoomInSLOT();
      return true;
    case Qt::Key_Minus:
      zoomOutSLOT();
      return true;
    case Qt::Key_0:
      zoomFitSLOT();
      return true;
    case Qt::Key_Left:
      pan(shift ? -0.5 : -0.1);
      return true;
    case Qt::Key_Right:
      pan(shift ? 0.5 : 0.1);
      return true;
    case Qt::Key_PageUp:
      pan(-1.0);
      return true;
    case Qt::Key_PageDown:
      pan(1.0);
      return true;
    case Qt::Key_Home:
      scrollToStart();
      return true;
    case Qt::Key_End:
      scrollToEnd();
      return true;
    case Qt::Key_N:
      if (ev->modifiers() & (Qt::ControlModifier | Qt::AltModifier | Qt::MetaModifier))
        return false;
      askNote();
      return true;
    case Qt::Key_Escape:
      if (m_cursorT[0] < 0 && m_cursorT[1] < 0)
        return false;
      clearTimeCursors();
      return true;
    default:
      return false;
  }
}

void GraphWidget::pan(double fraction)
{
  int step = qMax(1, qRound(m_windowSeconds * 10 * fabs(fraction)));
  if (fraction < 0)
    step = -step;
  scrollbar->setValue(qBound(0, scrollbar->value() + step, scrollbar->maximum()));
}

void GraphWidget::scrollToStart()
{
  scrollbar->setValue(0);
}

void GraphWidget::scrollToEnd()
{
  scrollbar->setValue(scrollbar->maximum());
}
