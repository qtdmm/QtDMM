// Copyright (c) 2026 The QtDMM developers
// SPDX-License-Identifier: GPL-3.0-or-later
//
// GraphWidget: the marks of the recordings shown - alarms, triggers, notes -
// as lines with flags below the plot, and the notes one sets, edits and drags.
#include "ui/views/graphwidget.h"

#include "recording/recordingstore.h"
#include "ui/dialogs/notedlg.h"
#include "ui/views/graphflags.h"
#include "ui/views/graphtimebar.h"
#include "ui/views/graphtext.h"   // durationText()
#include "ui/views/qcp.h"

#include <QCoreApplication>
#include <QMenu>
#include <QPainterPath>
#include <QToolTip>
#include <algorithm>

namespace
{
// A word of the context GraphWidget looked up through a variable;
// graphwords.h keeps it for lupdate.
QString word(const char *text)
{
  return QCoreApplication::translate("GraphWidget", text);
}

// "14:03:12 (0:42 into the recording)"
QString markWhen(const RecordingStore *store, qint64 t)
{
  return word("%1 (%2 into the recording)").arg(store->startDateTime().addMSecs(t).toString("HH:mm:ss"), durationText(t / 1000));
}

/// Vertical lines at times, each with a flag in the row of the time labels
/// (GraphFlags): the marks and the letters of the time cursors. The flags
/// are placed where the time axis is when they are drawn or asked for.
class FlagLines : public QCPAbstractItem
{
public:
  struct Entry
  {
    int     id = 0;        ///< the mark's place in the marks; -1, -2: the cursors A, B
    double  x = 0;         ///< time axis
    QString label;
    QString note;
    QColor  color;
    QPen    pen;           ///< its line; Qt::NoPen: none (a cursor has its own)
    bool    photo = false; ///< a note with a photo
  };
  struct Placed
  {
    int     id = 0;
    double  x = 0;         ///< pixels
    QRectF  rect;          ///< the tag; empty: none
    QString text;
    QList<int> group;      ///< the marks on this flag, id first; several where their lines fall together
  };

  /// Marks this close (pixels) share one flag: closer than a click on the
  /// lines tells them apart (markAt() takes a line within 3 px).
  static constexpr double kTogether = 3;

  explicit FlagLines(QCustomPlot *plot) : QCPAbstractItem(plot) { setClipToAxisRect(false); }
  double selectTest(const QPointF &, bool, QVariant *) const override { return -1; }

  QList<Entry> entries;    ///< ascending x
  QFont  font;
  /// The note of a flag for @p n marks ("3 photos" where @p photos).
  std::function<QString(int n, bool photos)> groupNote;

  /// The entries of the time shown, with their flags.
  QList<Placed> place() const
  {
    const QCPAxis *time = parentPlot()->xAxis;
    const QFontMetricsF metrics(font);
    QList<GraphFlags::Flag> flags;
    QList<QList<int>> groups;
    QString last;     // the label of the group's last mark
    bool photos = false;
    const auto close = [&] {
      if (groups.isEmpty() || groups.last().size() < 2)
        return;
      flags.last().label += QChar(0x2013) + last;
      flags.last().note = groupNote ? groupNote(int(groups.last().size()), photos) : QString();
    };
    for (const Entry &e : entries)
      if (e.x >= time->range().lower && e.x <= time->range().upper)
      {
        // marks taken at once (a series of photos): one flag "4-6 · 3 photos"
        const double px = time->coordToPixel(e.x);
        if (e.id >= 0 && !groups.isEmpty() && groups.last().first() >= 0 && px - flags.last().x <= kTogether)
        {
          groups.last() << e.id;
          last = e.label;
          photos = photos && e.photo;
          continue;
        }
        close();
        flags << GraphFlags::Flag{ px, e.label, e.note, {}, {} };
        groups << QList<int>{ e.id };
        photos = e.photo;
      }
    close();
    GraphFlags::layout(flags, QRectF(parentPlot()->axisRect()->rect()), metrics.height() + 2,
                       [&](const QString &text) { return metrics.horizontalAdvance(text); });
    QList<Placed> out;
    for (qsizetype i = 0; i < flags.size(); ++i)
      out << Placed{ groups.at(i).first(), flags.at(i).x, flags.at(i).rect, flags.at(i).text, groups.at(i) };
    return out;
  }

protected:
  void draw(QCPPainter *painter) override
  {
    const QRectF r = parentPlot()->axisRect()->rect();
    const QCPAxis *time = parentPlot()->xAxis;
    const QList<Placed> placed = place();
    QHash<int, const Entry *> byId;
    for (const Entry &e : entries)
      byId.insert(e.id, &e);
    // every line, also those of the marks sharing a flag
    painter->setAntialiasing(false);
    for (const Entry &e : entries)
      if (e.pen.style() != Qt::NoPen && e.x >= time->range().lower && e.x <= time->range().upper)
      {
        const double x = time->coordToPixel(e.x);
        painter->setPen(e.pen);
        painter->drawLine(QLineF(x, r.top(), x, r.bottom()));
      }
    painter->setAntialiasing(true);
    painter->setFont(font);
    const double k = GraphFlags::kPoint;
    for (const Placed &p : placed)
    {
      if (p.rect.isEmpty())
        continue;
      const QColor color = byId.value(p.id)->color;
      // the tag and its point up to the line, in the line's colour
      QPainterPath tag;
      tag.addRoundedRect(p.rect, 3, 3);
      tag.addPolygon(QPolygonF({ QPointF(p.x - k, p.rect.top() + 1), QPointF(p.x, p.rect.top() - k), QPointF(p.x + k, p.rect.top() + 1) }));
      painter->setPen(Qt::NoPen);
      painter->setBrush(color);
      painter->drawPath(tag.simplified());
      // black or white, whichever stands out from the fill
      painter->setPen(qGray(color.rgb()) > 140 ? Qt::black : Qt::white);
      painter->drawText(p.rect, Qt::AlignCenter, p.text);
    }
  }
};
}

