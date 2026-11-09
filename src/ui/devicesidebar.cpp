// Copyright (c) 2026 The QtDMM developers
// SPDX-License-Identifier: GPL-3.0-or-later
#include "ui/devicesidebar.h"

#include <cmath>

#include <QApplication>
#include <QClipboard>
#include <QContextMenuEvent>
#include <QDesktopServices>
#include <QDir>
#include <QDropEvent>
#include <QFileInfo>
#include <QHeaderView>
#include <QKeyEvent>
#include <QMenu>
#include <QMimeData>
#include <QMessageBox>
#include <QPainter>
#include <QStyledItemDelegate>
#include <QTimer>
#include <QUrl>

#include "core/devicelibrary.h"
#include "device/dmmdecoder.h"
#include "ui/devicesymbol.h"

namespace
{
// a device's picture (DeviceSymbol), kept with its item
constexpr int kSymbol = Qt::UserRole + 2;
// a value row: the value's Reading::id
constexpr int kValueId = Qt::UserRole + 3;
// a device: its model and where it is connected, the start of its tooltip
constexpr int kWhere = Qt::UserRole + 4;
// one picture before each name: the device's in My devices, the state's dot
// on the bench and at the services, none (but its room) on a value row
constexpr int kDotSize = 12;
constexpr int kSymbolSize = 20;
const QSize kIconSize(kSymbolSize, kSymbolSize);
// the columns: the name, where a value is shown or a service's port; the
// one between is hidden (the address to copy)
constexpr int kShown = 2;
constexpr int kMark = 13;   // one mark of kShown
// a row's marks of kShown (shownMarks())
constexpr int kMarks = Qt::UserRole + 5;

// the state of a device on the bench or a service: readings coming in or
// not; invalid: an empty picture that keeps the room. At the right of its
// room, so the name follows it closely. Selected the same, not tinted
QIcon dot(const QColor &color, qreal dpr)
{
  QPixmap pixmap(kIconSize * dpr);
  pixmap.setDevicePixelRatio(dpr);
  pixmap.fill(Qt::transparent);
  QPainter p(&pixmap);
  p.setRenderHint(QPainter::Antialiasing);
  if (color.isValid())
  {
    const double top = (kIconSize.height() - kDotSize) / 2.0;
    p.setPen(Qt::NoPen);
    p.setBrush(color);
    p.drawEllipse(QRectF(kIconSize.width() - kDotSize + 1, top + 1, kDotSize - 2, kDotSize - 2));
  }
  p.end();
  QIcon icon;
  icon.addPixmap(pixmap, QIcon::Normal);
  icon.addPixmap(pixmap, QIcon::Selected);
  return icon;
}

// where a value is shown, as a legend: a stroke of its curve's colour
// (not a dot, unlike the states before the names), then one symbol for each
// kind of view: front panel, needle meter, Poincaré plot (the tooltip names
// the windows)
QPixmap shownMarks(const DeviceSidebar::Value &value, qreal dpr)
{
  QList<QIcon> views;
  if (value.onPanel)
    views << QIcon::fromTheme("qtdmm-display");
  if (value.meters > 0)
    views << QIcon::fromTheme("qtdmm-meter");
  if (value.poincares > 0)
    views << QIcon::fromTheme("qtdmm-poincare");
  const int marks = int(views.size()) + (value.color.isValid() ? 1 : 0);
  if (marks == 0)
    return QPixmap();
  QPixmap pixmap(QSize(marks * (kMark + 1), kMark) * dpr);
  pixmap.setDevicePixelRatio(dpr);
  pixmap.fill(Qt::transparent);
  QPainter p(&pixmap);
  p.setRenderHint(QPainter::Antialiasing);
  int x = 0;
  if (value.color.isValid())
  {
    p.setPen(QPen(value.color, 3, Qt::SolidLine, Qt::RoundCap));
    p.drawLine(QPointF(x + 2, kMark / 2.0), QPointF(x + kMark - 2, kMark / 2.0));
    x += kMark + 1;
  }
  for (const QIcon &icon : std::as_const(views))
  {
    icon.paint(&p, QRect(x, 0, kMark, kMark));
    x += kMark + 1;
  }
  return pixmap;
}

// a recent bench's star: filled for a favourite, an outline to click else
QPixmap star(bool favorite, const QColor &outline, qreal dpr)
{
  QPixmap pixmap(QSize(kMark + 1, kMark) * dpr);
  pixmap.setDevicePixelRatio(dpr);
  pixmap.fill(Qt::transparent);
  QPainter p(&pixmap);
  p.setRenderHint(QPainter::Antialiasing);
  QPolygonF shape;
  for (int i = 0; i < 10; ++i)
  {
    const double r = (i % 2 ? 0.42 : 1.0) * (kMark / 2.0 - 0.5);
    const double a = -M_PI / 2 + i * M_PI / 5;
    shape << QPointF(kMark / 2.0 + r * std::cos(a), kMark / 2.0 + 0.5 + r * std::sin(a));
  }
  p.setPen(QPen(favorite ? QColor(0xc8, 0x96, 0x10) : outline, 1));
  p.setBrush(favorite ? QColor(0xf0, 0xb8, 0x20) : QColor(Qt::transparent));
  p.drawPolygon(shape);
  return pixmap;
}

// the marks at their own size: an icon would be scaled to the list's
// icon size, made for one picture
class MarksDelegate : public QStyledItemDelegate
{
public:
  using QStyledItemDelegate::QStyledItemDelegate;
  void paint(QPainter *p, const QStyleOptionViewItem &option, const QModelIndex &index) const override
  {
    QStyleOptionViewItem plain(option);
    initStyleOption(&plain, index);
    option.widget->style()->drawPrimitive(QStyle::PE_PanelItemViewItem, &plain, p, option.widget);
    const QPixmap marks = index.data(kMarks).value<QPixmap>();
    if (marks.isNull())
    {
      // a service's port
      QStyledItemDelegate::paint(p, option, index);
      return;
    }
    const QSizeF size = marks.deviceIndependentSize();
    p->drawPixmap(QPointF(option.rect.left() + 2, option.rect.center().y() - size.height() / 2.0 + 1), marks);
  }
  QSize sizeHint(const QStyleOptionViewItem &option, const QModelIndex &index) const override
  {
    const QPixmap marks = index.data(kMarks).value<QPixmap>();
    const QSize base = QStyledItemDelegate::sizeHint(option, index);
    return marks.isNull() ? base : QSize(int(marks.deviceIndependentSize().width()) + 4, base.height());
  }
};

// the picture of the model an entry of My devices has; "unknown" for
// manual settings
QPixmap symbolOf(const MyDevice &d, qreal dpr)
{
  const QSize box(kSymbolSize, kSymbolSize);
  for (const DmmDecoder::DMMInfo &info : DmmDecoder::getDeviceConfigurations())
    if (DmmDecoder::sameModel(d.model(), info.name))
      return DeviceSymbol::pixmap(info.vendor, DeviceType::of(info), box, dpr, DeviceSymbol::Design::Flat);
  return DeviceSymbol::unknown(box, dpr, DeviceSymbol::Design::Flat);
}
}

