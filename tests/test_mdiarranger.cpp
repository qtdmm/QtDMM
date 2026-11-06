// Tests for MdiArranger: the automatic layout ("Displays on top") and the
// title bars on a real QMdiArea (offscreen).
#include <QApplication>
#include <QDialog>
#include <QDebug>
#include <QLabel>
#include <QMdiArea>
#include <QMdiSubWindow>
#include <QMouseEvent>
#include <QSignalSpy>
#include <QToolButton>
#include <QVBoxLayout>
#include <QTest>

#include "ui/mdiarranger.h"
#include "testutil.h"

using R = MdiArranger::Role;

// no two rects overlap, all lie inside the area
static void checkTiling(const QRect &area, const QList<QRect> &rects, const QString &name)
{
  for (int i = 0; i < rects.size(); ++i)
  {
    check(area.contains(rects[i]), QString("%1: rect %2 inside the area").arg(name).arg(i));
    check(rects[i].width() > 50 && rects[i].height() > 50, QString("%1: rect %2 not degenerate").arg(name).arg(i));
    for (int j = i + 1; j < rects.size(); ++j)
      check(!rects[i].intersects(rects[j]), QString("%1: rects %2 and %3 do not overlap").arg(name).arg(i).arg(j));
  }
}

static void testLayout()
{
  const QRect area(0, 0, 1200, 700);

  // the full set: instruments on top, graph below, table on the right
  {
    const QList<QRect> r = MdiArranger::layout(area, { R::Instrument, R::Instrument, R::Graph, R::Table }, 22);
    checkTiling(area, r, "full");
    check(r[0].top() == r[1].top() && r[0].height() == r[1].height(), "full: instruments in one row");
    check(r[0].right() < r[1].left(), "full: first instrument on the left");
    check(r[0].bottom() < r[2].top() && r[1].bottom() < r[3].top(), "full: instruments above graph and table");
    check(r[0].height() <= area.height() * 0.45 + 1, "full: instrument strip at most 45 % high");
    check(r[3].left() > r[2].right() && r[3].width() >= 260, "full: table in a column right of the graph");
    check(r[2].width() > r[3].width(), "full: the graph is the wider one");
  }
  // only a graph: all of it
  {
    const QList<QRect> r = MdiArranger::layout(area, { R::Graph }, 22);
    check(r[0] == area.adjusted(6, 6, -6, -6), "graph alone fills the area");
  }
  // only instruments: the whole height
  {
    const QList<QRect> r = MdiArranger::layout(area, { R::Instrument, R::Instrument }, 22);
    checkTiling(area, r, "instruments");
    check(r[0].height() > area.height() * 0.9, "instruments alone take the whole height");
  }
  // a narrow, tall area: two instruments go into two rows
  {
    const QRect tall(0, 0, 500, 1200);
    const QList<QRect> r = MdiArranger::layout(tall, { R::Instrument, R::Instrument }, 22);
    checkTiling(tall, r, "tall");
    check(r[0].bottom() < r[1].top(), "tall: instruments on top of each other");
  }
  // instrument and table, no graph: the table takes the rest
  {
    const QList<QRect> r = MdiArranger::layout(area, { R::Instrument, R::Table }, 22);
    checkTiling(area, r, "no graph");
    check(r[1].width() == area.width() - 12, "no graph: table full width");
  }
}

