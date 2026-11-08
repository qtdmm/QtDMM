// Copyright (c) 2026 The QtDMM developers
// SPDX-License-Identifier: GPL-3.0-or-later
#include "ui/views/readingswidget.h"

#include <QApplication>
#include <QCheckBox>
#include <QClipboard>
#include <QFileDialog>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QKeyEvent>
#include <QLabel>
#include <QMenu>
#include <QMessageBox>
#include <QPushButton>
#include <QScrollBar>
#include <QSortFilterProxyModel>
#include <QSpinBox>
#include <QStyle>
#include <QStyleOptionViewItem>
#include <QTableView>
#include <QTimer>
#include <QToolButton>
#include <QVBoxLayout>

#include "core/projectfolder.h"
#include "core/siprefix.h"

// The rows of the values the table shows (ReadingsModel::hiddenValues()).
// Only in use while one is left out: a full table of a million rows costs
// nothing extra the rest of the time.
class ReadingsWidget::ValueFilter : public QSortFilterProxyModel
{
public:
  explicit ValueFilter(ReadingsModel *log, QObject *parent) : QSortFilterProxyModel(parent), m_log(log)
  {
    setSourceModel(log);
  }
  void refresh()
  {
#if QT_VERSION >= QT_VERSION_CHECK(6, 10, 0)
    beginFilterChange();   // invalidateFilter() is deprecated from there on
    endFilterChange();
#else
    invalidateFilter();
#endif
  }

protected:
  bool filterAcceptsRow(int row, const QModelIndex &) const override
  {
    return m_log->rowShown(row);
  }

private:
  ReadingsModel *m_log;
};