DeviceSidebar::DeviceSidebar(DeviceLibrary *library, QWidget *parent)
  : QTreeWidget(parent)
  , m_library(library)
{
  setObjectName("ui_deviceSidebar");
  setHeaderHidden(true);
  // the name, a service's port and where a value is shown; no readings and
  // no states: they flickered or ran long, and the front panels have them
  setColumnCount(3);
  header()->setStretchLastSection(false);
  header()->setSectionResizeMode(0, QHeaderView::Stretch);
  header()->hideSection(1);
  header()->setSectionResizeMode(kShown, QHeaderView::ResizeToContents);
  setItemDelegateForColumn(kShown, new MarksDelegate(this));
  setRootIsDecorated(true);
  setIconSize(kIconSize);
  setDragDropMode(QAbstractItemView::InternalMove);
  setSelectionMode(QAbstractItemView::SingleSelection);
  // renamed with F2 (keyPressEvent) or the context menu, not by a click
  setEditTriggers(QAbstractItemView::NoEditTriggers);
  // only the node takes entries: they cannot become children of each other
  invisibleRootItem()->setFlags(Qt::ItemIsEnabled);
  // folded at the start: one look away, not in the way of the devices
  // the open project with its benches, at the top; hidden without one
  m_project = new QTreeWidgetItem(this, { tr("Project") });
  m_project->setFlags(Qt::ItemIsEnabled);
  {
    QFont boldFont = m_project->font(0);
    boldFont.setBold(true);
    m_project->setFont(0, boldFont);
  }
  m_project->setFirstColumnSpanned(true);
  m_project->setHidden(true);
  m_workspaces = new QTreeWidgetItem(this, { tr("Recent benches") });
  m_workspaces->setFlags(Qt::ItemIsEnabled);
  QFont bold = m_workspaces->font(0);
  bold.setBold(true);
  m_workspaces->setFont(0, bold);
  m_workspaces->setFirstColumnSpanned(true);
  m_workspaces->setToolTip(0, tr("The benches used last: a double click loads one."));
  m_workspaces->setHidden(true);
  m_devices = new QTreeWidgetItem(this, { tr("My devices") });
  m_devices->setFlags(Qt::ItemIsEnabled | Qt::ItemIsDropEnabled);
  m_devices->setFont(0, bold);
  m_devices->setFirstColumnSpanned(true);
  // folded at the start: the devices in use are under "Connected devices"
  m_devices->setExpanded(false);
  m_open = new QTreeWidgetItem(this, { tr("Bench") });
  // an entry dragged here connects, one dragged back to My devices closes
  m_open->setFlags(Qt::ItemIsEnabled | Qt::ItemIsDropEnabled);
  m_open->setFont(0, bold);
  m_open->setFirstColumnSpanned(true);
  m_open->setToolTip(0, tr("The devices this window reads: each has its front panel and records with the others."));
  m_open->setExpanded(true);
  m_services = new QTreeWidgetItem(this, { tr("Services") });
  m_services->setFlags(Qt::ItemIsEnabled);
  m_services->setFont(0, bold);
  m_services->setFirstColumnSpanned(true);
  m_services->setToolTip(0, tr("What this window offers to other programs over the network."));
  m_services->setExpanded(true);

  // a click on a device does nothing (its context menu connects it), on the
  // last row it adds one
  connect(this, &QTreeWidget::itemClicked, this, [this](QTreeWidgetItem *item, int column)
  {
    if (!item)
      return;
    if (item == m_addItem)
      Q_EMIT addDeviceRequested();
    else if (item == m_newBenchItem)
      Q_EMIT newBenchRequested();
    else if (item->parent() == m_workspaces && column == kShown)
      Q_EMIT favoriteRequested(item->data(0, Qt::UserRole).toString(), !item->data(0, Qt::UserRole + 1).toBool());
    else if (column == kShown && !item->data(kShown, kMarks).isNull())
      Q_EMIT viewsHighlightRequested(valueOf(item));
  });
  connect(this, &QTreeWidget::itemDoubleClicked, this, [this](QTreeWidgetItem *item)
  {
    const QString id = idOf(item);
    if (!id.isEmpty())
    {
      Q_EMIT settingsRequested(id);
      return;
    }
    if (item && item->parent() == m_services)
    {
      Q_EMIT serviceSettingsRequested();
      return;
    }
    // a device on the bench or one of its values: its front panel
    if (const QString open = openOf(item && item->data(0, kValueId).isValid() ? item->parent() : item); !open.isEmpty())
    {
      Q_EMIT frontPanelRequested(open);
      return;
    }
    if (const QString file = workspaceOf(item); !file.isEmpty())
    {
      Q_EMIT workspaceRequested(file);
      return;
    }
  });
  // renamed in place
  connect(this, &QTreeWidget::itemChanged, this, [this](QTreeWidgetItem *item)
  {
    if (m_filling)
      return;
    const QString id = idOf(item);
    if (id.isEmpty())
      return;
    const std::optional<MyDevice> d = m_library->find(id);
    const QString name = item->text(0).trimmed();
    if (d && !name.isEmpty() && name != d->name)
      m_library->rename(id, name);
    else if (d)
    {
      QSignalBlocker block(this);
      item->setText(0, d->name);
    }
  });
  // later: a change in the middle of a drop must not take the items away
  connect(m_library, &DeviceLibrary::changed, this, &DeviceSidebar::fill, Qt::QueuedConnection);
  fill();
}