static void testWindows()
{
  QMdiArea area;
  area.resize(1000, 700);
  MdiArranger arranger(&area);
  QList<QMdiSubWindow *> wins;
  for (const char *name : { "a", "b", "g" })
  {
    QMdiSubWindow *w = area.addSubWindow(new QLabel(name));
    w->setObjectName(name);
    wins << w;
  }
  arranger.addWindow(wins[0], R::Instrument);
  arranger.addWindow(wins[1], R::Instrument);
  arranger.addWindow(wins[2], R::Graph);
  area.show();
  QTest::qWait(50);

  check(arranger.titleBarsHidden(), "title bars hidden in the automatic mode");
  check(MdiArranger::titleBarHidden(wins[0]), "window a has no title bar");
  check(wins[0]->geometry().right() < wins[1]->geometry().left(), "a left of b");
  check(wins[0]->geometry().bottom() < wins[2]->geometry().top(), "instruments above the graph");

  wins[1]->hide();
  QTest::qWait(50);
  check(wins[0]->geometry().width() > 800, "with b hidden a takes the whole strip");

  arranger.setMode(MdiArranger::Free);
  check(!arranger.titleBarsHidden() && !MdiArranger::titleBarHidden(wins[0]), "Free brings the title bars back");
  const QRect before = wins[0]->geometry();
  area.resize(600, 400);
  QTest::qWait(50);
  check(wins[0]->geometry() == before, "Free: a resize does not move the windows");

  arranger.setTitleBarHidden(wins[2], true);
  check(MdiArranger::titleBarHidden(wins[2]) && !MdiArranger::titleBarHidden(wins[0]), "one title bar alone");

  // back to the automatic mode, hide and show a window, then title bars on:
  // every visible window has its title bar again
  arranger.setMode(MdiArranger::DisplaysOnTop);
  wins[1]->show();
  wins[0]->hide();
  QTest::qWait(50);
  wins[0]->show();
  QTest::qWait(50);
  arranger.setTitleBarsHidden(false);
  QTest::qWait(50);
  for (QMdiSubWindow *w : wins)
    check(!(w->windowFlags() & Qt::FramelessWindowHint) && !MdiArranger::titleBarHidden(w),
          QString("title bar back on %1").arg(w->objectName()));
}

// counts the mouse presses and releases a widget gets
class ClickCounter : public QLabel
{
public:
  using QLabel::QLabel;
  int presses = 0;
  int releases = 0;

protected:
  void mousePressEvent(QMouseEvent *e) override { presses++; QLabel::mousePressEvent(e); }
  void mouseReleaseEvent(QMouseEvent *e) override { releases++; QLabel::mouseReleaseEvent(e); }
};

static void sendMouse(QWidget *w, QEvent::Type type, const QPoint &pos, Qt::MouseButtons buttons)
{
  QMouseEvent e(type, pos, w->mapToGlobal(pos), Qt::LeftButton, buttons, Qt::ControlModifier);
  QApplication::sendEvent(w, &e);
}

// Free mode: Ctrl+click reaches the widget (multiple selection in the table),
// Ctrl+drag moves the window and still leaves press and release to the widget
static void testCtrlDrag()
{
  QMdiArea area;
  area.resize(1000, 700);
  MdiArranger arranger(&area);
  auto *label = new ClickCounter("t");
  QMdiSubWindow *w = area.addSubWindow(label);
  arranger.addWindow(w, R::Table);
  area.show();
  arranger.setMode(MdiArranger::Free);
  w->setGeometry(50, 50, 300, 200);
  QTest::qWait(50);

  const QPoint p = label->rect().center();
  sendMouse(label, QEvent::MouseButtonPress, p, Qt::LeftButton);
  sendMouse(label, QEvent::MouseButtonRelease, p, Qt::NoButton);
  check(label->presses == 1 && label->releases == 1, "Ctrl+click reaches the widget");
  check(w->pos() == QPoint(50, 50), "Ctrl+click does not move the window");

  // the window follows the cursor, so the widget-local position stays the same
  sendMouse(label, QEvent::MouseButtonPress, p, Qt::LeftButton);
  sendMouse(label, QEvent::MouseMove, p + QPoint(1, 0), Qt::LeftButton);
  check(w->pos() == QPoint(50, 50), "Ctrl+drag waits for the drag distance");
  sendMouse(label, QEvent::MouseMove, p + QPoint(40, 30), Qt::LeftButton);
  check(w->pos() == QPoint(90, 80), QString("Ctrl+drag moves the window (at %1,%2)").arg(w->x()).arg(w->y()));
  sendMouse(label, QEvent::MouseButtonRelease, p, Qt::NoButton);
  check(label->presses == 2 && label->releases == 2, "the drag leaves press and release to the widget");

  // a button under the press: Ctrl+click fires it, Ctrl+drag does not (a
  // meter key would send a command)
  {
    auto *box = new QWidget;
    auto *button = new QToolButton(box);
    button->setText("key");
    (new QVBoxLayout(box))->addWidget(button);
    QMdiSubWindow *bw = area.addSubWindow(box);
    arranger.addWindow(bw, R::Instrument);
    bw->show();
    bw->setGeometry(400, 50, 200, 150);
    QTest::qWait(50);
    QSignalSpy clicked(button, &QToolButton::clicked);
    const QPoint b = button->rect().center();
    sendMouse(button, QEvent::MouseButtonPress, b, Qt::LeftButton);
    sendMouse(button, QEvent::MouseButtonRelease, b, Qt::NoButton);
    check(clicked.size() == 1, "Ctrl+click on a button fires it");
    sendMouse(button, QEvent::MouseButtonPress, b, Qt::LeftButton);
    sendMouse(button, QEvent::MouseMove, b + QPoint(40, 30), Qt::LeftButton);
    sendMouse(button, QEvent::MouseButtonRelease, b, Qt::NoButton);
    check(bw->pos() == QPoint(440, 80), QString("Ctrl+drag on a button moves the window (at %1,%2)").arg(bw->x()).arg(bw->y()));
    check(clicked.size() == 1 && !button->isDown(), "Ctrl+drag on a button does not fire it");
  }

  // in the automatic mode Ctrl+drag does nothing
  arranger.setMode(MdiArranger::DisplaysOnTop);
  QTest::qWait(50);
  const QPoint auto_ = w->pos();
  sendMouse(label, QEvent::MouseButtonPress, p, Qt::LeftButton);
  sendMouse(label, QEvent::MouseMove, p + QPoint(40, 30), Qt::LeftButton);
  sendMouse(label, QEvent::MouseButtonRelease, p + QPoint(40, 30), Qt::NoButton);
  check(w->pos() == auto_, "Displays on top: Ctrl+drag does not move");
}

