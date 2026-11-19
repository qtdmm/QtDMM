// Copyright (c) 2026 The QtDMM developers
// SPDX-License-Identifier: GPL-3.0-or-later
#include "ui/rack/rackwindow.h"

#include <QApplication>
#include <QContextMenuEvent>
#include <QDrag>
#include <QDragEnterEvent>
#include <QHBoxLayout>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLabel>
#include <QMenu>
#include <QMimeData>
#include <QPainter>
#include <QScrollArea>
#include <QScrollBar>
#include <QTimer>
#include <QToolButton>
#include <QVBoxLayout>

#include "core/devicelibrary.h"
#include "core/siprefix.h"
#include "logic/integralnode.h"
#include "service/devicesession.h"
#include "service/metercontroller.h"
#include "service/noderunner.h"
#include "ui/designs.h"
#include "ui/frameclock.h"
#include "ui/panelframe.h"

namespace
{
const char *const kInsertMime = "application/x-qtdmm-rack-insert";

// the LED: green and lit when on, a dark green when off
QIcon ledIcon(bool on)
{
  QPixmap pm(QSize(16, 16) * qApp->devicePixelRatio());
  pm.setDevicePixelRatio(qApp->devicePixelRatio());
  pm.fill(Qt::transparent);
  QPainter p(&pm);
  p.setRenderHint(QPainter::Antialiasing);
  QRadialGradient g(QPointF(6, 6), 8);
  g.setColorAt(0, on ? QColor(170, 255, 150) : QColor(60, 80, 60));
  g.setColorAt(1, on ? QColor(30, 170, 40) : QColor(25, 40, 25));
  p.setBrush(g);
  p.setPen(QPen(QColor(20, 20, 20), 1));
  p.drawEllipse(QRectF(2, 2, 12, 12));
  return QIcon(pm);
}

QString kindName(const QString &type)
{
  if (type == QLatin1String("formula"))
    return RackWindow::tr("Formula");
  if (type == QLatin1String("generator"))
    return RackWindow::tr("Simulated");
  if (type == QLatin1String("integral"))
    return RackWindow::tr("Integral");
  return type;
}
}

RackInsert::RackInsert(DeviceSession *session, MeterController *ctl, QWidget *parent) :
  QFrame(parent), m_session(session), m_ctl(ctl)
{
  setObjectName("ui_rackInsert_" + id());
  setMinimumHeight(64);
  setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Fixed);
  auto *row = new QHBoxLayout(this);
  row->setContentsMargins(14, 10, 14, 10);
  m_led = new QToolButton(this);
  m_led->setObjectName("ui_rackLed");
  m_led->setAutoRaise(true);
  m_led->setIconSize(QSize(16, 16));
  m_led->setToolTip(tr("On or off: off, a formula gives no value, an integral holds its sum"));
  row->addWidget(m_led);
  auto *texts = new QVBoxLayout;
  texts->setSpacing(0);
  m_name = new QLabel(this);
  QFont bold = m_name->font();
  bold.setBold(true);
  m_name->setFont(bold);
  m_kind = new QLabel(this);
  QFont small = m_kind->font();
  small.setPointSizeF(small.pointSizeF() * 0.85);
  m_kind->setFont(small);
  texts->addWidget(m_name);
  texts->addWidget(m_kind);
  row->addLayout(texts, 1);
  m_values = new QLabel(this);
  m_values->setObjectName("ui_rackValues");
  m_values->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
  row->addWidget(m_values);
  m_warning = new QLabel(QStringLiteral("⚠"), this);
  m_warning->setObjectName("ui_rackWarning");
  m_warning->setStyleSheet("color: #e0a020; font-size: 16px");
  row->addWidget(m_warning);
  const NodeRunner *runner = NodeRunner::of(ctl);
  if (runner && runner->node()->type() == QLatin1String(IntegralNode::kType))
  {
    m_zero = new QToolButton(this);
    m_zero->setObjectName("ui_rackZero");
    m_zero->setText(QStringLiteral("0"));
    m_zero->setToolTip(tr("Set the integral to 0"));
    connect(m_zero, &QToolButton::clicked, this, [this]
    {
      if (NodeRunner *r = NodeRunner::of(m_ctl))
        r->reset();
    });
    row->addWidget(m_zero);
  }
  m_gear = new QToolButton(this);
  m_gear->setObjectName("ui_rackGear");
  m_gear->setAutoRaise(true);
  m_gear->setIcon(QIcon::fromTheme("configure"));
  m_gear->setToolTip(tr("Settings"));
  row->addWidget(m_gear);
  connect(m_gear, &QToolButton::clicked, this, [this] { Q_EMIT settingsRequested(id()); });
  connect(m_led, &QToolButton::clicked, this, [this]
  {
    NodeRunner *r = NodeRunner::of(m_ctl);
    if (!r)
      return;
    const bool on = !r->node()->isOn();
    r->setOn(on);
    refresh();
    Q_EMIT switched(id(), on);
  });
  refresh();
}