QString DeviceSidebar::idOf(const QTreeWidgetItem *item) const
{
  return item && item != m_addItem && item->parent() == m_devices ? item->data(0, Qt::UserRole).toString() : QString();
}

QString DeviceSidebar::openOf(const QTreeWidgetItem *item) const
{
  return item && item->parent() == m_open ? item->data(0, Qt::UserRole).toString() : QString();
}

QString DeviceSidebar::chosenDevice() const
{
  const QTreeWidgetItem *item = currentItem();
  if (!item || !item->isSelected())
    return QString();
  if (const QString id = idOf(item); !id.isEmpty())
    return id;
  return openOf(item->data(0, kValueId).isValid() ? item->parent() : item);
}

QTreeWidgetItem *DeviceSidebar::openItem(const QString &id) const
{
  for (int i = 0; i < m_open->childCount(); ++i)
    if (m_open->child(i)->data(0, Qt::UserRole).toString() == id)
      return m_open->child(i);
  return nullptr;
}

void DeviceSidebar::setOpenDevices(const QList<OpenDevice> &devices)
{
  // a name being typed (Rename) would go: the next update, once it is done
  if (state() == QAbstractItemView::EditingState)
    return;
  m_openList = devices;
  m_filling = true;
  QStringList ids, shown;
  for (const OpenDevice &device : devices)
    ids << device.id;
  for (int i = 0; i < m_open->childCount(); ++i)
    shown << m_open->child(i)->data(0, Qt::UserRole).toString();
  if (ids != shown)
  {
    const QString selected = openOf(currentItem());
    qDeleteAll(m_open->takeChildren());
    for (const OpenDevice &device : devices)
    {
      auto *item = new QTreeWidgetItem(m_open, { device.name });
      item->setData(0, Qt::UserRole, device.id);
      item->setFlags(Qt::ItemIsEnabled | Qt::ItemIsSelectable | Qt::ItemIsDragEnabled);
      item->setFirstColumnSpanned(true);   // no marks: its values have them
      item->setExpanded(true);
      if (device.id == selected)
        setCurrentItem(item);
    }
  }
  for (int i = 0; i < devices.size(); ++i)
  {
    const OpenDevice &device = devices[i];
    QTreeWidgetItem *item = m_open->child(i);
    item->setText(0, device.name);
    item->setToolTip(0, device.name);
    item->setForeground(0, device.connected ? palette().text() : palette().brush(QPalette::Disabled, QPalette::Text));
    // connected: green while readings come in, grey while they do not
    item->setIcon(0, dot(device.active      ? QColor(0x22, 0xaa, 0x22)
                         : device.connected ? palette().color(QPalette::Disabled, QPalette::WindowText)
                                            : QColor(),
                         devicePixelRatioF()));
    fillValues(item, 0, device.values);
  }
  updateMarks();
  m_filling = false;
}

