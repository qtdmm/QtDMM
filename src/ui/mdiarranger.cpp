// Copyright (c) 2026 The QtDMM developers
// SPDX-License-Identifier: GPL-3.0-or-later
#include "ui/mdiarranger.h"

#include <QApplication>
#include <QMdiArea>
#include <QMdiSubWindow>
#include <QMouseEvent>
#include <QPainter>
#include <QRubberBand>
#include <QTimer>
#include <QtMath>

#include <algorithm>
#include <cmath>
#include <limits>

namespace
{
constexpr int kGap = 6;              // between windows and at the edges
constexpr double kAspect = 1.75;     // width/height an instrument face looks right at
constexpr int kTableMinWidth = 260;
constexpr int kGrip = 4;             // a divider can be taken this far into the windows

using Node = MdiArranger::Node;

Node leafNode(int leaf)
{
  Node n;
  n.leaf = leaf;
  return n;
}

Node splitNode(Qt::Orientation orient, std::vector<Node> kids, std::vector<double> ratios = {})
{
  if (kids.size() == 1)
    return kids.front();
  Node n;
  n.orient = orient;
  n.kids = std::move(kids);
  n.ratios = ratios.empty() ? std::vector<double>(n.kids.size(), 1.0 / n.kids.size()) : std::move(ratios);
  return n;
}

// leaf i becomes map[i]
void remapLeaves(Node &n, const QList<int> &map)
{
  if (n.isLeaf())
    n.leaf = map.value(n.leaf, -1);
  for (Node &k : n.kids)
    remapLeaves(k, map);
}

// the window at index @p removed is gone: the ones after it move up
void shiftLeaves(Node &n, int removed)
{
  if (n.leaf > removed)
    --n.leaf;
  for (Node &k : n.kids)
    shiftLeaves(k, removed);
}

// the split that holds @p child directly; nullptr: it is the root
Node *parentOf(Node &n, const Node *child)
{
  for (Node &k : n.kids)
  {
    if (&k == child)
      return &n;
    if (Node *p = parentOf(k, child))
      return p;
  }
  return nullptr;
}

// Keeps the leaves @p map has a window for, under its index; the others'
// neighbours share their room. False: nothing of @p n stays.
bool keepLeaves(Node &n, const QList<int> &map)
{
  if (n.isLeaf())
  {
    n.leaf = map.value(n.leaf, -1);
    return n.leaf >= 0;
  }
  std::vector<Node> kids;
  std::vector<double> ratios;
  double sum = 0;
  for (size_t i = 0; i < n.kids.size(); ++i)
    if (keepLeaves(n.kids[i], map))
    {
      kids.push_back(n.kids[i]);
      ratios.push_back(n.ratios[i]);
      sum += n.ratios[i];
    }
  if (kids.empty())
    return false;
  for (double &r : ratios)
    r /= sum;
  n = splitNode(n.orient, std::move(kids), std::move(ratios));
  return true;
}

// @p kid of @p split gets @p leaf beside it, with @p share of the split
void insertBeside(Node &split, const Node *kid, int leaf, bool before, double share)
{
  const long at = long(kid - split.kids.data()) + (before ? 0 : 1);
  for (double &x : split.ratios)
    x *= 1.0 - share;
  split.kids.insert(split.kids.begin() + at, leafNode(leaf));
  split.ratios.insert(split.ratios.begin() + at, share);
}

void applyNode(const Node &n, const QRect &r, QList<QRect> &rects, std::vector<MdiArranger::Divider> *dividers,
               std::vector<int> &path)
{
  if (n.isLeaf())
  {
    if (n.leaf >= 0)
    {
      if (rects.size() <= n.leaf)
        rects.resize(n.leaf + 1);
      rects[n.leaf] = r;
    }
    return;
  }
  const bool vert = n.orient == Qt::Vertical;
  const int k = int(n.kids.size());
  const int extent = vert ? r.height() : r.width();
  const int avail = extent - kGap * (k - 1);
  int pos = vert ? r.top() : r.left();
  const int end = pos + extent;
  for (int i = 0; i < k; ++i)
  {
    const int len = i == k - 1 ? end - pos : int(std::lround(avail * n.ratios[i]));
    const QRect cell = vert ? QRect(r.left(), pos, r.width(), len) : QRect(pos, r.top(), len, r.height());
    path.push_back(i);
    applyNode(n.kids[i], cell, rects, dividers, path);
    path.pop_back();
    pos += len;
    if (i < k - 1)
    {
      if (dividers)
        dividers->push_back({ path, i, vert ? QRect(r.left(), pos, r.width(), kGap) : QRect(pos, r.top(), kGap, r.height()),
                              avail, n.orient });
      pos += kGap;
    }
  }
}

struct Item
{
  int leaf;
  QRectF r;
};

Node derive(std::vector<Item> items, double tol)
{
  if (items.size() == 1)
    return leafNode(items.front().leaf);
  for (Qt::Orientation o : { Qt::Vertical, Qt::Horizontal })
  {
    const bool vert = o == Qt::Vertical;
    auto lo = [&](const Item &i) { return vert ? i.r.top() : i.r.left(); };
    auto hi = [&](const Item &i) { return vert ? i.r.bottom() : i.r.right(); };
    std::sort(items.begin(), items.end(), [&](const Item &a, const Item &b) { return lo(a) < lo(b); });
    std::vector<std::vector<Item>> groups;
    std::vector<double> starts, ends;
    for (const Item &it : items)
    {
      if (groups.empty() || lo(it) > ends.back() - tol)
      {
        groups.push_back({});
        starts.push_back(lo(it));
        ends.push_back(hi(it));
      }
      groups.back().push_back(it);
      ends.back() = qMax(ends.back(), hi(it));
    }
    if (groups.size() < 2)
      continue;
    // the outer edges of the span, the cuts in the middle of the gaps
    std::vector<double> b { starts.front() };
    for (size_t g = 0; g + 1 < groups.size(); ++g)
      b.push_back((ends[g] + starts[g + 1]) / 2);
    b.push_back(ends.back());
    const double total = b.back() - b.front();
    std::vector<Node> kids;
    std::vector<double> ratios;
    for (size_t g = 0; g < groups.size(); ++g)
    {
      kids.push_back(derive(groups[g], tol));
      ratios.push_back((b[g + 1] - b[g]) / total);
    }
    return splitNode(o, std::move(kids), std::move(ratios));
  }
  // overlapping beyond the tolerance: split by the centres along the longer side
  QRectF all;
  for (const Item &i : items)
    all |= i.r;
  const bool vert = all.height() > all.width();
  std::sort(items.begin(), items.end(), [&](const Item &a, const Item &b)
  {
    return vert ? a.r.center().y() < b.r.center().y() : a.r.center().x() < b.r.center().x();
  });
  const auto half = items.begin() + long(items.size() / 2);
  return splitNode(vert ? Qt::Vertical : Qt::Horizontal,
                   { derive({ items.begin(), half }, tol), derive({ half, items.end() }, tol) });
}

// ---- text form: name | H(r:node,r:node,...) | V(...)

struct Parser
{
  const QString &s;
  const QStringList &names;
  int pos = 0;
  QList<int> seen;