QString RackInsert::id() const
{
  return m_ctl ? m_ctl->deviceId() : QString();
}

QString RackInsert::inputsText() const
{
  const NodeRunner *runner = NodeRunner::of(m_ctl);
  if (!runner)
    return QString();
  const DeviceLibrary *library = m_session->library();
  auto name = [library](const QString &source)
  {
    if (library && source.startsWith(QLatin1String("device:")))
      if (const std::optional<MyDevice> d = library->find(source.mid(7)))
        return d->name;
    return source;
  };
  QStringList parts;
  const QList<ValueRef> inputs = runner->node()->inputs();
  for (int i = 0; i < inputs.size(); ++i)
  {
    if (runner->node()->takesDevices())
      parts << QString("v%1 %2").arg(i + 1).arg(name(inputs[i].source));
    else
    {
      const MeterController *from = m_session->controller(inputs[i].source);
      const QString value = from ? from->valueName(inputs[i].id) : QString();
      parts << (value.isEmpty() ? tr("%1, value %2").arg(name(inputs[i].source)).arg(inputs[i].id + 1)
                                : tr("%1: %2").arg(name(inputs[i].source), value));
    }
  }
  return parts.join(QStringLiteral("  ·  "));
}

void RackInsert::refresh()
{
  const NodeRunner *runner = NodeRunner::of(m_ctl);
  if (!m_ctl || !runner)
    return;
  const bool on = runner->node()->isOn();
  m_led->setIcon(ledIcon(on));
  m_name->setText(m_ctl->deviceName());
  m_name->setEnabled(on);
  const QString inputs = inputsText();
  m_kind->setText(inputs.isEmpty() ? kindName(runner->node()->type())
                                   : kindName(runner->node()->type()) + QStringLiteral("  ·  ") + inputs);
  // its values small, as the display shows them
  QStringList lines;
  for (int i = 0; i < m_ctl->valueCount(); ++i)
  {
    const Reading r = m_ctl->lastReading(i);
    if (r.msecs == 0 || r.noNumber())
      lines << QStringLiteral("—");
    else
      lines << QString("%1 %2").arg(r.text, SiPrefix::displayText(r.unit)).trimmed();
  }
  m_values->setText(lines.join('\n'));
  const bool waits = m_ctl->linkState() == MeterConnection::LinkState::Timeout;
  m_warning->setVisible(waits);
  m_warning->setToolTip(waits ? m_ctl->errorString() : QString());
}

void RackInsert::paintEvent(QPaintEvent *event)
{
  Q_UNUSED(event);
  // a front like the instruments': a bezel in the palette's colours
  QPainter p(this);
  PanelFrame::smooth(p);
  const QPalette &pal = palette();
  PanelFrame::Colors c { pal.color(QPalette::Light), pal.color(QPalette::Dark), pal.color(QPalette::Button),
                         QColor(Qt::transparent) };
  const QRectF r = QRectF(rect()).adjusted(1, 1, -1, -1);
  PanelFrame::paint(p, r, c, r.topLeft(), r.width());
}

void RackInsert::mousePressEvent(QMouseEvent *event)
{
  m_pressPos = event->position().toPoint();
  QFrame::mousePressEvent(event);
}

void RackInsert::mouseMoveEvent(QMouseEvent *event)
{
  // dragged by its front: to another place in the rack
  if (!(event->buttons() & Qt::LeftButton)
      || (event->position().toPoint() - m_pressPos).manhattanLength() < QApplication::startDragDistance())
    return;
  auto *drag = new QDrag(this);
  auto *mime = new QMimeData;
  mime->setData(kInsertMime, id().toUtf8());
  drag->setMimeData(mime);
  drag->setPixmap(grab().scaledToWidth(qMin(width(), 240), Qt::SmoothTransformation));
  drag->exec(Qt::MoveAction);
}

void RackInsert::contextMenuEvent(QContextMenuEvent *event)
{
  QMenu menu(this);
  QAction *settings = menu.addAction(QIcon::fromTheme("configure"), tr("&Settings..."));
  QAction *zero = m_zero ? menu.addAction(tr("Set integral to &0")) : nullptr;
  menu.addSeparator();
  QAction *remove = menu.addAction(QIcon::fromTheme("window-close"), tr("&Remove from bench"));
  QAction *chosen = menu.exec(event->globalPos());
  if (!chosen)
    return;
  if (chosen == settings)
    Q_EMIT settingsRequested(id());
  else if (zero && chosen == zero)
    m_zero->click();
  else if (chosen == remove)
    Q_EMIT removeRequested(id());
}

