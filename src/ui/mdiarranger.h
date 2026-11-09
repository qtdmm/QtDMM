// Copyright (c) 2026 The QtDMM developers
// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <QList>
#include <QObject>
#include <QPointer>
#include <QPoint>
#include <QRect>
#include <QStringList>

#include <vector>

class QMdiArea;
class QMdiSubWindow;
class QRubberBand;
class QWidget;

/// Places the sub-windows of a QMdiArea.
///
/// Every arrangement is a split tree: a node divides its rect horizontally or
/// vertically among its children by ratios, a leaf holds one window. The gaps
/// between the cells are dividers the user can drag.
///
/// - "Displays on top" / "Displays on the left" build the tree by a rule: the
///   instruments share a strip, the graph takes the rest and a table gets a
///   narrower column. The rule is redone when the area is resized or a window
///   is shown or hidden - until the user drags a divider or swaps two
///   windows; then the tree stays (and scales) until the set of windows
///   changes.
/// - "Fixed" keeps its tree for good: taken over from an automatic mode, or
///   derived from the window positions in Free mode. A hidden or closed
///   window leaves its space to its neighbours and gets its place back when
///   it returns under its object name, unless the tree changed meanwhile. A
///   new one stands beside the last of its kind, or at the edge when it is
///   the first.
/// - "Free" is the classic MDI.
///
/// Ctrl+drag moves a window, also one without title bar. In the arranged
/// modes a window dragged (with Ctrl or at its title bar) onto another one
/// swaps places with it; dropped elsewhere it snaps back.
///
/// Knows nothing about QtDMM's windows or settings: it only sees
/// sub-windows, their roles and object names, so it can move on unchanged.
class MdiArranger : public QObject
{
  Q_OBJECT
public:
  enum Mode { DisplaysOnTop, DisplaysOnLeft, Fixed, Free };
  enum Role { Instrument, Graph, Table };

  /// The width/height range a window's content can fill, and the pixels
  /// around it (frame, title bar, a bar below the content). Outside the
  /// range the content would leave empty room in its cell.
  struct Fit
  {
    double minAspect = 0;   ///< 0: no limit
    double maxAspect = 0;   ///< 0: no limit
    int extraWidth = 0;
    int extraHeight = 0;
  };

  /// A node of the split tree. A leaf (no kids) holds the index of a window
  /// (in the order added, or in the role list of the static functions).
  struct Node
  {
    int leaf = -1;
    Qt::Orientation orient = Qt::Vertical;   ///< Vertical: kids top to bottom
    std::vector<Node> kids;
    std::vector<double> ratios;              ///< parallel to kids, sum 1
    bool isLeaf() const { return kids.empty(); }
  };
  /// The gap between two cells of a split node.
  struct Divider
  {
    std::vector<int> path;   ///< kid indices from the root to the split node
    int index = 0;           ///< between kids[index] and kids[index + 1]
    QRect rect;
    int avail = 0;           ///< pixels the node's ratios share
    Qt::Orientation orient = Qt::Vertical;
  };

  explicit MdiArranger(QMdiArea *area, QObject *parent = nullptr);

  /// Takes part in the arrangement, in the order added.
  void addWindow(QMdiSubWindow *window, Role role);

  Mode mode() const { return m_mode; }
  /// Switches the mode. The arranged modes hide the title bars, Free shows
  /// them again (both can be changed afterwards). Fixed from an automatic
  /// mode keeps its tree; from Free it derives one from the positions and
  /// shows it for a moment before the windows snap into it.
  void setMode(Mode mode);

  /// Title bars of all windows (true = hidden).
  bool titleBarsHidden() const { return m_titleBarsHidden; }
  void setTitleBarsHidden(bool hidden);
  /// Title bar of one window.
  void setTitleBarHidden(QMdiSubWindow *window, bool hidden);
  static bool titleBarHidden(const QMdiSubWindow *window);
  /// The aspect range of @p window's content (see Fit): in a line of
  /// instruments a cell gets no more room than its content can fill, the
  /// rest goes to the others. @p extraHeight: pixels below or above the
  /// content (the meter keys); the frame is measured.
  void setContentAspect(QMdiSubWindow *window, double minAspect, double maxAspect, int extraHeight = 0);
  /// The table column in the automatic modes is at least this wide (still
  /// at most half the area): the width the main window grew by for it.
  void setTableWidth(int width);
  /// Held: arranges nothing and leaves the windows where they are (the
  /// start page over them, the project's window placed beside it by hand);
  /// let go, it arranges again.
  void setHeld(bool held);

  /// The tree worth keeping as text over the windows' object names - in
  /// Fixed mode, or once the user changed an automatic layout; empty
  /// otherwise (the rule applies).
  QString layoutText() const;
  /// Takes a tree saved with layoutText(). An automatic mode uses it while
  /// exactly its windows are shown; an empty text goes back to the rule.
  /// False (and nothing changes) when the text does not parse or names an
  /// unknown window.
  bool setLayoutText(const QString &text);

  /// Milliseconds Fixed from Free shows the new cells before they apply.
  void setPreviewTime(int ms) { m_previewMs = ms; }