  bool node(Node &out)
  {
    if (pos + 1 < s.size() && (s[pos] == 'H' || s[pos] == 'V') && s[pos + 1] == '(')
    {
      out = Node();
      out.orient = s[pos] == 'H' ? Qt::Horizontal : Qt::Vertical;
      pos += 2;
      for (;;)
      {
        const int colon = s.indexOf(':', pos);
        if (colon < 0)
          return false;
        bool ok = false;
        const double r = s.mid(pos, colon - pos).toDouble(&ok);
        if (!ok || r <= 0)
          return false;
        pos = colon + 1;
        Node kid;
        if (!node(kid))
          return false;
        out.kids.push_back(kid);
        out.ratios.push_back(r);
        if (pos >= s.size())
          return false;
        if (s[pos] == ')')
        {
          ++pos;
          break;
        }
        if (s[pos] != ',')
          return false;
        ++pos;
      }
      if (out.kids.size() < 2)
        return false;
      double sum = 0;
      for (double r : out.ratios)
        sum += r;
      for (double &r : out.ratios)
        r /= sum;
      return true;
    }
    int end = pos;
    while (end < s.size() && (s[end].isLetterOrNumber() || s[end] == '_' || s[end] == '-'))
      ++end;
    const int leaf = names.indexOf(s.mid(pos, end - pos));
    if (end == pos || leaf < 0 || seen.contains(leaf))
      return false;
    seen << leaf;
    out = leafNode(leaf);
    pos = end;
    return true;
  }
};

// the cells a fixed layout is about to get, over the free windows
class Preview : public QWidget
{
public:
  using QWidget::QWidget;
  QList<QRect> cells;

protected:
  void paintEvent(QPaintEvent *) override
  {
    QPainter p(this);
    p.fillRect(rect(), QColor(0, 0, 0, 60));
    const QColor accent = palette().color(QPalette::Highlight);
    for (const QRect &c : cells)
    {
      p.fillRect(c, QColor(accent.red(), accent.green(), accent.blue(), 45));
      p.setPen(QPen(accent.lighter(130), 2.5, Qt::DashLine));
      p.drawRect(QRectF(c).adjusted(1, 1, -1, -1));
    }
  }
};
}

MdiArranger::MdiArranger(QMdiArea *area, QObject *parent)
  : QObject(parent)
  , m_area(area)
{
  // mouse events of the windows' children (Ctrl+drag, dividers) and the
  // viewport's resize go through one application-wide filter; it only looks
  // at them while they concern our area
  qApp->installEventFilter(this);
}

void MdiArranger::addWindow(QMdiSubWindow *window, Role role)
{
  m_order << window;
  m_roles << role;
  m_fits << Fit();
  setTitleBarHidden(window, m_titleBarsHidden);
  connect(window, &QObject::destroyed, this, [this, window]
  {
    const int i = m_order.indexOf(window);
    if (i >= 0)
    {
      // Fixed keeps its tree, a window that comes back finds its place
      const bool keep = m_mode == Fixed && m_hasTree;
      if (keep && findLeaf(m_root, i))
        leaveTree(i);
      m_order.removeAt(i);
      m_roles.removeAt(i);
      m_fits.removeAt(i);
      // the leaves are indices: moved up, or start over
      if (keep && m_hasTree)
        shiftLeaves(m_root, i);
      else
        m_hasTree = false;
      m_treeFor.clear();
    }
  });
  arrange();
}