void DeviceSidebar::fillValues(QTreeWidgetItem *parent, int first, const QList<Value> &values)
{
  const bool filling = m_filling;
  m_filling = true;
  // a row for each value, also for a single one: where it is shown, and
  // what is dragged onto a view
  const int rows = int(values.size());
  while (parent->childCount() > first + rows)
    delete parent->takeChild(parent->childCount() - 1);
  for (int i = 0; i < rows; ++i)
  {
    const Value &value = values[i];
    QTreeWidgetItem *item = parent->child(first + i);
    if (!item)
    {
      item = new QTreeWidgetItem(parent);
      // dragged onto a view of the window (mimeData())
      item->setFlags(Qt::ItemIsEnabled | Qt::ItemIsSelectable | Qt::ItemIsDragEnabled);
      // the room of a picture: the name one step in from its device's
      item->setIcon(0, dot(QColor(), devicePixelRatioF()));
    }
    item->setData(0, kValueId, value.id);
    // its name with the unit ("Carbon dioxide (ppm)"), the number in the tooltip
    item->setText(0, value.name);
    item->setToolTip(0, tr("Value %1: %2").arg(value.id + 1).arg(value.name));
    const QPixmap marks = shownMarks(value, devicePixelRatioF());
    item->setData(kShown, kMarks, marks.isNull() ? QVariant() : QVariant(marks));
    QString where = tr("Where it is shown; a click points to it");
    if (!value.views.isEmpty())
      where += '\n' + value.views.join('\n');
    item->setToolTip(kShown, where);
  }
  m_filling = filling;
}

ValueRef DeviceSidebar::valueOf(const QTreeWidgetItem *item) const
{
  const QTreeWidgetItem *row = item && item->data(0, kValueId).isValid() ? item->parent() : item;
  if (!row || row->parent() != m_open)
    return ValueRef(0, "-");
  const QString device = row->data(0, Qt::UserRole).toString();
  const QString source = "device:" + device;
  if (item != row)
    return ValueRef(item->data(0, kValueId).toInt(), source);
  // a device row stands for none: its values have rows of their own
  return ValueRef(0, "-");
}

QTreeWidgetItem *DeviceSidebar::valueItem(int id, const QString &device) const
{
  const QTreeWidgetItem *row = openItem(device);
  for (int i = 0; row && i < row->childCount(); ++i)
    if (row->child(i)->data(0, kValueId).isValid() && row->child(i)->data(0, kValueId).toInt() == id)
      return row->child(i);
  return nullptr;
}

void DeviceSidebar::setServices(const QList<Service> &services)
{
  if (state() == QAbstractItemView::EditingState)
    return;
  m_serviceList = services;
  m_filling = true;
  while (m_services->childCount() > services.size())
    delete m_services->takeChild(m_services->childCount() - 1);
  const QColor grey = palette().color(QPalette::Disabled, QPalette::WindowText);
  for (int i = 0; i < services.size(); ++i)
  {
    const Service &service = services[i];
    QTreeWidgetItem *item = m_services->child(i);
    if (!item)
    {
      item = new QTreeWidgetItem(m_services);
      item->setFlags(Qt::ItemIsEnabled | Qt::ItemIsSelectable);
      item->setTextAlignment(kShown, Qt::AlignRight | Qt::AlignVCenter);
    }
    item->setData(0, Qt::UserRole, service.id);
    item->setData(1, Qt::UserRole, service.address);
    item->setText(0, service.name);
    item->setText(kShown, service.value);
    item->setToolTip(0, service.tooltip);
    item->setToolTip(kShown, service.tooltip);
    item->setForeground(0, service.running ? palette().color(QPalette::Text) : grey);
    item->setForeground(kShown, service.running ? palette().color(QPalette::Text) : grey);
    // green while a client is connected, dark green while it runs and waits
    // for one; red: switched on but not running (port taken)
    item->setIcon(0, dot(service.active    ? QColor(0x22, 0xaa, 0x22)
                         : service.failed  ? QColor(0xcc, 0x22, 0x22)
                         : service.running ? QColor(0x1a, 0x5c, 0x1a)
                                           : QColor(), devicePixelRatioF()));
  }
  m_filling = false;
}