ReadingsWidget::ReadingsWidget(QWidget *parent) :
  QWidget(parent),
  m_log(new ReadingsModel(this))
{
  QVBoxLayout *layout = new QVBoxLayout(this);
  layout->setContentsMargins(4, 4, 4, 4);
  layout->setSpacing(4);

  m_view = new QTableView(this);
  m_view->setModel(m_log);
  m_view->setSelectionBehavior(QAbstractItemView::SelectRows);
  m_view->setAlternatingRowColors(true);
  m_view->setEditTriggers(QAbstractItemView::NoEditTriggers);
  m_view->setWordWrap(false);
  m_view->verticalHeader()->setDefaultSectionSize(m_view->fontMetrics().height() + 4);
  m_view->verticalHeader()->setVisible(false);
  m_view->horizontalHeader()->setStretchLastSection(false);
  // widths come from fitColumns(): every column as wide as its content, the
  // rest of the width shared out; a table narrower than that scrolls
  m_view->horizontalHeader()->setSectionResizeMode(QHeaderView::Fixed);
  m_view->setHorizontalScrollMode(QAbstractItemView::ScrollPerPixel);
  m_view->horizontalHeader()->setHighlightSections(false);
  m_view->viewport()->installEventFilter(this);
  m_view->setContextMenuPolicy(Qt::CustomContextMenu);
  connect(m_view, &QTableView::customContextMenuRequested, this, [this](const QPoint &pos)
  {
    QMenu menu(this);
    if (!m_windowActions.isEmpty())
    {
      menu.addActions(m_windowActions);
      menu.addSeparator();
    }
    menu.addAction(tr("&Copy"), this, &ReadingsWidget::copySLOT);
    menu.addAction(tr("Select &all"), m_view, &QTableView::selectAll);
    menu.addSeparator();
    menu.addAction(tr("&Export..."), this, &ReadingsWidget::exportSLOT);
    menu.addAction(tr("C&lear table"), this, &ReadingsWidget::clearSLOT);
    menu.exec(m_view->viewport()->mapToGlobal(pos));
  });
  // Ctrl+C copies whole rows while the table has the focus. Elsewhere in
  // the window it connects the meter, so this is not a QAction shortcut
  // (that would be ambiguous) but a ShortcutOverride: the key event then
  // reaches the view before the window's shortcuts see it.
  m_view->installEventFilter(this);
  layout->addWidget(m_view, 1);

  // two groups in one row, or - in a narrow cell - on two rows, so no
  // button is cut off (placeBar())
  m_barLeft = new QWidget(this);
  QHBoxLayout *left = new QHBoxLayout(m_barLeft);
  left->setContentsMargins(0, 0, 0, 0);
  // one button, pause while logging, play while paused
  m_pause = new QToolButton(m_barLeft);
  m_pause->setCheckable(true);
  m_pause->setAutoRaise(true);
  m_pause->setToolTip(tr("Pause logging"));
  connect(m_pause, &QToolButton::toggled, this, [this](bool paused)
  {
    m_log->setPaused(paused);
    m_pause->setIcon(QIcon::fromTheme(paused ? "media-playback-start" : "media-playback-pause"));
    m_pause->setToolTip(paused ? tr("Resume logging") : tr("Pause logging"));
  });
  m_pause->setIcon(QIcon::fromTheme("media-playback-pause"));
  left->addWidget(m_pause);

  m_follow = new QCheckBox(tr("&Follow"), m_barLeft);
  m_follow->setChecked(true);
  m_follow->setToolTip(tr("Keep the newest reading in view. Scrolling up switches this off."));
  left->addWidget(m_follow);

  m_barRight = new QWidget(this);
  QHBoxLayout *right = new QHBoxLayout(m_barRight);
  right->setContentsMargins(0, 0, 0, 0);
  QLabel *keep = new QLabel(tr("Keep"), m_barRight);
  right->addWidget(keep);
  m_maxRows = new QSpinBox(m_barRight);
  m_maxRows->setRange(100, 1000000);
  m_maxRows->setSingleStep(1000);
  m_maxRows->setValue(m_log->maxRows());
  m_maxRows->setSuffix(tr(" rows"));
  m_maxRows->setToolTip(tr("How many readings the table keeps; the oldest are dropped."));
  connect(m_maxRows, qOverload<int>(&QSpinBox::valueChanged), m_log, &ReadingsModel::setMaxRows);
  right->addWidget(m_maxRows);

  // the buttons sit in a narrow column: short words where a language has
  // them, the menu has the long ones
  QPushButton *exportButton = new QPushButton(tr("&Export...", "button"), m_barRight);
  connect(exportButton, &QPushButton::clicked, this, &ReadingsWidget::exportSLOT);
  right->addWidget(exportButton);
  QPushButton *clearButton = new QPushButton(tr("C&lear table", "button"), m_barRight);
  clearButton->setToolTip(tr("Remove all rows; the recorder's graph is not affected"));
  connect(clearButton, &QPushButton::clicked, this, &ReadingsWidget::clearSLOT);
  right->addWidget(clearButton);

  m_row1 = new QHBoxLayout;
  m_row1->addWidget(m_barLeft);
  m_row1->addStretch(1);
  m_row1->addWidget(m_barRight);
  layout->addLayout(m_row1);
  m_row2 = new QHBoxLayout;
  m_row2->addStretch(1);
  layout->addLayout(m_row2);

  m_stats = new QLabel(this);
  m_stats->setTextInteractionFlags(Qt::TextSelectableByMouse);
  m_stats->setWordWrap(true);   // a long line must not dictate the width
  layout->addWidget(m_stats);

  connect(m_log, &QAbstractItemModel::rowsInserted, this, [this](const QModelIndex &, int first, int last)
  {
    measureRows(first, last);
  });
  connect(m_log, &QAbstractItemModel::dataChanged, this, [this](const QModelIndex &tl, const QModelIndex &br)
  {
    // the Time column gained or lost its date: measure it afresh
    if (tl.column() <= ReadingsModel::Time && br.column() >= ReadingsModel::Time && br.row() - tl.row() + 1 == m_log->rowCount())
    {
      m_need[ReadingsModel::Time] = 0;
      measureRows(qMax(0, m_log->rowCount() - kMeasureRows), m_log->rowCount() - 1);
    }
  });
  connect(m_log, &QAbstractItemModel::modelReset, this, [this]
  {
    m_need.fill(0);
    measureRows(0, -1);
  });
  connect(m_log, &QAbstractItemModel::rowsInserted, this, &ReadingsWidget::followSLOT);
  // the line goes over all rows: not at each reading, several meters come fast.
  // A running timer stays as it is, a steady stream must not put it off
  m_statsTimer = new QTimer(this);
  m_statsTimer->setSingleShot(true);
  m_statsTimer->setInterval(250);
  connect(m_statsTimer, &QTimer::timeout, this, &ReadingsWidget::updateStats);
  auto statsLater = [this] { if (!m_statsTimer->isActive()) m_statsTimer->start(); };
  connect(m_log, &QAbstractItemModel::rowsInserted, this, statsLater);
  connect(m_log, &QAbstractItemModel::rowsRemoved, this, statsLater);
  connect(m_log, &QAbstractItemModel::modelReset, this, &ReadingsWidget::updateStats);
  // scrolling away from the end pauses following; scrolling back resumes it
  connect(m_view->verticalScrollBar(), &QScrollBar::valueChanged, this, [this](int value)
  {
    if (m_view->verticalScrollBar()->isSliderDown() || QApplication::mouseButtons() != Qt::NoButton
        || m_view->verticalScrollBar()->hasFocus())
      m_follow->setChecked(value >= m_view->verticalScrollBar()->maximum());
  });
  connect(m_log, &ReadingsModel::hiddenValuesChanged, this, &ReadingsWidget::applyFilter);
  connect(m_log, &ReadingsModel::hiddenValuesChanged, this, &ReadingsWidget::updateStats);
  connect(m_log, &ReadingsModel::valueNamesChanged, this, &ReadingsWidget::updateStats);
  connect(m_log, &ReadingsModel::otherValuesChanged, this, [this](bool on)
  {
    m_view->setColumnHidden(ReadingsModel::Number, !on);
    fitColumns();
  });
  m_view->setColumnHidden(ReadingsModel::Number, !m_log->hasOtherValues());
  // the limit test's column once a verdict has come
  connect(m_log, &ReadingsModel::verdictsChanged, this, [this](bool on)
  {
    m_view->setColumnHidden(ReadingsModel::Limit, !on);
    fitColumns();
  });
  m_view->setColumnHidden(ReadingsModel::Limit, !m_log->hasVerdicts());
  // the meter of each row, while there is more than one
  connect(m_log, &ReadingsModel::devicesChanged, this, [this](bool on)
  {
    m_view->setColumnHidden(ReadingsModel::Device, !on);
    fitColumns();
  });
  m_view->setColumnHidden(ReadingsModel::Device, !m_log->hasDevices());
  m_need.fill(0, ReadingsModel::ColumnCount);
  measureRows(0, -1);
  updateStats();
}