void MdiArranger::setMode(Mode mode)
{
  const Mode before = m_mode;
  m_mode = mode;
  if (before != mode)
    m_savedValid = false;
  if (mode == DisplaysOnTop || mode == DisplaysOnLeft)
  {
    // the rule of the new mode
    if (before != mode)
      m_custom = false;
  }
  else if (mode == Fixed && before == Free)
  {
    // from the positions: show the cells, then snap
    const QList<int> visible = visibleWindows();
    if (!visible.isEmpty())
    {
      QList<QRect> rects;
      for (int i : visible)
        rects << m_order[i]->geometry();
      const QRect area = m_area->viewport()->rect();
      const double tol = qMax(40.0, 0.04 * qMin(area.width(), area.height()));
      m_root = deriveTree(rects, tol);
      remapLeaves(m_root, visible);
      m_hasTree = true;
      m_treeFor = visible;
      QList<QRect> cells;
      applyTree(m_root, cellArea(), cells);
      QList<QRect> shown;
      for (int i : visible)
        shown << cells.value(i);
      showPreview(shown);
      m_pending = true;   // no layout while the preview shows
      QTimer::singleShot(m_previewMs, this, [this]
      {
        if (m_preview)
          m_preview->hide();
        m_pending = false;
        if (m_mode == Fixed)
        {
          setTitleBarsHidden(true);
          doArrange();
        }
      });
      Q_EMIT changed();
      return;
    }
  }
  if (mode == Free)
  {
    m_dividers.clear();
    showDividerHint(nullptr);
  }
  setTitleBarsHidden(mode != Free);
  arrange();
  Q_EMIT changed();
}

void MdiArranger::setTitleBarsHidden(bool hidden)
{
  m_titleBarsHidden = hidden;
  for (QMdiSubWindow *w : m_order)
    setTitleBarHidden(w, hidden);
  arrange();
  Q_EMIT changed();
}

void MdiArranger::setTitleBarHidden(QMdiSubWindow *window, bool hidden)
{
  if (titleBarHidden(window) == hidden && window->property("titleBarKnown").toBool())
    return;
  const QRect g = window->geometry();
  const bool visible = window->isVisible();
  // a frameless sub-window has neither title bar nor border
  window->setWindowFlags(hidden ? Qt::FramelessWindowHint : Qt::SubWindow);
  window->setProperty("titleBarHidden", hidden);
  window->setProperty("titleBarKnown", true);
  window->setGeometry(g);
  if (visible)
  {
    window->show();
    if (!hidden)
    {
      // a window that was activated while frameless draws an empty title
      // bar until it is activated again
      QMdiSubWindow *active = m_area->activeSubWindow();
      m_area->setActiveSubWindow(window);
      if (active && active != window)
        m_area->setActiveSubWindow(active);
    }
  }
  arrange();
}

bool MdiArranger::titleBarHidden(const QMdiSubWindow *window)
{
  return window->property("titleBarHidden").toBool();
}

void MdiArranger::setContentAspect(QMdiSubWindow *window, double minAspect, double maxAspect, int extraHeight)
{
  const int i = int(m_order.indexOf(window));
  if (i < 0)
    return;
  Fit &f = m_fits[i];
  if (f.minAspect == minAspect && f.maxAspect == maxAspect && f.extraHeight == extraHeight)
    return;
  f.minAspect = minAspect;
  f.maxAspect = maxAspect;
  f.extraHeight = extraHeight;
  arrange();
}

void MdiArranger::setTableWidth(int width)
{
  if (width == m_tableWidth)
    return;
  m_tableWidth = width;
  arrange();
}

void MdiArranger::setHeld(bool held)
{
  if (held == m_held)
    return;
  m_held = held;
  arrange();
}

QString MdiArranger::layoutText() const
{
  if (!m_hasTree || m_mode == Free || ((m_mode == DisplaysOnTop || m_mode == DisplaysOnLeft) && !m_custom))
    return QString();
  QStringList names;
  for (QMdiSubWindow *w : m_order)
    names << w->objectName();
  return toText(m_root, names);
}

bool MdiArranger::setLayoutText(const QString &text)
{
  if (text.trimmed().isEmpty())
  {
    // back to the rule (Fixed: the rule's tree, kept from then on)
    m_savedValid = false;
    m_custom = false;
    m_hasTree = false;
    arrange();
    return true;
  }
  QStringList names;
  for (QMdiSubWindow *w : m_order)
    names << w->objectName();
  Node tree;
  if (!fromText(text, names, tree))
    return false;
  m_savedValid = false;
  m_root = tree;
  m_hasTree = true;
  m_treeFor = leaves(tree);
  std::sort(m_treeFor.begin(), m_treeFor.end());
  m_custom = true;
  arrange();
  return true;
}

void MdiArranger::arrange()
{
  if (m_mode == Free || m_pending)
    return;
  m_pending = true;
  QTimer::singleShot(0, this, [this]
  {
    m_pending = false;
    doArrange();
  });
}

void MdiArranger::arrangeNow()
{
  const QList<int> visible = visibleWindows();
  QList<Role> roles;
  for (int i : visible)
    roles << m_roles[i];
  const QList<QRect> rects = layout(m_area->viewport()->rect(), roles, headerHeight(), tableMinWidth(visible));
  for (int k = 0; k < visible.size(); ++k)
    m_order[visible[k]]->setGeometry(rects[k]);
}

// ---------------------------------------------------------------- the tree