// ---------------------------------------------------------------- split tree

static void sendAt(QWidget *w, QEvent::Type type, const QPoint &global, Qt::MouseButtons buttons,
                   Qt::KeyboardModifiers mods = Qt::NoModifier)
{
  const QPoint local = w->mapFromGlobal(global);
  QMouseEvent e(type, local, global, type == QEvent::MouseMove ? Qt::NoButton : Qt::LeftButton, buttons, mods);
  QApplication::sendEvent(w, &e);
}

static QPoint globalCenter(QWidget *w)
{
  return w->mapToGlobal(w->rect().center());
}

static void testLeft()
{
  const QRect area(0, 0, 1200, 700);
  const QList<QRect> r = MdiArranger::layout(area, { R::Instrument, R::Instrument, R::Graph, R::Table }, 22, 260, true);
  checkTiling(area, r, "left");
  check(r[0].right() < r[2].left() && r[1].right() < r[2].left(), "left: instruments left of the graph");
  check(r[0].left() == r[1].left() && r[0].bottom() < r[1].top(), "left: instruments in one column");
  check(r[0].width() <= area.width() * 0.4 + 1, "left: instrument column at most 40 % wide");
  check(r[3].left() > r[2].right(), "left: table right of the graph");
}

// Displays on the left: a cell no taller than its content can fill (the LCD
// keeps 1.8..3.2), the rest to the analog meter
static void testFit()
{
  const QRect area(0, 0, 1200, 800);
  const QList<R> roles { R::Instrument, R::Instrument, R::Graph, R::Table };
  const QList<MdiArranger::Fit> fits { { 1.8, 3.2, 0, 0 }, { 1.3, 2.2, 0, 0 }, {}, {} };
  const QList<QRect> r = MdiArranger::layout(area, roles, 22, 260, true, fits);
  checkTiling(area, r, "fit");
  check(qAbs(r[0].height() - r[0].width() / 1.8) <= 2,
        QString("fit: LCD cell %1 x %2, as tall as 1.8 allows").arg(r[0].width()).arg(r[0].height()));
  check(r[1].height() > r[0].height() && r[1].bottom() == area.height() - 7, "fit: the meter takes the rest");
  // without fits the column is shared evenly, as before
  const QList<QRect> even = MdiArranger::layout(area, roles, 22, 260, true);
  check(qAbs(even[0].height() - even[1].height()) <= 1, "no fits: an even share");
  // on top: the meter keeps at most 2.2 wide, the LCD gets the rest
  const QList<QRect> top = MdiArranger::layout(QRect(0, 0, 1400, 700), roles, 22, 260, false, fits);
  check(qAbs(top[1].width() - top[1].height() * 2.2) <= 2 && top[0].width() > top[1].width(),
        QString("fit on top: LCD %1, meter %2 x %3").arg(top[0].width()).arg(top[1].width()).arg(top[1].height()));
}