void GraphWidget::setupMarks()
{
  auto *item = new FlagLines(m_plot);
  item->setLayer(QStringLiteral("overlay"));
  item->setSelectable(false);
  item->groupNote = [](int n, bool photos) { return photos ? tr("%n photos", nullptr, n) : tr("%n marks", nullptr, n); };
  m_markItem = item;
  connect(m_timeBar, &GraphTimeBar::noteClicked, this, [this] { askNote(); });
}

// The marks of every store shown, on the graph's time; then the entries of
// the item: the marks numbered as they stand in time, and the cursors.
void GraphWidget::updateMarks()
{
  m_marks.clear();
  QList<const RecordingStore *> seen;
  bool several = false;
  for (const Curve &c : std::as_const(m_curves))
    several = several || c.value.source != m_curves.first().value.source;
  for (const Curve &c : std::as_const(m_curves))
  {
    if (!c.store || seen.contains(c.store.data()))
      continue;
    seen << c.store.data();
    const qint64 shift = qRound64(c.shift * 1000);
    const QList<RecordingStore::Mark> marks = c.store->marks();
    for (int i = 0; i < marks.size(); ++i)
    {
      const RecordingStore::Mark &m = marks.at(i);
      m_marks << Mark{ c.store, i, shift, m.t + shift, m.color, m.name, m.note, several ? c.meter : QString(),
                       m.kind == RecordingStore::MarkKind::Note, m.photo };
    }
  }
  std::stable_sort(m_marks.begin(), m_marks.end(), [](const Mark &a, const Mark &b) { return a.t < b.t; });

  auto *item = static_cast<FlagLines *>(m_markItem);
  item->font = m_plot->xAxis->tickLabelFont();
  item->entries.clear();
  // "Alarm Peak" says "Peak" on the flag: the word would take the room (the tooltip keeps it)
  const QString alarmWord = QCoreApplication::translate("Alarm", "Alarm") + ' ';
  for (int i = 0; i < m_marks.size(); ++i)
  {
    const Mark &m = m_marks.at(i);
    QString note = m.name;
    if (note.startsWith(alarmWord, Qt::CaseInsensitive) && note.size() > alarmWord.size())
      note = note.mid(alarmWord.size());
    item->entries << FlagLines::Entry{ i, m.t / 1000.0, QString::number(i + 1), note, QColor::fromRgba(m.color),
                                       QPen(QColor::fromRgba(m.color), 2, Qt::DashLine), !m.photo.isEmpty() };
  }
  for (int i = 0; i < 2; ++i)
    if (m_cursorT[i] >= 0)
      item->entries << FlagLines::Entry{ -1 - i, m_cursorT[i] / 1000.0, i ? QStringLiteral("B") : QStringLiteral("A"), QString(),
                                         m_look.cursor, QPen(Qt::NoPen) };
  std::stable_sort(item->entries.begin(), item->entries.end(),
                   [](const FlagLines::Entry &a, const FlagLines::Entry &b) { return a.x < b.x; });
}

void GraphWidget::setMarksEditable(bool on)
{
  m_marksEditable = on;
  m_timeBar->setNoteEnabled(on);
}

int GraphWidget::draggedIndex() const
{
  if (!m_dragStore)
    return -1;
  const QList<RecordingStore::Mark> marks = m_dragStore->marks();
  const auto same = [&](int i) { return i >= 0 && i < marks.size() && marks.at(i).t == m_dragT && marks.at(i).name == m_dragName; };
  if (same(m_dragIndex))
    return m_dragIndex;
  for (int i = 0; i < marks.size(); ++i)
    if (same(i))
      return i;
  return -1;
}