// The instruments in a strip on top (or on the left) - as many lines as make
// them biggest, at most 45 % of the height (40 % of the width) when there is
// anything else - and the graph in the rest, with a table in a column on the
// right.
MdiArranger::Node MdiArranger::ruleTree(const QRect &area, const QList<Role> &roles, bool left, int headerHeight,
                                        int tableMinWidth, const QList<Fit> &fits)
{
  const QRect all = area.adjusted(kGap, kGap, -kGap, -kGap);
  QList<int> inst, graphs, tables;
  for (int i = 0; i < roles.size(); ++i)
    (roles[i] == Instrument ? inst : roles[i] == Graph ? graphs : tables) << i;

  auto line = [](Qt::Orientation o, const QList<int> &ids)
  {
    std::vector<Node> kids;
    for (int id : ids)
      kids.push_back(leafNode(id));
    return splitNode(o, std::move(kids));
  };
  auto rest = [&](const QRect &r)
  {
    if (graphs.isEmpty())
      return line(Qt::Vertical, tables);
    if (tables.isEmpty())
      return line(Qt::Vertical, graphs);
    // the table's footer (buttons, statistics) must not be squeezed, but the
    // graph keeps at least half
    const int tw = qMin(qMax(tableMinWidth, int(r.width() * 0.3)), r.width() / 2);
    const double avail = r.width() - kGap;
    return splitNode(Qt::Horizontal, { line(Qt::Vertical, graphs), line(Qt::Vertical, tables) },
                     { (avail - tw) / avail, tw / avail });
  };

  const int n = inst.size();
  if (n == 0)
    return rest(all);
  const bool onlyInst = graphs.isEmpty() && tables.isEmpty();
  const double maxShare = onlyInst ? 1.0 : left ? 0.4 : 0.45;
  int bestLines = 1;
  double bestSize = 0, bestExtent = 0;
  for (int lines = 1; lines <= n; ++lines)
  {
    const int per = (n + lines - 1) / lines;
    double size, extent;
    if (!left)
    {
      const double cw = double(all.width() - kGap * (per - 1)) / per;
      const double ch = qMin(cw / kAspect + headerHeight, (all.height() * maxShare - kGap * (lines - 1)) / lines);
      size = qMin(cw, (ch - headerHeight) * kAspect);
      extent = ch * lines + kGap * (lines - 1);
    }
    else
    {
      const double ch = double(all.height() - kGap * (per - 1)) / per;
      const double cw = qMin((ch - headerHeight) * kAspect, (all.width() * maxShare - kGap * (lines - 1)) / lines);
      size = qMin(cw, (ch - headerHeight) * kAspect);
      extent = cw * lines + kGap * (lines - 1);
    }
    if (size > bestSize * 1.05)
    {
      bestSize = size;
      bestLines = lines;
      bestExtent = extent;
    }
  }
  const int per = (n + bestLines - 1) / bestLines;
  // a line of instruments: no cell bigger than its content can fill (an LCD
  // in a tall cell leaves a hole), the rest shared by the others
  const double cross = ((onlyInst ? (left ? all.width() : all.height()) : bestExtent) - kGap * (bestLines - 1))
                       / bestLines;
  const double extent = left ? all.height() : all.width();
  auto fittedLine = [&](const QList<int> &ids)
  {
    Node node = line(left ? Qt::Vertical : Qt::Horizontal, ids);
    if (node.isLeaf() || fits.isEmpty())
      return node;
    const size_t n = size_t(ids.size());
    const double avail = extent - kGap * double(n - 1);
    std::vector<double> cap;
    for (int id : ids)
    {
      const Fit f = fits.value(id);
      double c = std::numeric_limits<double>::infinity();
      if (left && f.minAspect > 0)
        c = (cross - f.extraWidth) / f.minAspect + f.extraHeight;
      else if (!left && f.maxAspect > 0)
        c = (cross - f.extraHeight) * f.maxAspect + f.extraWidth;
      cap.push_back(qMax(c, 1.0));
    }
    // water filling: an equal share, capped; what a capped cell leaves goes
    // to the others
    std::vector<double> size(n, 0);
    std::vector<bool> fixed(n, false);
    double rest = avail;
    int open = int(n);
    for (bool changed = true; changed && open > 0;)
    {
      changed = false;
      const double share = rest / open;
      for (size_t i = 0; i < n; ++i)
        if (!fixed[i] && cap[i] < share)
        {
          size[i] = cap[i];
          fixed[i] = true;
          rest -= cap[i];
          --open;
          changed = true;
        }
    }
    for (size_t i = 0; i < n; ++i)
      if (!fixed[i])
        size[i] = rest / open;
    if (open == 0 && rest > 0)
    {
      // all full: the rest to the one that minds least - the content that
      // may be tallest (left) or widest (on top), e.g. the analog meter
      // rather than the LCD
      size_t best = 0;
      for (size_t i = 1; i < n; ++i)
        if (cap[i] > cap[best])
          best = i;
      size[best] += rest;
    }
    for (size_t i = 0; i < n; ++i)
      node.ratios[i] = size[i] / avail;
    return node;
  };
  std::vector<Node> lines;
  for (int l = 0; l < bestLines; ++l)
    lines.push_back(fittedLine(inst.mid(qsizetype(l) * per, per)));
  const Qt::Orientation outer = left ? Qt::Horizontal : Qt::Vertical;
  Node strip = splitNode(outer, std::move(lines));
  if (onlyInst)
    return strip;
  const int full = left ? all.width() : all.height();
  const int stripExtent = int(bestExtent);
  const QRect restRect = left ? all.adjusted(stripExtent + kGap, 0, 0, 0) : all.adjusted(0, stripExtent + kGap, 0, 0);
  const double avail = full - kGap;
  return splitNode(outer, { strip, rest(restRect) }, { stripExtent / avail, (avail - stripExtent) / avail });
}