static void testText()
{
  const QStringList names { "display", "meter", "graph", "readings" };
  using Node = MdiArranger::Node;
  const Node rule = MdiArranger::ruleTree(QRect(0, 0, 1200, 700), { R::Instrument, R::Instrument, R::Graph, R::Table },
                                          false, 22);
  const QString text = MdiArranger::toText(rule, names);
  check(text.startsWith("V(") && text.contains("H(") && text.contains("display") && text.contains("readings"),
        "text: " + text);
  Node back;
  check(MdiArranger::fromText(text, names, back) && MdiArranger::toText(back, names) == text, "text round trip");
  QList<QRect> a, b;
  MdiArranger::applyTree(rule, QRect(0, 0, 1000, 600), a);
  MdiArranger::applyTree(back, QRect(0, 0, 1000, 600), b);
  check(a == b, "text round trip: same rects");
  // ratios are normalised
  check(MdiArranger::fromText("H(2:display,2:meter)", names, back) && back.ratios[0] == 0.5, "text: ratios normalised");
  for (const char *bad : { "", "H(0.5:display)", "H(0.5:display,0.5:foo)", "H(0.5:display,0.5:display)",
                           "V(0.5:display,0.5:meter", "display,meter", "H(x:display,1:meter)", "H(0:display,1:meter)" })
    check(!MdiArranger::fromText(bad, names, back), QString("text rejected: '%1'").arg(bad));
}

// Sarah's hand-made example from the UI demo: two meters on top (offset a
// little), two graphs on the left below (one indented), the table on the
// right not reaching the edges
static void testDerive()
{
  const QList<QRect> rects { QRect(10, 10, 480, 250), QRect(505, 22, 480, 240), QRect(30, 290, 560, 200),
                             QRect(10, 505, 580, 190), QRect(620, 280, 330, 380) };
  using Node = MdiArranger::Node;
  const Node t = MdiArranger::deriveTree(rects, 40);
  check(!t.isLeaf() && t.orient == Qt::Vertical && t.kids.size() == 2, "derive: two rows");
  if (!t.isLeaf() && t.kids.size() == 2)
  {
    check(t.kids[0].orient == Qt::Horizontal && MdiArranger::leaves(t.kids[0]) == QList<int>({ 0, 1 }),
          "derive: the meters side by side on top");
    const Node &bottom = t.kids[1];
    check(bottom.orient == Qt::Horizontal && bottom.kids.size() == 2, "derive: bottom split into columns");
    if (bottom.kids.size() == 2)
    {
      check(bottom.kids[0].orient == Qt::Vertical && MdiArranger::leaves(bottom.kids[0]) == QList<int>({ 2, 3 }),
            "derive: the graphs on top of each other");
      check(bottom.kids[1].isLeaf() && bottom.kids[1].leaf == 4, "derive: the table on the right");
    }
  }
  // removing a leaf: the neighbours share its space
  Node n = t;
  check(MdiArranger::removeLeaf(n, 3), "remove leaf");
  QList<int> left = MdiArranger::leaves(n);
  check(left == QList<int>({ 0, 1, 2, 4 }), "remove leaf: the others stay");
  QList<QRect> r;
  MdiArranger::applyTree(n, QRect(0, 0, 1000, 700), r);
  check(r[2].bottom() == 699, "remove leaf: the graph above takes the space");
  MdiArranger::addAtEdge(n, 3, false);
  MdiArranger::applyTree(n, QRect(0, 0, 1000, 700), r);
  check(r[3].bottom() == 699 && r[3].width() == 1000, "add at edge: a new row at the bottom");
}