void ReadingsWidget::applyFilter()
{
  const bool filter = !m_log->hiddenValues().isEmpty();
  const bool created = filter && !m_filter;
  if (created)
    m_filter = new ValueFilter(m_log, this);
  else if (filter)
    m_filter->refresh();
  QAbstractItemModel *model = filter ? static_cast<QAbstractItemModel *>(m_filter) : m_log;
  if (m_view->model() != model)
  {
    m_view->setModel(model);
    m_view->setColumnHidden(ReadingsModel::Number, !m_log->hasOtherValues());
    m_view->setColumnHidden(ReadingsModel::Limit, !m_log->hasVerdicts());
    m_view->setColumnHidden(ReadingsModel::Device, !m_log->hasDevices());
    fitColumns();
  }
  // the filter has a new row only after the model's signal: follow it from
  // there, after the view took it (connected after setModel()), or the
  // newest row stays below the end
  if (created)
    connect(m_filter, &QAbstractItemModel::rowsInserted, this, &ReadingsWidget::followSLOT);
  followSLOT();
}

void ReadingsWidget::measureRows(int first, int last)
{
  // Header and the given rows, measured with the view's own item size hints
  // (padding and style included). m_need only grows: a column that jumps
  // back and forth with every reading would be worse than one a bit wide.
  // Only new rows are measured, so a full table costs nothing per reading.
  QHeaderView *header = m_view->horizontalHeader();
  const QStyleOptionViewItem option = [this]
  {
    QStyleOptionViewItem o;
    o.initFrom(m_view);
    o.font = m_view->font();
    o.widget = m_view;   // the delegate asks the view's style, not the application's
    return o;
  }();
  bool grown = false;
  for (int c = 0; c < ReadingsModel::ColumnCount; ++c)
  {
    int w = qMax(m_need[c], header->sectionSizeHint(c));
    for (int r = qMax(first, last - kMeasureRows + 1); r <= last; ++r)
      w = qMax(w, m_view->itemDelegateForColumn(c) ? m_view->itemDelegateForColumn(c)->sizeHint(option, m_log->index(r, c)).width()
                                                    : m_view->itemDelegate()->sizeHint(option, m_log->index(r, c)).width());
    if (w != m_need[c])
    {
      m_need[c] = w;
      grown = true;
    }
  }
  if (grown || first > last)
    fitColumns();
}

void ReadingsWidget::fitColumns()
{
  int sum = 0, n = 0, last = 0;
  for (int c = 0; c < ReadingsModel::ColumnCount; ++c)
    if (!m_view->isColumnHidden(c))
    {
      sum += m_need[c];
      ++n;
      last = c;
    }
  const int extra = qMax(0, m_view->viewport()->width() - sum);
  for (int c = 0; c < ReadingsModel::ColumnCount; ++c)
    if (!m_view->isColumnHidden(c))
      m_view->horizontalHeader()->resizeSection(c, m_need[c] + extra / n + (c == last ? extra % n : 0));
}

QSize ReadingsWidget::minimumSizeHint() const
{
  const QMargins m = layout()->contentsMargins();
  QSize size = QWidget::minimumSizeHint();
  size.setWidth(qMax(m_barLeft->sizeHint().width(), m_barRight->sizeHint().width()) + m.left() + m.right());
  return size;
}

void ReadingsWidget::resizeEvent(QResizeEvent *event)
{
  QWidget::resizeEvent(event);
  placeBar();
  // the spare width shared out anew, also when there is less of it
  fitColumns();
}