RackWindow::RackWindow(DeviceSession *session, QWidget *parent) :
  QWidget(parent, Qt::Tool), m_session(session)
{
  setObjectName("ui_rackWindow");
  setWindowTitle(tr("Rack"));
  setAcceptDrops(true);
  auto *top = new QVBoxLayout(this);
  auto *scroll = new QScrollArea(this);
  scroll->setWidgetResizable(true);
  scroll->setFrameShape(QFrame::NoFrame);
  // the window's design through the list: no backgrounds of their own
  scroll->setStyleSheet("QScrollArea { background: transparent }");
  scroll->viewport()->setAutoFillBackground(false);
  scroll->viewport()->setStyleSheet("background: transparent");
  m_list = new QWidget(scroll);
  m_list->setObjectName("ui_rackList");
  m_list->setAutoFillBackground(false);
  m_list->setStyleSheet("QWidget#ui_rackList { background: transparent }");
  m_listLayout = new QVBoxLayout(m_list);
  m_listLayout->setSpacing(6);
  m_empty = new QLabel(tr("No calculated values on this bench yet. \"+ Insert\" adds one."), m_list);
  m_empty->setObjectName("ui_rackEmpty");
  m_empty->setWordWrap(true);
  m_empty->setAlignment(Qt::AlignCenter);
  m_listLayout->addWidget(m_empty);
  m_listLayout->addStretch(1);
  scroll->setWidget(m_list);
  top->addWidget(scroll, 1);
  auto *add = new QToolButton(this);
  add->setObjectName("ui_rackAdd");
  add->setText(tr("+ &Insert"));
  add->setToolButtonStyle(Qt::ToolButtonTextOnly);
  add->setPopupMode(QToolButton::InstantPopup);
  auto *addMenu = new QMenu(add);
  connect(addMenu->addAction(tr("&Calculated (formula)")), &QAction::triggered, this,
          [this] { Q_EMIT addRequested(QStringLiteral("formula")); });
  connect(addMenu->addAction(tr("&Simulated")), &QAction::triggered, this,
          [this] { Q_EMIT addRequested(QStringLiteral("generator")); });
  connect(addMenu->addAction(tr("&Integral")), &QAction::triggered, this,
          [this] { Q_EMIT addRequested(QStringLiteral("integral")); });
  add->setMenu(addMenu);
  auto *bottom = new QHBoxLayout;
  bottom->addWidget(add);
  bottom->addStretch(1);
  top->addLayout(bottom);

  connect(m_session, &DeviceSession::opened, this, &RackWindow::rebuild);
  connect(m_session, &DeviceSession::closed, this, &RackWindow::rebuild);
  connect(FrameClock::instance(), &FrameClock::tick, this, [this]
  {
    // the values a few times a second are enough
    if (!m_lastRefresh.isValid() || m_lastRefresh.elapsed() >= 250)
      refresh();
  });
  resize(420, 360);
  applyDesign();
  rebuild();
}

void RackWindow::applyDesign()
{
  // the metal the front panels stand on (Silver its gradient); a design
  // without one: the area's colour
  QBrush back = Designs::frameBrush(Designs::current());
  if (back.style() == Qt::NoBrush)
    back = Designs::areaBrush(Designs::current());
  QPalette pal = palette();
  pal.setBrush(QPalette::Window, back);
  setPalette(pal);
  setAutoFillBackground(true);
  Designs::fitTexture(this, back.style() == Qt::TexturePattern);
  update();
}

QStringList RackWindow::order() const
{
  QStringList ids = m_order;
  for (const RackInsert *insert : m_inserts)
    if (!ids.contains(insert->id()))
      ids << insert->id();
  return ids;
}

void RackWindow::setOrder(const QStringList &ids)
{
  m_order = ids;
  rebuild();
}

QList<RackInsert *> RackWindow::inserts() const
{
  return m_inserts;
}

