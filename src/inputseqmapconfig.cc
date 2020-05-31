// This file is part of Projecteur - https://github.com/jahnf/projecteur - See LICENSE.md and README.md
#include "inputseqmapconfig.h"

#include "inputseqedit.h"
#include "logging.h"

#include <QHeaderView>
#include <QKeyEvent>

// -------------------------------------------------------------------------------------------------
namespace  {
  const InputSeqMapConfig invalidItem_;
}

// -------------------------------------------------------------------------------------------------
InputSeqMapConfigModel::InputSeqMapConfigModel(QObject* parent)
  : InputSeqMapConfigModel(nullptr, parent)
{}

// -------------------------------------------------------------------------------------------------
InputSeqMapConfigModel::InputSeqMapConfigModel(InputMapper* im, QObject* parent)
  : QAbstractTableModel(parent)
  , m_inputMapper(im)
{}

// -------------------------------------------------------------------------------------------------
int InputSeqMapConfigModel::rowCount(const QModelIndex& parent) const
{
  return ( parent == QModelIndex() ) ? m_inputSeqMapConfigs.size() : 0;
}

// -------------------------------------------------------------------------------------------------
int InputSeqMapConfigModel::columnCount(const QModelIndex& /*parent*/) const
{
  return ColumnsCount;
}

// -------------------------------------------------------------------------------------------------
Qt::ItemFlags InputSeqMapConfigModel::flags(const QModelIndex &index) const
{
  if (index.column() == InputSeqCol)
    return (QAbstractTableModel::flags(index) | Qt::ItemIsEditable);
  else if (index.column() == ActionCol)
    return (QAbstractTableModel::flags(index) | Qt::ItemIsEditable);

  return QAbstractTableModel::flags(index) & ~Qt::ItemIsEditable;
}

// -------------------------------------------------------------------------------------------------
QVariant InputSeqMapConfigModel::data(const QModelIndex& index, int role) const
{
  if (index.row() >= static_cast<int>(m_inputSeqMapConfigs.size()))
    return QVariant();

  if (index.column() == InputSeqCol && role == Roles::InputSeqRole) {
    return QVariant::fromValue(m_inputSeqMapConfigs[index.row()].sequence);
  }
  else if (index.column() == ActionCol && role == Qt::DisplayRole) {
    return m_inputSeqMapConfigs[index.row()].keySequence;
  }

  return QVariant();
}

// -------------------------------------------------------------------------------------------------
QVariant InputSeqMapConfigModel::headerData(int section, Qt::Orientation orientation, int role) const
{
  if (orientation == Qt::Horizontal && role == Qt::DisplayRole)
  {
    switch(section)
    {
    case InputSeqCol: return tr("Input Sequence");
    //case ActionTypeCol: return tr("Type");
    case ActionCol: return tr("Mapped Key(s)");
    }
  }
  return QVariant();
}

// -------------------------------------------------------------------------------------------------
const InputSeqMapConfig& InputSeqMapConfigModel::configData(const QModelIndex& index) const
{
  if (index.row() >= static_cast<int>(m_inputSeqMapConfigs.size()))
    return invalidItem_;

  return m_inputSeqMapConfigs[index.row()];
}

// -------------------------------------------------------------------------------------------------
void InputSeqMapConfigModel::removeConfigItemRows(int fromRow, int toRow)
{
  if (fromRow > toRow) return;

  beginRemoveRows(QModelIndex(), fromRow, toRow);
  for (int i = toRow; i >= fromRow && i < m_inputSeqMapConfigs.size(); --i) {
    m_inputSeqMapConfigs.removeAt(i);
  }
  endRemoveRows();
}

// -------------------------------------------------------------------------------------------------
int InputSeqMapConfigModel::addConfigItem(const InputSeqMapConfig& cfg)
{
  const auto row = m_inputSeqMapConfigs.size();
  beginInsertRows(QModelIndex(), row, row);
  m_inputSeqMapConfigs.push_back(cfg);
  endInsertRows();
  return row;
}