void ReadingsWidget::changeEvent(QEvent *event)
{
  QWidget::changeEvent(event);
  // a design switch brings another style with other paddings, a font change
  // other text widths: what was measured no longer fits. Once the view has
  // the new style too (the children get the event in no fixed order).
  if (event->type() == QEvent::StyleChange || event->type() == QEvent::FontChange)
    QTimer::singleShot(0, this, [this]
    {
      m_need.fill(0);
      measureRows(qMax(0, m_log->rowCount() - kMeasureRows), m_log->rowCount() - 1);
      fitColumns();
    });
}

// Both control groups in one row while they fit, else the right one below.
void ReadingsWidget::placeBar()
{
  const QMargins m = layout()->contentsMargins();
  const int need = m_barLeft->sizeHint().width() + m_row1->spacing() + m_barRight->sizeHint().width()
                   + m.left() + m.right();
  const bool wrap = width() < need;
  QHBoxLayout *target = wrap ? m_row2 : m_row1;
  if (target->indexOf(m_barRight) >= 0)
    return;
  (wrap ? m_row1 : m_row2)->removeWidget(m_barRight);
  target->addWidget(m_barRight);
}

int ReadingsWidget::maxRows() const
{
  return m_maxRows->value();
}

void ReadingsWidget::setMaxRows(int rows)
{
  m_maxRows->setValue(rows);
}

void ReadingsWidget::followSLOT()
{
  if (m_follow->isChecked())
    m_view->scrollToBottom();
}

void ReadingsWidget::updateStats()
{
  // of one value: the first one the table shows
  const ReadingsModel::Stats s = m_log->stats();
  if (m_log->rowCount() == 0)
  {
    m_stats->setText(tr("No readings yet."));
    return;
  }
  // a line break only between the entries, never inside "Min 6.3 V":
  // their spaces are no-break spaces, the entries are joined by plain ones
  const QChar nbsp(0x00a0);
  auto fmt = [&](double v)
  {
    QString prefix;
    const double scaled = SiPrefix::scale(v, s.unit, &prefix);
    return QString::number(scaled, 'g', 5) + nbsp + SiPrefix::displayText(prefix + s.unit);
  };
  auto entry = [&](const QString &e) { return QString(e).replace(' ', nbsp); };
  QStringList entries;
  // a meter with more values: which one they are of
  if (!s.source.isEmpty())
    entries << entry(tr("%1:").arg(m_log->deviceName(s.source)));
  else if (m_log->hasOtherValues())
  {
    const QString name = m_log->valueNames().value(s.id);
    entries << entry(name.isEmpty() ? tr("Value %1:").arg(s.id + 1) : tr("Value %1 (%2):").arg(s.id + 1).arg(name));
  }
  entries << entry(tr("%n reading(s)", "", s.count));
  if (s.numeric)
    entries << entry(tr("Min %1").arg(fmt(s.min))) << entry(tr("Max %1").arg(fmt(s.max)))
            << entry(tr("Mean %1").arg(fmt(s.mean))) << entry(tr("Span %1").arg(fmt(s.max - s.min)));
  m_stats->setText(entries.join("   "));
}

void ReadingsWidget::clearSLOT()
{
  m_log->clear();
}

void ReadingsWidget::exportSLOT()
{
  const QString csvFilter = tr("CSV (*.csv)"), xlsxFilter = tr("Excel (*.xlsx)"), odsFilter = tr("OpenDocument (*.ods)");
  QString filter = csvFilter;
  QString path = QFileDialog::getSaveFileName(this, tr("Export readings"), ProjectFolder::proposal(QString()),
                                              csvFilter + ";;" + xlsxFilter + ";;" + odsFilter, &filter);
  if (path.isEmpty())
    return;
  if (QFileInfo(path).suffix().isEmpty())
    path += filter == xlsxFilter ? ".xlsx" : filter == odsFilter ? ".ods" : ".csv";
  QString error;
  if (!m_log->writeAny(path, &error))
    QMessageBox::warning(this, tr("Export readings"), error);
}

void ReadingsWidget::copySLOT()
{
  QList<int> rows;
  for (const QModelIndex &index : m_view->selectionModel()->selectedRows())
    rows << (m_view->model() == m_filter ? m_filter->mapToSource(index).row() : index.row());
  QApplication::clipboard()->setText(m_log->toText(rows));
}

bool ReadingsWidget::eventFilter(QObject *watched, QEvent *event)
{
  if (watched == m_view->viewport() && event->type() == QEvent::Resize)
    fitColumns();
  if (watched == m_view && (event->type() == QEvent::ShortcutOverride || event->type() == QEvent::KeyPress))
  {
    QKeyEvent *key = static_cast<QKeyEvent *>(event);
    if (key->matches(QKeySequence::Copy))
    {
      if (event->type() == QEvent::KeyPress)
        copySLOT();
      event->accept();
      return true;
    }
  }
  return QWidget::eventFilter(watched, event);
}