QString DeviceSidebar::workspaceOf(const QTreeWidgetItem *item) const
{
  if (!item || (item->parent() != m_workspaces && item->parent() != m_project) || item == m_newBenchItem)
    return QString();
  const QString file = item->data(0, Qt::UserRole).toString();
  return QFileInfo::exists(file) ? file : QString();
}

void DeviceSidebar::setWorkspaces(const QList<Workspace> &workspaces)
{
  m_workspaceList = workspaces;
  m_filling = true;
  qDeleteAll(m_workspaces->takeChildren());
  const QColor grey = palette().color(QPalette::Disabled, QPalette::WindowText);
  for (const Workspace &ws : workspaces)
  {
    const QFileInfo info(ws.file);
    auto *item = new QTreeWidgetItem(m_workspaces, { ws.name.isEmpty() ? info.completeBaseName() : ws.name });
    item->setData(0, Qt::UserRole, ws.file);
    item->setData(0, Qt::UserRole + 1, ws.favorite);
    item->setFlags(Qt::ItemIsEnabled | Qt::ItemIsSelectable);
    // the star at the right: a click makes it a favourite or no more
    item->setData(kShown, kMarks, star(ws.favorite, grey, devicePixelRatioF()));
    item->setToolTip(kShown, ws.favorite ? tr("Favourite: stays on the list. A click takes the star away.")
                                         : tr("A click makes it a favourite: first on the list, never dropped."));
    const QString path = QDir::toNativeSeparators(ws.file);
    if (info.exists())
      item->setToolTip(0, ws.tooltip.isEmpty() ? path : ws.tooltip);
    else
    {
      item->setForeground(0, grey);
      item->setToolTip(0, tr("%1 (not found)").arg(path));
    }
  }
  m_workspaces->setHidden(workspaces.isEmpty());
  m_filling = false;
}

void DeviceSidebar::setProject(const QString &name, const QList<Workspace> &benches, const QString &current)
{
  m_filling = true;
  qDeleteAll(m_project->takeChildren());
  m_newBenchItem = nullptr;
  m_project->setHidden(name.isEmpty());
  if (name.isEmpty())
  {
    m_filling = false;
    return;
  }
  m_project->setText(0, tr("Project: %1").arg(name));
  m_project->setToolTip(0, tr("The benches of the open project: a double click loads one."));
  for (const Workspace &ws : benches)
  {
    const QString shown = ws.name.isEmpty() ? QFileInfo(ws.file).completeBaseName() : ws.name;
    auto *item = new QTreeWidgetItem(m_project, { shown });
    item->setData(0, Qt::UserRole, ws.file);
    item->setFlags(Qt::ItemIsEnabled | Qt::ItemIsSelectable);
    item->setFirstColumnSpanned(true);
    item->setToolTip(0, ws.tooltip.isEmpty() ? QDir::toNativeSeparators(ws.file) : ws.tooltip);
    if (ws.file == current)
    {
      QFont f = item->font(0);
      f.setBold(true);
      item->setFont(0, f);
    }
  }
  m_newBenchItem = new QTreeWidgetItem(m_project, { tr("+ New bench...") });
  m_newBenchItem->setFlags(Qt::ItemIsEnabled);
  m_newBenchItem->setFirstColumnSpanned(true);
  m_newBenchItem->setForeground(0, palette().color(QPalette::Disabled, QPalette::WindowText));
  m_newBenchItem->setToolTip(0, tr("Save the devices and windows as they are as a new bench of the project"));
  m_project->setExpanded(true);
  m_filling = false;
}

void DeviceSidebar::setCurrentWorkspace(const QString &name, const QString &tooltip)
{
  // after the node of the devices it is about, in the name's own cell: in a
  // column of its own it widened that for every row
  m_open->setText(0, name.isEmpty() ? tr("Bench") : tr("Bench · %1").arg(name));
  m_open->setToolTip(0, tooltip.isEmpty() ? tr("The devices this window reads: each has its front panel and records with the others.")
                                          : tooltip);
}