void MdiArranger::applyTree(const Node &node, const QRect &area, QList<QRect> &rects, std::vector<Divider> *dividers)
{
  std::vector<int> path;
  applyNode(node, area, rects, dividers, path);
}

QList<QRect> MdiArranger::layout(const QRect &area, const QList<Role> &roles, int headerHeight, int tableMinWidth,
                                 bool left, const QList<Fit> &fits)
{
  QList<QRect> rects(roles.size());
  if (!roles.isEmpty())
    applyTree(ruleTree(area, roles, left, headerHeight, tableMinWidth, fits), area.adjusted(kGap, kGap, -kGap, -kGap),
              rects);
  return rects;
}

MdiArranger::Node MdiArranger::deriveTree(const QList<QRect> &rects, double tolerance)
{
  std::vector<Item> items;
  for (int i = 0; i < rects.size(); ++i)
    items.push_back({ i, QRectF(rects[i]) });
  return items.empty() ? Node() : derive(items, tolerance);
}

bool MdiArranger::removeLeaf(Node &node, int leaf)
{
  for (size_t i = 0; i < node.kids.size(); ++i)
  {
    if (node.kids[i].isLeaf() && node.kids[i].leaf == leaf)
    {
      const double r = node.ratios[i];
      node.kids.erase(node.kids.begin() + long(i));
      node.ratios.erase(node.ratios.begin() + long(i));
      for (double &x : node.ratios)
        x /= (1.0 - r);   // the neighbours share the space
      if (node.kids.size() == 1)
      {
        const Node only = node.kids.front();
        node = only;
      }
      return true;
    }
    if (removeLeaf(node.kids[i], leaf))
      return true;
  }
  return false;
}

void MdiArranger::addAtEdge(Node &root, int leaf, bool wide, bool first)
{
  const Qt::Orientation o = wide ? Qt::Horizontal : Qt::Vertical;
  if (root.isLeaf() || root.orient != o)
  {
    root = first ? splitNode(o, { leafNode(leaf), root }, { 0.25, 0.75 })
                 : splitNode(o, { root, leafNode(leaf) }, { 0.75, 0.25 });
  }
  else
  {
    const Node *edge = first ? &root.kids.front() : &root.kids.back();
    insertBeside(root, edge, leaf, first, 1.0 / double(root.kids.size() + 1));
  }
}

// Leaf @p leaf leaves the tree, its neighbours share the room. The first to
// go saves the tree: all who left come back to it, whatever their order.
void MdiArranger::leaveTree(int leaf)
{
  if (!m_savedValid)
  {
    m_saved = m_root;
    m_savedNames.clear();
    for (const QMdiSubWindow *w : std::as_const(m_order))
      m_savedNames << w->objectName();
    m_savedValid = true;
  }
  if (m_root.isLeaf())
    m_hasTree = false;
  else
    removeLeaf(m_root, leaf);
}

// The saved tree with the windows @p present in it; those no longer there
// leave their room to their neighbours.
void MdiArranger::restoreSaved(const QList<int> &present)
{
  QList<int> map(m_savedNames.size(), -1);
  for (int i : present)
    if (const qsizetype at = m_savedNames.indexOf(m_order[i]->objectName()); at >= 0)
      map[at] = i;
  Node tree = m_saved;
  if (keepLeaves(tree, map))
  {
    m_root = tree;
    m_hasTree = true;
  }
}

// A window new to the tree: beside the last of its kind, along the longer
// side of that cell (a graph below the last graph). With none of its kind at the edge, an instrument at the
// start, as the rules have it.
void MdiArranger::addNew(int leaf)
{
  m_savedValid = false;   // the tree is another one now
  const Role role = m_roles[leaf];
  int same = -1;
  for (int id : leaves(m_root))
    if (m_roles[id] == role)
      same = id;
  if (same < 0)
  {
    // a table goes to the right as in the rule's layout, the others
    // along the longer side
    const QRect a = m_area->viewport()->rect();
    addAtEdge(m_root, leaf, role == Table || a.width() > a.height() * 2, role == Instrument);
    return;
  }
  QList<QRect> rects;
  applyTree(m_root, cellArea(), rects);
  const QRect cell = rects.value(same);
  // graphs stack, as the rules have it: their time axes want the width
  const Qt::Orientation o = role != Graph && cell.width() >= cell.height() ? Qt::Horizontal : Qt::Vertical;
  Node *node = findLeaf(m_root, same);
  Node *split = parentOf(m_root, node);
  if (split && split->orient == o)
    insertBeside(*split, node, leaf, false, 1.0 / double(split->kids.size() + 1));
  else
    *node = splitNode(o, { leafNode(same), leafNode(leaf) });
}

MdiArranger::Node *MdiArranger::findLeaf(Node &node, int leaf)
{
  if (node.isLeaf())
    return node.leaf == leaf ? &node : nullptr;
  for (Node &k : node.kids)
    if (Node *f = findLeaf(k, leaf))
      return f;
  return nullptr;
}