RecordingStore *GraphWidget::noteStore(qint64 *shift) const
{
  if (!m_marksEditable)
    return nullptr;
  const int curve = leftCurve();
  if (shift)
    *shift = curve < 0 ? 0 : qRound64(m_curves.at(curve).shift * 1000);
  return curve < 0 ? m_store.data() : m_curves.at(curve).store.data();
}

int GraphWidget::markOf(const RecordingStore *store, int index) const
{
  for (int i = 0; i < m_marks.size(); ++i)
    if (m_marks.at(i).store == store && m_marks.at(i).index == index)
      return i;
  return -1;
}

// A flag first - it lies below the plot -, then the line of a note: an
// alarm's line takes no click, there may be many
int GraphWidget::markAt(const QPoint &pos) const
{
  const QList<FlagLines::Placed> placed = static_cast<const FlagLines *>(m_markItem)->place();
  for (const FlagLines::Placed &p : placed)
    if (p.id >= 0 && p.rect.contains(pos))
      return p.id;
  if (m_plot->axisRect()->rect().contains(pos))
    for (const FlagLines::Placed &p : placed)
      if (p.id >= 0 && markIsNote(p.id) && std::abs(p.x - pos.x()) <= 3)
        return p.id;
  return -1;
}

int GraphWidget::markAtGlobal(const QPoint &globalPos) const
{
  return markAt(m_plot->mapFromGlobal(globalPos));
}

bool GraphWidget::markIsNote(int mark) const
{
  return mark >= 0 && mark < m_marks.size() && m_marks.at(mark).isNote;
}

int GraphWidget::addNote(qint64 t, const QString &title, const QString &note, const QString &photo)
{
  qint64 shift = 0;
  RecordingStore *store = noteStore(&shift);
  if (!store)
    return -1;
  const int index = store->addNote(t < 0 ? -1 : qBound(store->origin(), t - shift, qMax(store->origin(), store->duration())), title, note, photo);
  return markOf(store, index);
}

int GraphWidget::askNote(qint64 t)
{
  qint64 shift = 0;
  RecordingStore *store = noteStore(&shift);
  if (!store)
    return -1;
  const qint64 at = t < 0 ? store->markTime() : qBound(store->origin(), t - shift, qMax(store->origin(), store->duration()));
  NoteDlg dlg(word("At %1").arg(markWhen(store, at)), this);
  if (dlg.exec() != QDialog::Accepted || dlg.title().isEmpty() || !store)
    return -1;
  const int mark = markOf(store, store->addNote(at, dlg.title(), dlg.text()));
  Q_EMIT noteAdded(store->startDateTime().addMSecs(at), dlg.title());
  return mark;
}

bool GraphWidget::editMark(int mark)
{
  if (!m_marksEditable || mark < 0 || mark >= m_marks.size() || !m_marks.at(mark).store)
    return false;
  const Mark m = m_marks.at(mark);
  NoteDlg dlg(word("Mark %1 at %2").arg(mark + 1).arg(markWhen(m.store, m.t - m.shift)), this);
  dlg.setTitle(m.name);
  dlg.setText(m.note);
  // the marks may have changed meanwhile: only the one that was asked about
  if (dlg.exec() != QDialog::Accepted || dlg.title().isEmpty() || !m.store || m.index >= m.store->marks().size()
      || m.store->marks().at(m.index).t != m.t - m.shift)
    return false;
  m.store->setMarkText(m.index, dlg.title(), dlg.text());
  return true;
}

void GraphWidget::removeMark(int mark)
{
  if (m_marksEditable && mark >= 0 && mark < m_marks.size() && m_marks.at(mark).store)
    m_marks.at(mark).store->removeMark(m_marks.at(mark).index);
}

int GraphWidget::moveMark(int mark, qint64 t)
{
  if (!m_marksEditable || !markIsNote(mark) || !m_marks.at(mark).store)
    return mark;
  const Mark m = m_marks.at(mark);
  const qint64 at = qBound(m.store->origin(), t - m.shift, qMax(m.store->origin(), m.store->duration()));
  return markOf(m.store, m.store->setMarkTime(m.index, at));
}

void GraphWidget::endMarkDrag(bool cancel)
{
  if (m_drag != Drag::Mark)
    return;
  m_drag = Drag::None;
  const int index = draggedIndex();
  if (!m_moved || index < 0)
    return;
  if (cancel)
  {
    m_dragStore->setMarkTime(index, m_dragFrom);
    return;
  }
  const RecordingStore::Mark m = m_dragStore->marks().at(index);
  if (m.t != m_dragFrom)
    Q_EMIT noteMoved(m_dragStore->startDateTime().addMSecs(m_dragFrom), m_dragStore->startDateTime().addMSecs(m.t), m.name);
}