void DeviceSidebar::workspaceMenu(QTreeWidgetItem *item, const QPoint &pos)
{
  const QString file = item->data(0, Qt::UserRole).toString();
  const QString dir = QFileInfo(file).absolutePath();
  QMenu menu(this);
  QAction *load = menu.addAction(QIcon::fromTheme("document-open"), tr("&Load"));
  load->setEnabled(!workspaceOf(item).isEmpty());
  QAction *folder = menu.addAction(QIcon::fromTheme("folder-open"), tr("Open &folder"));
  folder->setEnabled(QFileInfo(dir).isDir());
  QAction *favorite = menu.addAction(tr("Fa&vourite"));
  favorite->setCheckable(true);
  favorite->setChecked(item->data(0, Qt::UserRole + 1).toBool());
  menu.addSeparator();
  QAction *remove = menu.addAction(QIcon::fromTheme("list-remove"), tr("&Remove from list"));
  QAction *chosen = menu.exec(pos);
  if (chosen == load)
    Q_EMIT workspaceRequested(file);
  else if (chosen == folder)
    QDesktopServices::openUrl(QUrl::fromLocalFile(dir));
  else if (chosen == remove)
    Q_EMIT removeWorkspaceRequested(file);
  else if (chosen == favorite)
    Q_EMIT favoriteRequested(file, favorite->isChecked());
}

void DeviceSidebar::serviceMenu(QTreeWidgetItem *item, const QPoint &pos)
{
  const QString address = item->data(1, Qt::UserRole).toString();
  QMenu menu(this);
  QAction *settings = menu.addAction(QIcon::fromTheme("configure"), tr("&Settings..."));
  QAction *copy = menu.addAction(QIcon::fromTheme("edit-copy"), tr("&Copy address"));
  copy->setEnabled(!address.isEmpty());
  QAction *chosen = menu.exec(pos);
  if (chosen == settings)
    Q_EMIT serviceSettingsRequested();
  else if (chosen == copy)
    QApplication::clipboard()->setText(address);
}

void DeviceSidebar::fill()
{
  // a name being typed (Rename) would go: later, once it is done
  if (state() == QAbstractItemView::EditingState)
  {
    QTimer::singleShot(250, this, &DeviceSidebar::fill);
    return;
  }
  m_filling = true;
  const QString selected = idOf(currentItem());
  qDeleteAll(m_devices->takeChildren());
  m_addItem = nullptr;
  for (const MyDevice &d : m_library->list())
  {
    auto *item = new QTreeWidgetItem(m_devices, { d.name });
    item->setData(0, Qt::UserRole, d.id);
    item->setFlags(Qt::ItemIsEnabled | Qt::ItemIsSelectable | Qt::ItemIsDragEnabled | Qt::ItemIsEditable);
    item->setFirstColumnSpanned(true);
    item->setData(0, kWhere, d.where().isEmpty() ? d.model() : QString("%1 · %2").arg(d.model(), d.where()));
    item->setData(0, kSymbol, symbolOf(d, devicePixelRatioF()));
    if (d.id == selected)
      setCurrentItem(item);
  }
  // a device more: the assistant, from where the devices are
  m_addItem = new QTreeWidgetItem(m_devices, { tr("+ Add device...") });
  m_addItem->setFlags(Qt::ItemIsEnabled);
  m_addItem->setFirstColumnSpanned(true);
  m_addItem->setForeground(0, palette().color(QPalette::Disabled, QPalette::WindowText));
  m_addItem->setToolTip(0, tr("Set up another meter (the assistant)."));
  m_addItem->setIcon(0, dot(QColor(), devicePixelRatioF()));   // its text where the names are
  // folded as the user left it; without devices the row to add one shows
  if (m_devices->childCount() == 1)
    m_devices->setExpanded(true);
  updateMarks();
  m_filling = false;
}

void DeviceSidebar::updateMarks()
{
  for (int i = 0; i < m_devices->childCount(); ++i)
  {
    QTreeWidgetItem *item = m_devices->child(i);
    if (item == m_addItem)
      continue;
    // on the bench: bold, its picture without a dot (the bench has those)
    const QString id = idOf(item);
    const auto open = std::find_if(m_openList.cbegin(), m_openList.cend(),
                                   [&id](const OpenDevice &d) { return d.id == id && d.connected; });
    const bool connected = open != m_openList.cend();
    const bool active = connected && open->active;
    // model and port, whether it is connected, how to read more
    QString tip = item->data(0, kWhere).toString();
    if (connected)
      tip += '\n' + (active ? tr("Connected, readings are coming in") : tr("Connected"));
    item->setToolTip(0, tip + '\n' + tr("Double-click: info and settings"));
    QFont font = item->font(0);
    font.setBold(isConnected(id));   // on the bench, connected or not
    item->setFont(0, font);
    QIcon symbol(item->data(0, kSymbol).value<QPixmap>());
    symbol.addPixmap(item->data(0, kSymbol).value<QPixmap>(), QIcon::Selected);
    item->setIcon(0, symbol);
  }
}