QList<int> MdiArranger::leaves(const Node &node)
{
  if (node.isLeaf())
    return node.leaf >= 0 ? QList<int> { node.leaf } : QList<int>();
  QList<int> out;
  for (const Node &k : node.kids)
    out << leaves(k);
  return out;
}

QString MdiArranger::toText(const Node &node, const QStringList &names)
{
  if (node.isLeaf())
    return names.value(node.leaf);
  QStringList kids;
  for (size_t i = 0; i < node.kids.size(); ++i)
    kids << QString::number(node.ratios[i], 'g', 4) + ':' + toText(node.kids[i], names);
  return QString(node.orient == Qt::Horizontal ? "H(" : "V(") + kids.join(',') + ')';
}

bool MdiArranger::fromText(const QString &text, const QStringList &names, Node &out)
{
  const QString s = QString(text).remove(' ');
  Parser p { s, names };
  Node n;
  if (s.isEmpty() || !p.node(n) || p.pos != s.size())
    return false;
  out = n;
  return true;
}

// ---------------------------------------------------------------- layout

QList<int> MdiArranger::visibleWindows() const
{
  // isHidden(), not isVisible(): a minimized main window hides the windows
  // without closing them
  QList<int> out;
  for (int i = 0; i < m_order.size(); ++i)
    if (!m_order[i]->isHidden() && !m_order[i]->isMinimized())
      out << i;
  return out;
}

int MdiArranger::headerHeight() const
{
  // a header line inside the window, plus the title bar when it is shown
  return m_titleBarsHidden ? 22 : 48;
}

int MdiArranger::tableMinWidth(const QList<int> &visible) const
{
  int w = qMax(kTableMinWidth, m_tableWidth);
  for (int i : visible)
    if (m_roles[i] == Table)
      w = qMax(w, m_order[i]->minimumSizeHint().width());
  return w;
}

QRect MdiArranger::cellArea() const
{
  return m_area->viewport()->rect().adjusted(kGap, kGap, -kGap, -kGap);
}

MdiArranger::Node *MdiArranger::nodeAt(const std::vector<int> &path)
{
  Node *n = &m_root;
  for (int i : path)
  {
    if (i < 0 || i >= int(n->kids.size()))
      return nullptr;
    n = &n->kids[size_t(i)];
  }
  return n;
}

void MdiArranger::doArrange()
{
  m_dividers.clear();
  if (m_divHint)
    m_divHint->hide();
  if (m_mode == Free || m_held)
    return;
  const QList<int> visible = visibleWindows();
  for (int i : visible)
    if (m_order[i]->isMaximized())
      m_order[i]->showNormal();
  if (visible.isEmpty())
    return;

  auto byRule = [&](bool left)
  {
    QList<Role> roles;
    QList<Fit> fits;
    for (int i : visible)
    {
      roles << m_roles[i];
      // the frame and title bar around the content, as they are now
      Fit f = m_fits[i];
      if (QWidget *content = m_order[i]->widget())
      {
        f.extraWidth += qMax(0, m_order[i]->width() - content->width());
        f.extraHeight += qMax(0, m_order[i]->height() - content->height());
      }
      fits << f;
    }
    m_root = ruleTree(m_area->viewport()->rect(), roles, left, headerHeight(), tableMinWidth(visible), fits);
    remapLeaves(m_root, visible);
    m_hasTree = true;
  };
  if (m_mode == Fixed)
  {
    // the user's tree: vanished windows leave, returning ones find their
    // place, new ones stand beside their kind
    if (!m_hasTree && !m_savedValid)
      byRule(false);
    else
    {
      if (m_hasTree)
        for (int i : leaves(m_root))
          if (!visible.contains(i))
            leaveTree(i);
      const QList<int> inTree = m_hasTree ? leaves(m_root) : QList<int>();
      QList<int> known, coming;
      for (int i : visible)
        if (!inTree.contains(i))
          (m_savedValid && m_savedNames.contains(m_order[i]->objectName()) ? known : coming) << i;
      if (!known.isEmpty())
        restoreSaved(inTree + known);
      if (!m_hasTree && !coming.isEmpty())
      {
        m_root = leafNode(coming.takeFirst());
        m_hasTree = true;
      }
      for (int i : std::as_const(coming))
        addNew(i);
      // all back: nothing left to come
      if (m_savedValid && m_hasTree && leaves(m_root).size() == leaves(m_saved).size())
        m_savedValid = false;
    }
  }
  else if (!m_hasTree || !m_custom || visible != m_treeFor)
  {
    // the rule; a changed layout lasts until the windows change
    byRule(m_mode == DisplaysOnLeft);
    m_custom = false;
  }
  m_treeFor = visible;

  QList<QRect> rects;
  applyTree(m_root, cellArea(), rects, &m_dividers);
  for (int i : visible)
    if (i < rects.size() && rects[i].isValid())
      m_order[i]->setGeometry(rects[i]);
}

// ---------------------------------------------------------------- drag to swap

QMdiSubWindow *MdiArranger::subWindowOf(QObject *object) const
{
  for (QObject *o = object; o; o = o->parent())
  {
    if (auto *w = qobject_cast<QMdiSubWindow *>(o))
      return m_order.contains(w) ? w : nullptr;
    // a dialog of a window (Preferences) has it as its parent, but is a
    // window of its own on top of the area: neither divider nor Ctrl+drag
    if (auto *widget = qobject_cast<QWidget *>(o); widget && widget->isWindow())
      return nullptr;
  }
  return nullptr;
}