QString GraphWidget::markTip(int mark) const
{
  if (mark < 0 || mark >= m_marks.size() || !m_marks.at(mark).store)
    return QString();
  const Mark &m = m_marks.at(mark);
  QString html = QString("<b>%1 · %2</b><br>%3").arg(mark + 1).arg(m.name.toHtmlEscaped(), markWhen(m.store, m.t - m.shift));
  // whose it is, where several meters are shown
  if (!m.meter.isEmpty())
    html += "<br>" + m.meter.toHtmlEscaped();
  if (!m.note.isEmpty())
    html += "<p>" + m.note.toHtmlEscaped().replace('\n', "<br>") + "</p>";
  if (!m.photo.isEmpty())
    html += "<p><i>" + tr("Click to show the photo").toHtmlEscaped() + "</i></p>";
  return html;
}

qint64 GraphWidget::timeAt(const QPoint &globalPos) const
{
  const QPoint pos = m_plot->mapFromGlobal(globalPos);
  return m_plot->axisRect()->rect().contains(pos) ? qRound64(m_plot->xAxis->pixelToCoord(pos.x()) * 1000) : -1;
}

qint64 GraphWidget::markTime(int mark) const
{
  return mark >= 0 && mark < m_marks.size() ? m_marks.at(mark).t : -1;
}

QString GraphWidget::markName(int mark) const
{
  return mark >= 0 && mark < m_marks.size() ? m_marks.at(mark).name : QString();
}

QString GraphWidget::markPhoto(int mark) const
{
  return mark >= 0 && mark < m_marks.size() ? m_marks.at(mark).photo : QString();
}

QColor GraphWidget::markColor(int mark) const
{
  return mark >= 0 && mark < m_marks.size() ? QColor::fromRgba(m_marks.at(mark).color) : QColor();
}

int GraphWidget::markPixel(int mark) const
{
  for (const FlagLines::Placed &p : static_cast<const FlagLines *>(m_markItem)->place())
    if (p.group.contains(mark))
      return qRound(m_plot->xAxis->coordToPixel(m_marks.at(mark).t / 1000.0));
  return -1;
}

QList<int> GraphWidget::markGroup(int mark) const
{
  for (const FlagLines::Placed &p : static_cast<const FlagLines *>(m_markItem)->place())
    if (p.group.contains(mark))
      return p.group;
  return {};
}

QString GraphWidget::flagTip(int mark) const
{
  const QList<int> group = markGroup(mark);
  if (group.size() < 2)
    return markTip(mark);
  // one line per mark; the click asks which one
  QStringList lines;
  for (int m : group)
    if (const Mark &one = m_marks.at(m); one.store)
      lines << QString("<b>%1 · %2</b> %3").arg(m + 1).arg(one.name.toHtmlEscaped(),
                                                          one.store->startDateTime().addMSecs(one.t - one.shift).toString("HH:mm:ss"));
  return lines.join("<br>") + "<p><i>" + tr("Click to choose one").toHtmlEscaped() + "</i></p>";
}

void GraphWidget::chooseMark(const QList<int> &group, const QPoint &globalPos)
{
  // what each entry does, taken now: the marks may change while the menu is open
  QMenu menu(m_plot);
  for (int m : group)
  {
    const Mark &one = m_marks.at(m);
    if (!one.store)
      continue;
    QAction *action = menu.addAction(QString("%1 · %2  %3").arg(m + 1).arg(one.name, one.store->startDateTime().addMSecs(one.t - one.shift).toString("HH:mm:ss")));
    action->setData(QStringList{ one.photo, markTip(m) });
  }
  const QAction *chosen = menu.exec(globalPos);
  if (!chosen)
    return;
  const QStringList what = chosen->data().toStringList();
  if (!what.value(0).isEmpty())
    Q_EMIT photoClicked(what.value(0));
  else
    QToolTip::showText(globalPos, what.value(1), m_plot);
}

QRect GraphWidget::markFlagRect(int mark) const
{
  for (const FlagLines::Placed &p : static_cast<const FlagLines *>(m_markItem)->place())
    if (p.id == mark)
      return p.rect.toAlignedRect();
  return QRect();
}

QString GraphWidget::markFlagText(int mark) const
{
  for (const FlagLines::Placed &p : static_cast<const FlagLines *>(m_markItem)->place())
    if (p.id == mark)
      return p.rect.isEmpty() ? QString() : p.text;
  return QString();
}

QRect GraphWidget::cursorFlagRect(int which) const
{
  return markFlagRect(-1 - (which & 1));
}