QTreeWidgetItem *DeviceSidebar::deviceItem(const QString &id) const
{
  for (int i = 0; i < m_devices->childCount(); ++i)
    if (idOf(m_devices->child(i)) == id)
      return m_devices->child(i);
  return nullptr;
}

QStringList DeviceSidebar::deviceIds() const
{
  QStringList ids;
  for (int i = 0; i < m_devices->childCount(); ++i)
    if (m_devices->child(i) != m_addItem)
      ids << idOf(m_devices->child(i));
  return ids;
}

QTreeWidgetItem *DeviceSidebar::nodeOf(QTreeWidgetItem *item)
{
  while (item && item->parent())
    item = item->parent();
  return item;
}

void DeviceSidebar::dragMoveEvent(QDragMoveEvent *event)
{
  QTreeWidget::dragMoveEvent(event);
  // between the nodes: connect or close; within "Connected devices": nothing
  const QTreeWidgetItem *from = nodeOf(currentItem());
  const QTreeWidgetItem *to = nodeOf(itemAt(event->position().toPoint()));
  // a value row goes onto the views only
  if ((from == m_open && to == m_open) || currentItem()->data(0, kValueId).isValid())
    event->ignore();
  else if ((from == m_devices && to == m_open && !isConnected(idOf(currentItem()))) || (from == m_open && to == m_devices))
    event->accept();
}

QMimeData *DeviceSidebar::mimeData(const QList<QTreeWidgetItem *> &items) const
{
  QMimeData *data = QTreeWidget::mimeData(items);
  if (!data || items.size() != 1)
    return data;
  // for the window: a value onto a view, an entry of My devices to connect
  QTreeWidgetItem *item = items.first();
  if (const ValueRef value = valueOf(item); value.source != QLatin1String("-"))
    data->setData(kValueMime, value.toString().toUtf8());
  else if (nodeOf(item) == m_devices && item != m_addItem && !idOf(item).isEmpty())
    data->setData(kDeviceMime, idOf(item).toUtf8());
  return data;
}

void DeviceSidebar::dropEvent(QDropEvent *event)
{
  if (currentItem() && currentItem()->data(0, kValueId).isValid())
  {
    event->ignore();
    return;
  }
  const QTreeWidgetItem *from = nodeOf(currentItem());
  const QTreeWidgetItem *to = nodeOf(itemAt(event->position().toPoint()));
  if (from == m_open || to == m_open)
  {
    // the items stay where they are: the session changes and fills them anew
    event->setDropAction(Qt::IgnoreAction);
    event->accept();
    if (from == m_devices && to == m_open && !isConnected(idOf(currentItem())))
      Q_EMIT openBesideRequested(idOf(currentItem()));
    else if (from == m_open && to == m_devices)
      Q_EMIT closeRequested(openOf(currentItem()));
    return;
  }
  const QString id = idOf(currentItem());
  QTreeWidget::dropEvent(event);
  // the entry's new place in the node is its place in the list
  const int index = int(deviceIds().indexOf(id));
  if (!id.isEmpty() && index >= 0)
    m_library->move(id, index);
}

void DeviceSidebar::changeEvent(QEvent *event)
{
  QTreeWidget::changeEvent(event);
  // the greys are set on the entries: those of the new design
  if (event->type() != QEvent::PaletteChange || m_filling || !m_addItem)
    return;
  m_addItem->setForeground(0, palette().color(QPalette::Disabled, QPalette::WindowText));
  setOpenDevices(m_openList);
  setServices(m_serviceList);
  setWorkspaces(m_workspaceList);
}

void DeviceSidebar::keyPressEvent(QKeyEvent *event)
{
  // F2 renames the device
  QTreeWidgetItem *item = currentItem();
  if ((event->key() == Qt::Key_Return || event->key() == Qt::Key_Enter) && !workspaceOf(item).isEmpty())
  {
    Q_EMIT workspaceRequested(workspaceOf(item));
    event->accept();
    return;
  }
  if (event->key() == Qt::Key_F2 && event->modifiers() == Qt::NoModifier && item
      && (item->flags() & Qt::ItemIsEditable))
  {
    editItem(item, 0);
    event->accept();
    return;
  }
  QTreeWidget::keyPressEvent(event);
}