QMdiSubWindow *MdiArranger::windowAt(const QPoint &global, const QMdiSubWindow *except) const
{
  const QPoint p = m_area->viewport()->mapFromGlobal(global);
  for (int i : visibleWindows())
    if (m_order[i] != except && m_order[i]->geometry().contains(p))
      return m_order[i];
  return nullptr;
}

void MdiArranger::dragMoving(QMdiSubWindow *window, const QPoint &global)
{
  if (m_mode == Free || m_held)
    return;
  if (!m_band)
    m_band = new QRubberBand(QRubberBand::Rectangle, m_area->viewport());
  if (QMdiSubWindow *target = windowAt(global, window))
  {
    m_band->setGeometry(target->geometry());
    m_band->show();
    m_band->raise();
  }
  else
    m_band->hide();
  window->raise();
}

void MdiArranger::dragDropped(QMdiSubWindow *window, const QPoint &global)
{
  if (m_band)
    m_band->hide();
  if (m_mode == Free || m_held)
    return;
  if (QMdiSubWindow *target = windowAt(global, window); target && m_hasTree)
  {
    Node *a = findLeaf(m_root, int(m_order.indexOf(window)));
    Node *b = findLeaf(m_root, int(m_order.indexOf(target)));
    if (a && b)
    {
      std::swap(a->leaf, b->leaf);
      m_custom = true;   // swapped in place: the ratios stay
      m_savedValid = false;
    }
  }
  doArrange();   // without a target: back to its cell
}

// ---------------------------------------------------------------- dividers

const MdiArranger::Divider *MdiArranger::dividerAt(const QPoint &viewportPos, int margin) const
{
  // in the gap itself first (where two dividers meet, the grip zone of one
  // reaches into the other), then the nearest one within the margin
  const Divider *best = nullptr;
  int bestDist = margin + 1;
  for (const Divider &d : m_dividers)
  {
    const QRect &r = d.rect;
    const int dx = qMax(0, qMax(r.left() - viewportPos.x(), viewportPos.x() - r.right()));
    const int dy = qMax(0, qMax(r.top() - viewportPos.y(), viewportPos.y() - r.bottom()));
    const int dist = qMax(dx, dy);
    if (dist < bestDist)
    {
      best = &d;
      bestDist = dist;
    }
  }
  return best;
}

void MdiArranger::showDividerHint(const Divider *divider)
{
  const bool want = divider != nullptr;
  if (want != m_divCursor)
  {
    if (want)
      QApplication::setOverrideCursor(divider->orient == Qt::Vertical ? Qt::SplitVCursor : Qt::SplitHCursor);
    else
      QApplication::restoreOverrideCursor();
    m_divCursor = want;
  }
  if (!divider)
  {
    if (m_divHint)
      m_divHint->hide();
    return;
  }
  if (!m_divHint)
  {
    m_divHint = new QWidget(m_area->viewport());
    m_divHint->setAttribute(Qt::WA_TransparentForMouseEvents);
    m_divHint->setAutoFillBackground(true);
  }
  QPalette pal = m_divHint->palette();
  QColor c = m_area->palette().color(QPalette::Highlight);
  c.setAlpha(140);
  pal.setColor(QPalette::Window, c);
  m_divHint->setPalette(pal);
  // the gap, narrowed a little: it marks the line, it does not fill the gap
  const QRect r = divider->orient == Qt::Vertical ? divider->rect.adjusted(0, 1, 0, -1) : divider->rect.adjusted(1, 0, -1, 0);
  m_divHint->setGeometry(r);
  m_divHint->show();
  m_divHint->raise();
}

// The gaps between the cells split their node; they can be taken a few
// pixels into the windows, too.
bool MdiArranger::dividerEvent(QObject *watched, QEvent *event)
{
  auto *me = static_cast<QMouseEvent *>(event);
  const bool ours = watched == m_area->viewport() || subWindowOf(watched);
  if (m_divDragging)
  {
    const QPoint p = m_area->viewport()->mapFromGlobal(me->globalPosition().toPoint());
    if (event->type() == QEvent::MouseMove)
    {
      Node *n = nodeAt(m_divDrag.path);
      if (n && m_divDrag.index + 1 < int(n->ratios.size()) && m_divDrag.avail > 0)
      {
        const bool vert = m_divDrag.orient == Qt::Vertical;
        const double delta = double(vert ? p.y() - m_divLast.y() : p.x() - m_divLast.x()) / m_divDrag.avail;
        double &a = n->ratios[size_t(m_divDrag.index)];
        double &b = n->ratios[size_t(m_divDrag.index) + 1];
        const double minR = 4.0 * QFontMetrics(m_area->font()).height() / m_divDrag.avail;
        const double d = qBound(qMin(0.0, minR - a), delta, qMax(0.0, b - minR));
        a += d;
        b -= d;
        m_divLast = p;
        m_custom = true;
        m_savedValid = false;
        doArrange();
        // keep dragging the same divider (doArrange() made them anew)
        for (const Divider &dv : m_dividers)
          if (dv.path == m_divDrag.path && dv.index == m_divDrag.index)
          {
            m_divDrag = dv;
            showDividerHint(&dv);
          }
      }
      return true;
    }
    if (event->type() == QEvent::MouseButtonRelease)
    {
      m_divDragging = false;
      showDividerHint(dividerAt(p, ours ? kGrip : -1000));
      return true;
    }
    return true;
  }
  if (m_mode == Free || (me->modifiers() & Qt::ControlModifier) || m_drag || m_titleDrag)
  {
    if (m_divCursor)
      showDividerHint(nullptr);
    return false;
  }
  const QPoint p = m_area->viewport()->mapFromGlobal(me->globalPosition().toPoint());
  const Divider *d = ours ? dividerAt(p, kGrip) : nullptr;
  if (event->type() == QEvent::MouseMove && !me->buttons())
    showDividerHint(d);
  else if (event->type() == QEvent::MouseButtonPress && me->button() == Qt::LeftButton && d)
  {
    m_divDrag = *d;
    m_divDragging = true;
    m_divLast = p;
    showDividerHint(d);
    return true;
  }
  return false;
}