// -------------------------------------------------------------------------------------------------
void InputSeqMapConfigModel::removeConfigItemRows(std::vector<int> rows)
{
  if (rows.empty()) return;
  std::sort(rows.rbegin(), rows.rend());

  int seq_last = rows.front();
  int seq_first = seq_last;

  for (auto it = ++rows.cbegin(); it != rows.cend(); ++it)
  {
    if (seq_first - *it > 1)
    {
      removeConfigItemRows(seq_first, seq_last);
      seq_last = seq_first = *it;
    }
    else
    {
      seq_first = *it;
    }
  }

  removeConfigItemRows(seq_first, seq_last);
}

// -------------------------------------------------------------------------------------------------
void InputSeqMapConfigModel::setInputSequence(const QModelIndex& index, const KeyEventSequence& kes)
{
  if (index.row() < static_cast<int>(m_inputSeqMapConfigs.size()))
  {
    auto& c = m_inputSeqMapConfigs[index.row()];
    if (c.sequence != kes)
    {
      c.sequence = kes;
      emit dataChanged(index, index, {Qt::DisplayRole, Roles::InputSeqRole});
    }
  }
}

// -------------------------------------------------------------------------------------------------
void InputSeqMapConfigModel::setKeySequence(const QModelIndex& index, const QKeySequence& ks)
{
  if (index.row() < static_cast<int>(m_inputSeqMapConfigs.size()))
  {
    auto& c = m_inputSeqMapConfigs[index.row()];
    if (c.keySequence != ks)
    {
      c.keySequence = ks;
      emit dataChanged(index, index, {Qt::DisplayRole, Roles::InputSeqRole});
    }
  }
}

// -------------------------------------------------------------------------------------------------
InputMapper* InputSeqMapConfigModel::inputMapper() const
{
  return m_inputMapper;
}

// -------------------------------------------------------------------------------------------------
void InputSeqMapConfigModel::setInputMapper(InputMapper* im)
{
  m_inputMapper = im;
}

// -------------------------------------------------------------------------------------------------
// -------------------------------------------------------------------------------------------------
InputSeqMapTableView::InputSeqMapTableView(QWidget* parent)
  : QTableView(parent)
{
  verticalHeader()->setHidden(true);

  const auto imSeqDelegate = new InputSeqDelegate(this);
  setItemDelegateForColumn(InputSeqMapConfigModel::InputSeqCol, imSeqDelegate);

  const auto keySeqDelegate = new KeySequenceDelegate(this);
  setItemDelegateForColumn(InputSeqMapConfigModel::ActionCol, keySeqDelegate);

  setSelectionMode(QAbstractItemView::SelectionMode::ExtendedSelection);
  setSelectionBehavior(QAbstractItemView::SelectionBehavior::SelectRows);
  horizontalHeader()->setSectionResizeMode(QHeaderView::ResizeMode::Stretch);
}

// -------------------------------------------------------------------------------------------------
void InputSeqMapTableView::setModel(QAbstractItemModel* model)
{
  QTableView::setModel(model);

//  if (const auto imModel = qobject_cast<InputSeqMapConfigModel*>(model)) {
//    horizontalHeader()->setSectionResizeMode(InputSeqMapConfigModel::ActionTypeCol, QHeaderView::ResizeToContents);
//    horizontalHeader()->setSectionResizeMode(InputSeqMapConfigModel::ActionCol, QHeaderView::ResizeToContents);
//  }
}

//-------------------------------------------------------------------------------------------------
void InputSeqMapTableView::keyPressEvent(QKeyEvent* e)
{
  switch (e->key()) {
  case Qt::Key_Enter:
  case Qt::Key_Return:
    if (model()->flags(currentIndex()) & Qt::ItemIsEditable) {
      edit(currentIndex());
      return;
    }
    break;
  case Qt::Key_Delete:
    if (const auto imModel = qobject_cast<InputSeqMapConfigModel*>(model()))
    switch (currentIndex().column())
    {
    case InputSeqMapConfigModel::InputSeqCol:
      imModel->setInputSequence(currentIndex(), KeyEventSequence{});
      return;
    case InputSeqMapConfigModel::ActionCol:
      imModel->setKeySequence(currentIndex(), QKeySequence());
      return;
    }
    break;
  case Qt::Key_Tab:
    e->ignore(); // Allow to change focus to other widgets in dialog.
    return;
  }

  QTableView::keyPressEvent(e);
}