void DeviceSidebar::openMenu(QTreeWidgetItem *item, const QPoint &pos)
{
  const QString id = openOf(item);
  bool connected = true;
  for (const OpenDevice &device : std::as_const(m_openList))
    if (device.id == id)
      connected = device.connected;
  QMenu menu(this);
  // its entry of My devices, as there: all its values for a look
  QAction *panel = menu.addAction(QIcon::fromTheme("qtdmm-display"), tr("Show &front panel"));
  QAction *info = nullptr;
  if (m_library->find(id))
    info = menu.addAction(QIcon::fromTheme("configure"), tr("&Info and settings..."));
  QAction *link = menu.addAction(connected ? tr("&Disconnect") : tr("Co&nnect"));
  // its rules, kept with it
  menu.addSeparator();
  QAction *alarms = menu.addAction(tr("&Alarms..."));
  QAction *limits = menu.addAction(tr("Li&mits..."));
  menu.addSeparator();
  // its frames with what it showed, for a report of what is read wrong
  QAction *capture = menu.addAction(tr("&Capture..."));
  menu.addSeparator();
  QAction *close = menu.addAction(QIcon::fromTheme("window-close"), tr("&Remove from bench"));
  QAction *chosen = menu.exec(pos);
  if (chosen && chosen == info)
    Q_EMIT settingsRequested(id);
  else if (chosen == panel)
    Q_EMIT frontPanelRequested(id);
  else if (chosen == link)
    Q_EMIT connectRequested(id, !connected);
  else if (chosen == alarms)
    Q_EMIT alarmsRequested(id);
  else if (chosen == limits)
    Q_EMIT limitsRequested(id);
  else if (chosen == capture)
    Q_EMIT captureRequested(id);
  else if (chosen && chosen == close)
    Q_EMIT closeRequested(id);
}

void DeviceSidebar::contextMenuEvent(QContextMenuEvent *event)
{
  QTreeWidgetItem *item = itemAt(event->pos());
  if (item && item->parent() == m_workspaces)
  {
    workspaceMenu(item, event->globalPos());
    return;
  }
  if (item && item == m_workspaces)
  {
    if (!m_workspaceActions.isEmpty())
    {
      QMenu menu(this);
      menu.addActions(m_workspaceActions);
      menu.exec(event->globalPos());
    }
    return;
  }
  if (item && (item == m_services || item->parent() == m_services))
  {
    if (item->parent() == m_services)
      serviceMenu(item, event->globalPos());
    return;
  }
  if (item && item->data(0, kValueId).isValid())
  {
    Q_EMIT valueMenuRequested(valueOf(item), event->globalPos());
    return;
  }
  if (item && !openOf(item).isEmpty())
  {
    openMenu(item, event->globalPos());
    return;
  }
  const QString id = idOf(item);
  const std::optional<MyDevice> d = m_library->find(id);
  if (!d)
    return;
  QMenu menu(this);
  QAction *settings = menu.addAction(QIcon::fromTheme("configure"), tr("&Info and settings..."));
  QAction *rename = menu.addAction(tr("&Rename"));
  QAction *beside = menu.addAction(QIcon::fromTheme("network-connect"), tr("&Use"));
  beside->setToolTip(tr("Reads it with the other devices connected: recorded with them, in formulas and over SCPI"));
  beside->setEnabled(!isConnected(id));
  menu.addSeparator();
  QAction *remove = menu.addAction(QIcon::fromTheme("list-remove"), tr("Re&move from My devices"));
  menu.setToolTipsVisible(true);
  QAction *chosen = menu.exec(event->globalPos());
  if (chosen == settings)
    Q_EMIT settingsRequested(id);
  else if (chosen == rename)
  {
    // the list may have been filled anew meanwhile
    for (int i = 0; i < m_devices->childCount(); ++i)
      if (m_devices->child(i)->data(0, Qt::UserRole).toString() == id)
        editItem(m_devices->child(i));
  }
  else if (chosen == beside)
    Q_EMIT openBesideRequested(id);
  else if (chosen == remove
           && QMessageBox::question(this, tr("Remove device"), tr("Remove \"%1\" from My devices?").arg(d->name))
                == QMessageBox::Yes)
    m_library->remove(id);
}

QSize DeviceSidebar::sizeHint() const
{
  // room for a name with its unit, a state and the marks of where it is shown
  return { fontMetrics().averageCharWidth() * 40, QTreeWidget::sizeHint().height() };
}

bool DeviceSidebar::isConnected(const QString &id) const
{
  for (const OpenDevice &device : m_openList)
    if (device.id == id)
      return true;
  return false;
}

bool DeviceSidebar::shownAtStart(const QVariant &stored, int devices)
{
  return stored.isValid() ? stored.toBool() : devices >= 2;
}

bool DeviceSidebar::opensAfterAdd(int devicesBefore, int devicesAfter)
{
  return devicesBefore < 2 && devicesAfter >= 2;
}