// ---------------------------------------------------------------- Fixed from Free

void MdiArranger::showPreview(const QList<QRect> &cells)
{
  if (m_previewMs <= 0)
    return;
  auto *preview = static_cast<Preview *>(m_preview);
  if (!preview)
  {
    preview = new Preview(m_area->viewport());
    preview->setAttribute(Qt::WA_TransparentForMouseEvents);
    m_preview = preview;
  }
  preview->cells = cells;
  preview->setGeometry(m_area->viewport()->rect());
  preview->show();
  preview->raise();
  preview->update();
}

// ---------------------------------------------------------------- events

bool MdiArranger::eventFilter(QObject *watched, QEvent *event)
{
  switch (event->type())
  {
    case QEvent::Resize:
      if (watched == m_area->viewport())
        arrange();
      break;
    case QEvent::Show:
    case QEvent::Hide:
      if (auto *w = qobject_cast<QMdiSubWindow *>(watched); w && m_order.contains(w))
        arrange();
      break;
    case QEvent::Move:
      // a window dragged at its title bar in an arranged mode
      if (m_titleDrag && watched == m_titleDrag && m_titleDrag->size() == m_titleStart.size())
        dragMoving(m_titleDrag, QCursor::pos());
      break;
    case QEvent::MouseButtonPress:
    {
      if (dividerEvent(watched, event))
        return true;
      auto *me = static_cast<QMouseEvent *>(event);
      if (me->button() != Qt::LeftButton)
        break;
      // the title bar (or the frame) of a window in an arranged mode
      if (!(me->modifiers() & Qt::ControlModifier))
      {
        if (auto *w = qobject_cast<QMdiSubWindow *>(watched); w && m_order.contains(w) && m_mode != Free)
        {
          m_titleDrag = w;
          m_titleStart = w->geometry();
        }
        break;
      }
      // Ctrl+drag moves a window, also one without title bar. The press
      // itself goes through, so a Ctrl+click still reaches the widget
      // (multiple selection in the table); the move starts after the usual
      // drag distance.
      if (m_cancelling)
        break;
      if (QMdiSubWindow *w = subWindowOf(watched))
      {
        m_pressed = w;
        if (!m_pressWidget)   // the first delivery is to the widget under the cursor
          m_pressWidget = qobject_cast<QWidget *>(watched);
        m_pressPos = me->globalPosition().toPoint();
        m_dragOffset = m_pressPos - w->pos();
        w->raise();
      }
      break;
    }
    case QEvent::MouseMove:
    {
      if (dividerEvent(watched, event))
        return true;
      auto *me = static_cast<QMouseEvent *>(event);
      const QPoint global = me->globalPosition().toPoint();
      if (m_pressed && !m_drag && (global - m_pressPos).manhattanLength() >= QApplication::startDragDistance())
      {
        m_drag = m_pressed;
        // a drag is no click: the widget gets its release far outside, so a
        // button under the press does not fire (a meter key would send a
        // command to the meter)
        if (m_pressWidget)
        {
          const QPointF outside(-100000, -100000);
          QMouseEvent release(QEvent::MouseButtonRelease, outside, m_pressWidget->mapToGlobal(outside),
                              Qt::LeftButton, Qt::NoButton, me->modifiers());
          m_cancelling = true;
          QApplication::sendEvent(m_pressWidget, &release);
          m_cancelling = false;
        }
      }
      if (m_drag)
      {
        m_drag->move(global - m_dragOffset);
        dragMoving(m_drag, global);
        return true;
      }
      break;
    }
    case QEvent::MouseButtonRelease:
    {
      if (dividerEvent(watched, event))
        return true;
      // a plain Ctrl+click keeps its release; after a drag the widget had its
      // release already
      if (m_cancelling)
        break;
      auto *me = static_cast<QMouseEvent *>(event);
      if (m_titleDrag && watched == m_titleDrag)
      {
        QMdiSubWindow *w = m_titleDrag;
        m_titleDrag = nullptr;
        const QPoint global = me->globalPosition().toPoint();
        // after the window's own release: moved -> swap or back, resized -> back
        QTimer::singleShot(0, this, [this, w, global, start = m_titleStart]
        {
          if (w->geometry() == start)
            return;
          if (w->size() == start.size())
            dragDropped(w, global);
          else
            doArrange();
        });
        break;
      }
      QMdiSubWindow *dragged = m_drag;
      m_pressed = nullptr;
      m_pressWidget = nullptr;
      m_drag = nullptr;
      if (dragged)
      {
        dragDropped(dragged, me->globalPosition().toPoint());
        return true;
      }
      break;
    }
    default:
      break;
  }
  return QObject::eventFilter(watched, event);
}