  /// @name The layout engine, static for the tests
  /// @{
  /// The rule tree for the windows with @p roles inside @p area (viewport
  /// coordinates); @p left: the instruments on the left instead of on top.
  static Node ruleTree(const QRect &area, const QList<Role> &roles, bool left, int headerHeight,
                       int tableMinWidth = 260, const QList<Fit> &fits = {});
  /// Lays @p node out in @p area: the rect of each leaf (indexed by leaf)
  /// and, if wanted, the dividers.
  static void applyTree(const Node &node, const QRect &area, QList<QRect> &rects,
                        std::vector<Divider> *dividers = nullptr);
  /// The rects the automatic mode gives the windows with @p roles.
  static QList<QRect> layout(const QRect &area, const QList<Role> &roles, int headerHeight,
                             int tableMinWidth = 260, bool left = false, const QList<Fit> &fits = {});
  /// A tree from hand-placed rects: cut where the windows (with @p tolerance)
  /// leave a gap, the cut in the middle of the gap. Leaves are the indices
  /// of @p rects.
  static Node deriveTree(const QList<QRect> &rects, double tolerance);
  /// Removes a leaf; its neighbours share its space.
  static bool removeLeaf(Node &node, int leaf);
  /// Adds a leaf in a new cell at the bottom (or, @p wide, the right) edge;
  /// with @p first at the top (or left).
  static void addAtEdge(Node &root, int leaf, bool wide, bool first = false);
  static Node *findLeaf(Node &node, int leaf);
  static QList<int> leaves(const Node &node);
  /// Text form: a name, or H(ratio:child,ratio:child,...) / V(...).
  static QString toText(const Node &node, const QStringList &names);
  static bool fromText(const QString &text, const QStringList &names, Node &out);
  /// @}

public Q_SLOTS:
  /// Lays the windows out again (deferred, once per event loop pass).
  void arrange();
  /// Lays the windows out by the rule of "Displays on top" now, in any
  /// mode - a start in Free mode seeds the positions with it.
  void arrangeNow();

Q_SIGNALS:
  /// The mode or the title bars changed.
  void changed();

protected:
  bool eventFilter(QObject *watched, QEvent *event) override;

private:
  void leaveTree(int leaf);
  void restoreSaved(const QList<int> &present);
  void addNew(int leaf);

  void doArrange();
  QMdiSubWindow *subWindowOf(QObject *object) const;
  QList<int> visibleWindows() const;
  int headerHeight() const;
  int tableMinWidth(const QList<int> &visible) const;
  QRect cellArea() const;
  Node *nodeAt(const std::vector<int> &path);
  // drag to swap
  QMdiSubWindow *windowAt(const QPoint &global, const QMdiSubWindow *except) const;
  void dragMoving(QMdiSubWindow *window, const QPoint &global);
  void dragDropped(QMdiSubWindow *window, const QPoint &global);
  // dividers
  const Divider *dividerAt(const QPoint &viewportPos, int margin) const;
  bool dividerEvent(QObject *watched, QEvent *event);
  void showDividerHint(const Divider *divider);
  // Fixed from Free
  void showPreview(const QList<QRect> &cells);

  QMdiArea *m_area;
  Mode m_mode = DisplaysOnTop;
  bool m_titleBarsHidden = true;
  QList<QMdiSubWindow *> m_order;
  QList<Role> m_roles;             ///< parallel to m_order
  QList<Fit> m_fits;               ///< parallel to m_order
  bool m_pending = false;
  int m_tableWidth = 0;            ///< see setTableWidth()
  bool m_held = false;             ///< see setHeld()

  Node m_root;
  bool m_hasTree = false;
  QList<int> m_treeFor;            ///< the windows m_root was made for (sorted)
  Node m_saved;                    ///< Fixed: the tree when the first window left
  QStringList m_savedNames;        ///< the windows m_saved's leaves stand for
  bool m_savedValid = false;       ///< a window is away; changing the tree ends it
  bool m_custom = false;           ///< automatic mode: the user changed the tree
  std::vector<Divider> m_dividers;

  // Ctrl+drag
  QMdiSubWindow *m_pressed = nullptr; ///< Ctrl+press on this window, not moved yet
  QPointer<QWidget> m_pressWidget;    ///< the widget that got that press
  bool m_cancelling = false;          ///< sending the widget its cancelling release
  QMdiSubWindow *m_drag = nullptr;    ///< a window being moved with Ctrl+drag
  QPoint m_pressPos;                  ///< global cursor position of the press
  QPoint m_dragOffset;                ///< cursor - window position
  // title bar drag in the arranged modes
  QMdiSubWindow *m_titleDrag = nullptr;
  QRect m_titleStart;
  QRubberBand *m_band = nullptr;      ///< marks the swap target

  // divider drag
  bool m_divDragging = false;
  Divider m_divDrag;
  QPoint m_divLast;
  bool m_divCursor = false;           ///< the split cursor is set
  QWidget *m_divHint = nullptr;       ///< highlights the divider under the mouse

  QWidget *m_preview = nullptr;
  int m_previewMs = 900;
};