void RackWindow::rebuild()
{
  // the nodes open on the bench, in the order, new ones at the end
  QList<MeterController *> nodes;
  for (MeterController *ctl : m_session->controllers())
    if (ctl->isNode() && NodeRunner::of(ctl))
      nodes << ctl;
  std::stable_sort(nodes.begin(), nodes.end(), [this](const MeterController *a, const MeterController *b)
  {
    const qsizetype ia = m_order.indexOf(a->deviceId());
    const qsizetype ib = m_order.indexOf(b->deviceId());
    return (ia < 0 ? std::numeric_limits<qsizetype>::max() : ia) < (ib < 0 ? std::numeric_limits<qsizetype>::max() : ib);
  });
  for (RackInsert *insert : std::as_const(m_inserts))
  {
    m_listLayout->removeWidget(insert);
    insert->deleteLater();
  }
  m_inserts.clear();
  for (MeterController *ctl : std::as_const(nodes))
  {
    auto *insert = new RackInsert(m_session, ctl, m_list);
    connect(insert, &RackInsert::settingsRequested, this, &RackWindow::settingsRequested);
    connect(insert, &RackInsert::removeRequested, this, &RackWindow::removeRequested);
    connect(insert, &RackInsert::switched, this, &RackWindow::storeOn);
    m_listLayout->insertWidget(int(m_inserts.size()), insert);
    m_inserts << insert;
  }
  m_empty->setVisible(m_inserts.isEmpty());
}

void RackWindow::moveInsert(const QString &id, int index)
{
  QStringList ids = order();
  const qsizetype from = ids.indexOf(id);
  if (from < 0)
    return;
  ids.removeAt(from);
  ids.insert(qBound(0, from < index ? index - 1 : index, int(ids.size())), id);
  if (ids == order())
    return;
  m_order = ids;
  rebuild();
  Q_EMIT orderChanged();
}

void RackWindow::showInsert(const QString &id)
{
  show();
  raise();
  activateWindow();
  for (RackInsert *insert : std::as_const(m_inserts))
    if (insert->id() == id)
    {
      if (auto *scroll = qobject_cast<QScrollArea *>(m_list->parentWidget()->parentWidget()))
        scroll->ensureWidgetVisible(insert);
      // lit up a moment: where it is
      insert->setStyleSheet("RackInsert { border: 2px solid palette(highlight); border-radius: 8px }");
      QTimer::singleShot(1200, insert, [insert] { insert->setStyleSheet(QString()); });
    }
}

int RackWindow::dropIndex(int y) const
{
  for (int i = 0; i < m_inserts.size(); ++i)
    if (y < m_inserts[i]->geometry().center().y())
      return i;
  return int(m_inserts.size());
}

void RackWindow::dragEnterEvent(QDragEnterEvent *event)
{
  if (event->mimeData()->hasFormat(kInsertMime))
    event->acceptProposedAction();
}

void RackWindow::dragMoveEvent(QDragMoveEvent *event)
{
  if (event->mimeData()->hasFormat(kInsertMime))
    event->acceptProposedAction();
}

void RackWindow::dropEvent(QDropEvent *event)
{
  if (!event->mimeData()->hasFormat(kInsertMime))
    return;
  const QString id = QString::fromUtf8(event->mimeData()->data(kInsertMime));
  const QPoint at = m_list->mapFrom(this, event->position().toPoint());
  event->acceptProposedAction();
  // after the drag is over: the insert dragged is deleted by the rebuild
  QTimer::singleShot(0, this, [this, id, index = dropIndex(at.y())] { moveInsert(id, index); });
}

void RackWindow::showEvent(QShowEvent *event)
{
  QWidget::showEvent(event);
  m_open = true;
  FrameClock::instance()->want(this, true);
  refresh();
}

void RackWindow::hideEvent(QHideEvent *event)
{
  QWidget::hideEvent(event);
  // closed, it costs nothing
  FrameClock::instance()->want(this, false);
}

void RackWindow::closeEvent(QCloseEvent *event)
{
  // closed by the user (its title bar: from the window system): not open
  // with the bench next time; closed with all windows when QtDMM ends: open
  if (event->spontaneous())
    m_open = false;
  QWidget::closeEvent(event);
}

void RackWindow::refresh()
{
  m_lastRefresh.start();
  for (RackInsert *insert : std::as_const(m_inserts))
    insert->refresh();
}

void RackWindow::storeOn(const QString &id, bool on)
{
  DeviceLibrary *library = m_session->library();
  const std::optional<MyDevice> entry = library ? library->find(id) : std::nullopt;
  if (!entry || !entry->isNode())
    return;
  QJsonObject node = QJsonDocument::fromJson(entry->keys.value("Node/json").toString().toUtf8()).object();
  node["on"] = on;
  QVariantMap keys = entry->keys;
  keys.insert("Node/json", QString::fromUtf8(QJsonDocument(node).toJson(QJsonDocument::Compact)));
  library->update(id, keys);
}