static void testArranged()
{
  QMdiArea area;
  area.resize(1000, 700);
  MdiArranger arranger(&area);
  arranger.setPreviewTime(0);
  QList<QMdiSubWindow *> wins;
  QList<ClickCounter *> labels;
  for (const char *name : { "a", "b", "g" })
  {
    auto *label = new ClickCounter(name);
    QMdiSubWindow *w = area.addSubWindow(label);
    w->setObjectName(name);
    wins << w;
    labels << label;
  }
  arranger.addWindow(wins[0], R::Instrument);
  arranger.addWindow(wins[1], R::Instrument);
  arranger.addWindow(wins[2], R::Graph);
  area.show();
  QTest::qWait(50);
  const QRect ra = wins[0]->geometry(), rb = wins[1]->geometry();
  check(arranger.layoutText().isEmpty(), "the rule: nothing to save");

  // Ctrl+drag a onto b: they swap
  QWidget *la = labels[0];
  sendAt(la, QEvent::MouseButtonPress, globalCenter(la), Qt::LeftButton, Qt::ControlModifier);
  const QPoint target = globalCenter(wins[1]);
  sendAt(la, QEvent::MouseMove, target, Qt::LeftButton, Qt::ControlModifier);
  sendAt(la, QEvent::MouseButtonRelease, target, Qt::NoButton, Qt::ControlModifier);
  QTest::qWait(50);
  check(wins[0]->geometry() == rb && wins[1]->geometry() == ra,
        QString("Ctrl+drag onto another window swaps them (a at %1,%2)").arg(wins[0]->x()).arg(wins[0]->y()));
  check(labels[0]->presses == 1 && labels[0]->releases == 1 && labels[1]->presses == 0, "the swap is no click");
  check(arranger.layoutText().contains("b") && arranger.layoutText().indexOf('b') < arranger.layoutText().indexOf('a'),
        "a swap is kept: " + arranger.layoutText());

  // at the title bar, when there is one: the same swap back
  arranger.setTitleBarsHidden(false);
  QTest::qWait(50);
  {
    const QRect before0 = wins[0]->geometry(), before1 = wins[1]->geometry();
    const QPoint title = wins[0]->mapToGlobal(QPoint(wins[0]->width() / 2, 14));
    const QPoint to = globalCenter(wins[1]);
    sendAt(wins[0], QEvent::MouseMove, title, Qt::NoButton);   // hovering the title bar first, as a mouse does
    sendAt(wins[0], QEvent::MouseButtonPress, title, Qt::LeftButton);
    sendAt(wins[0], QEvent::MouseMove, title + QPoint(10, 0), Qt::LeftButton);
    sendAt(wins[0], QEvent::MouseMove, to, Qt::LeftButton);
    sendAt(wins[0], QEvent::MouseButtonRelease, to, Qt::NoButton);
    QTest::qWait(50);
    check(wins[0]->geometry() == before1 && wins[1]->geometry() == before0,
          QString("dragged at the title bar onto another window: swapped (a at %1,%2)").arg(wins[0]->x()).arg(wins[0]->y()));
    // and back, so the rest of the test sees the swap made with Ctrl
    const QPoint title2 = wins[0]->mapToGlobal(QPoint(wins[0]->width() / 2, 14));
    const QPoint to2 = globalCenter(wins[1]);
    sendAt(wins[0], QEvent::MouseMove, title2, Qt::NoButton);
    sendAt(wins[0], QEvent::MouseButtonPress, title2, Qt::LeftButton);
    sendAt(wins[0], QEvent::MouseMove, title2 + QPoint(10, 0), Qt::LeftButton);
    sendAt(wins[0], QEvent::MouseMove, to2, Qt::LeftButton);
    sendAt(wins[0], QEvent::MouseButtonRelease, to2, Qt::NoButton);
    QTest::qWait(50);
  }
  arranger.setTitleBarsHidden(true);
  QTest::qWait(50);

  // dropped outside any window: back to its cell
  sendAt(la, QEvent::MouseButtonPress, globalCenter(la), Qt::LeftButton, Qt::ControlModifier);
  const QPoint nowhere = area.viewport()->mapToGlobal(QPoint(-50, -50));
  sendAt(la, QEvent::MouseMove, nowhere, Qt::LeftButton, Qt::ControlModifier);
  sendAt(la, QEvent::MouseButtonRelease, nowhere, Qt::NoButton, Qt::ControlModifier);
  QTest::qWait(50);
  check(wins[0]->geometry() == rb, "dropped outside: back to its cell");

  // the divider between the strip and the graph: drag it 60 px down (at
  // x = 500, where it meets the one between a and b)
  QWidget *vp = area.viewport();
  const int gapY = (wins[0]->geometry().bottom() + wins[2]->geometry().top()) / 2;
  const QPoint gap = vp->mapToGlobal(QPoint(500, gapY));
  const int graphTop = wins[2]->geometry().top();
  sendAt(vp, QEvent::MouseButtonPress, gap, Qt::LeftButton);
  sendAt(vp, QEvent::MouseMove, gap + QPoint(0, 60), Qt::LeftButton);
  sendAt(vp, QEvent::MouseButtonRelease, gap + QPoint(0, 60), Qt::NoButton);
  QTest::qWait(50);
  check(qAbs(wins[2]->geometry().top() - (graphTop + 60)) <= 1,
        QString("divider dragged: graph top %1, expected %2").arg(wins[2]->geometry().top()).arg(graphTop + 60));
  // the ratio stays when the area grows
  const double share = double(wins[0]->height()) / vp->height();
  area.resize(1000, 900);
  QTest::qWait(50);
  check(qAbs(double(wins[0]->height()) / vp->height() - share) < 0.02, "the dragged ratio scales with the area");
  // taken into the window a few pixels, too
  const int gap2 = (wins[0]->geometry().bottom() + wins[2]->geometry().top()) / 2;
  const QPoint inGraph = vp->mapToGlobal(QPoint(500, wins[2]->geometry().top() + 2));
  const int top2 = wins[2]->geometry().top();
  sendAt(labels[2], QEvent::MouseButtonPress, inGraph, Qt::LeftButton);
  sendAt(labels[2], QEvent::MouseMove, inGraph - QPoint(0, 30), Qt::LeftButton);
  sendAt(labels[2], QEvent::MouseButtonRelease, inGraph - QPoint(0, 30), Qt::NoButton);
  QTest::qWait(50);
  check(qAbs(wins[2]->geometry().top() - (top2 - 30)) <= 1 && labels[2]->presses == 0,
        QString("divider taken inside the window edge (gap at %1)").arg(gap2));

  // a dialog of a window (Preferences) lying over the divider: its mouse is
  // its own - no split cursor, no divider drag, no Ctrl+drag of the window
  {
    auto *dialog = new QDialog(labels[2]);
    auto *inside = new ClickCounter("dialog", dialog);
    inside->resize(200, 100);
    dialog->resize(200, 100);
    const QPoint overGap = vp->mapToGlobal(QPoint(500, (wins[0]->geometry().bottom() + wins[2]->geometry().top()) / 2));
    dialog->move(overGap - QPoint(100, 50));
    dialog->show();
    QTest::qWait(50);
    const QRect graphBefore = wins[2]->geometry();
    sendAt(inside, QEvent::MouseMove, overGap, Qt::NoButton);
    check(QApplication::overrideCursor() == nullptr, "no split cursor over a dialog");
    sendAt(inside, QEvent::MouseButtonPress, overGap, Qt::LeftButton);
    sendAt(inside, QEvent::MouseMove, overGap + QPoint(0, 40), Qt::LeftButton);
    sendAt(inside, QEvent::MouseButtonRelease, overGap + QPoint(0, 40), Qt::NoButton);
    sendAt(inside, QEvent::MouseButtonPress, overGap, Qt::LeftButton, Qt::ControlModifier);
    sendAt(inside, QEvent::MouseMove, overGap + QPoint(40, 30), Qt::LeftButton, Qt::ControlModifier);
    sendAt(inside, QEvent::MouseButtonRelease, overGap + QPoint(40, 30), Qt::NoButton, Qt::ControlModifier);
    QTest::qWait(50);
    check(wins[2]->geometry() == graphBefore && inside->presses == 2,
          "a press in a dialog over the divider stays in the dialog");
    delete dialog;
  }

  // saved and taken back by another arranger
  const QString saved = arranger.layoutText();
  const QRect graphBefore = wins[2]->geometry();
  {
    MdiArranger other(&area);
    for (int i = 0; i < 3; ++i)
      other.addWindow(wins[i], i == 2 ? R::Graph : R::Instrument);
    check(other.setLayoutText(saved), "layout text taken back");
    QTest::qWait(50);
    check(wins[2]->geometry() == graphBefore && wins[0]->geometry().left() > wins[1]->geometry().left(),
          "layout text: same layout again");
    check(!other.setLayoutText("V(0.5:a,0.5:zz)"), "layout text with an unknown window refused");
  }
  arranger.arrange();
  QTest::qWait(50);

  // another window set: the rule again
  wins[1]->hide();
  QTest::qWait(50);
  check(arranger.layoutText().isEmpty() && wins[0]->width() > 900, "a window less: back to the rule");
  wins[1]->show();
  QTest::qWait(50);

  // Displays on the left
  arranger.setMode(MdiArranger::DisplaysOnLeft);
  QTest::qWait(50);
  check(wins[0]->geometry().right() < wins[2]->geometry().left(), "Displays on the left");

  // Fixed from Free: the grid from the positions
  arranger.setMode(MdiArranger::Free);
  wins[0]->setGeometry(20, 20, 400, 250);
  wins[1]->setGeometry(450, 30, 400, 240);
  wins[2]->setGeometry(30, 300, 800, 300);
  arranger.setMode(MdiArranger::Fixed);
  QTest::qWait(50);
  check(arranger.titleBarsHidden(), "Fixed: title bars hidden");
  check(wins[0]->geometry().top() == wins[1]->geometry().top() && wins[0]->geometry().right() < wins[1]->geometry().left()
          && wins[2]->geometry().top() > wins[0]->geometry().bottom() && wins[2]->geometry().width() > 900,
        "Fixed from Free: meters side by side, graph below across");
  const QString fixedText = arranger.layoutText();
  check(!fixedText.isEmpty(), "Fixed: the tree is saved");
  const QRect placeOfB = wins[1]->geometry();
  wins[1]->hide();
  QTest::qWait(50);
  check(wins[0]->width() > 900, "Fixed: a hidden window leaves its space to its neighbour");
  wins[1]->show();
  QTest::qWait(50);
  check(wins[1]->geometry() == placeOfB, "Fixed: a window shown again gets its place back");

  // closed for good, and back under its name: the tree stays, in one go and
  // with the arranger working between
  auto reopen = [&](QMdiSubWindow *old)
  {
    const QString name = old->objectName();
    delete old;
    QMdiSubWindow *w = area.addSubWindow(new QLabel(name));
    w->setObjectName(name);
    w->show();
    arranger.addWindow(w, R::Instrument);
    return w;
  };
  wins[1] = reopen(wins[1]);
  QTest::qWait(50);
  check(wins[1]->geometry() == placeOfB, "Fixed: reopened at once, it stands where it stood");
  const QString name = wins[1]->objectName();
  delete wins[1];
  arranger.arrange();
  QTest::qWait(50);
  check(wins[0]->width() > 900 && wins[2]->width() > 900, "Fixed: a closed window leaves its space");
  wins[1] = area.addSubWindow(new QLabel(name));
  wins[1]->setObjectName(name);
  wins[1]->show();
  arranger.addWindow(wins[1], R::Instrument);
  QTest::qWait(50);
  check(wins[1]->geometry() == placeOfB, "Fixed: added again later, it stands where it stood");

  // both gone, both back: whichever comes first
  wins[0]->hide();
  wins[1]->hide();
  QTest::qWait(50);
  wins[1]->show();
  wins[0]->show();
  QTest::qWait(50);
  check(wins[1]->geometry() == placeOfB && wins[0]->geometry().right() < wins[1]->geometry().left(),
        "Fixed: two windows back at once, each in its place");

  // a new instrument stands beside the others, not below the graph
  QMdiSubWindow *fresh = area.addSubWindow(new QLabel("new"));
  fresh->setObjectName("new");
  fresh->show();
  arranger.addWindow(fresh, R::Instrument);
  QTest::qWait(50);
  check(fresh->geometry().top() == wins[1]->geometry().top() && fresh->geometry().left() > wins[1]->geometry().left()
          && fresh->geometry().bottom() < wins[2]->geometry().top(),
        "Fixed: a new instrument joins the row of instruments");
  delete fresh;
  arranger.arrange();
  QTest::qWait(50);
  // a resize keeps the proportions
  const double gShare = double(wins[2]->height()) / vp->height();
  area.resize(900, 700);
  QTest::qWait(50);
  check(qAbs(double(wins[2]->height()) / vp->height() - gShare) < 0.02, "Fixed: the cells scale with the area");

  // the first instrument of a tree without any: at the start, above the graph
  {
    MdiArranger::Node tree;
    tree.leaf = 0;
    MdiArranger::addAtEdge(tree, 1, false, true);
    check(tree.kids.size() == 2 && tree.kids[0].leaf == 1 && tree.kids[1].leaf == 0, "addAtEdge first: before the rest");
  }
}

int main(int argc, char **argv)
{
  qputenv("QT_QPA_PLATFORM", "offscreen");
  QApplication app(argc, argv);
  testLayout();
  testWindows();
  testCtrlDrag();
  testLeft();
  testFit();
  testText();
  testDerive();
  testArranged();
  if (failed)
    qWarning() << failed << "check(s) failed";
  else
    qInfo() << "all checks passed";
  return failed ? 1 : 0;
}
