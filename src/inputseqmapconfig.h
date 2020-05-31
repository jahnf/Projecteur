// This file is part of Projecteur - https://github.com/jahnf/projecteur - See LICENSE.md and README.md
#pragma once

#include <deviceinput.h>

#include <QAbstractTableModel>
#include <QKeySequence>
#include <QPointer>
#include <QTableView>

#include <vector>

// -------------------------------------------------------------------------------------------------
struct InputSeqMapConfig {
  KeyEventSequence sequence;
  QKeySequence keySequence;
};

// -------------------------------------------------------------------------------------------------
class InputSeqMapConfigModel : public QAbstractTableModel
{
  Q_OBJECT

public:
  enum Roles { InputSeqRole = Qt::UserRole + 1 };
  enum Columns { InputSeqCol = 0, /*ActionTypeCol,*/ ActionCol, ColumnsCount};

  InputSeqMapConfigModel(QObject* parent = nullptr);
  InputSeqMapConfigModel(InputMapper* im, QObject* parent = nullptr);

  int rowCount(const QModelIndex& parent = QModelIndex()) const override;
  int columnCount(const QModelIndex& parent = QModelIndex()) const override;
  QVariant headerData(int section, Qt::Orientation orientation, int role = Qt::DisplayRole) const override;
  QVariant data(const QModelIndex& index, int role = Qt::DisplayRole) const override;
  Qt::ItemFlags flags(const QModelIndex& index) const override;

  void removeConfigItemRows(std::vector<int> rows);
  int addConfigItem(const InputSeqMapConfig& cfg = {});

  const InputSeqMapConfig& configData(const QModelIndex& index) const;
  void setInputSequence(const QModelIndex& index, const KeyEventSequence& kes);
  void setKeySequence(const QModelIndex& index, const QKeySequence& ks);

  InputMapper* inputMapper() const;
  void setInputMapper(InputMapper* im);

private:
  void removeConfigItemRows(int fromRow, int toRow);
  QPointer<InputMapper> m_inputMapper;
  QList<InputSeqMapConfig> m_inputSeqMapConfigs;
};

// -------------------------------------------------------------------------------------------------
struct InputSeqMapTableView : public QTableView
{
  Q_OBJECT

public:
  InputSeqMapTableView(QWidget* parent = nullptr);

  void setModel(QAbstractItemModel* model) override;

protected:
  void keyPressEvent(QKeyEvent* e) override;
};

