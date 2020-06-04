// This file is part of Projecteur - https://github.com/jahnf/projecteur - See LICENSE.md and README.md
#pragma once

#include "deviceinput.h"
#include "nativekeyseqedit.h"

#include <QAbstractTableModel>
#include <QKeySequence>
#include <QPointer>
#include <QTableView>

#include <map>
#include <vector>

// -------------------------------------------------------------------------------------------------
struct InputMapModelItem {
  KeyEventSequence deviceSequence;
  NativeKeySequence mappedSequence;
  bool isDuplicate = false;
};

// -------------------------------------------------------------------------------------------------
class InputMapConfigModel : public QAbstractTableModel
{
  Q_OBJECT

public:
  enum Roles { InputSeqRole = Qt::UserRole + 1, NativeSeqRole };
  enum Columns { InputSeqCol = 0, /*ActionTypeCol,*/ ActionCol, ColumnsCount};

  InputMapConfigModel(QObject* parent = nullptr);
  InputMapConfigModel(InputMapper* im, QObject* parent = nullptr);

  int rowCount(const QModelIndex& parent = QModelIndex()) const override;
  int columnCount(const QModelIndex& parent = QModelIndex()) const override;
  QVariant headerData(int section, Qt::Orientation orientation, int role = Qt::DisplayRole) const override;
  QVariant data(const QModelIndex& index, int role = Qt::DisplayRole) const override;
  Qt::ItemFlags flags(const QModelIndex& index) const override;

  void removeConfigItemRows(std::vector<int> rows);
  int addConfigItem(const InputMapModelItem& cfg = {});

  const InputMapModelItem& configData(const QModelIndex& index) const;
  void setInputSequence(const QModelIndex& index, const KeyEventSequence& kes);
  void setKeySequence(const QModelIndex& index, const NativeKeySequence& ks);

  InputMapper* inputMapper() const;
  void setInputMapper(InputMapper* im);

  InputMapConfig configuration() const;
  void setConfiguration(const InputMapConfig& config);

private:
  void configureInputMapper();
  void removeConfigItemRows(int fromRow, int toRow);
  void updateDuplicates();
  QPointer<InputMapper> m_inputMapper;
  QList<InputMapModelItem> m_configItems;
  std::map<KeyEventSequence, int> m_duplicates;
};

// -------------------------------------------------------------------------------------------------
struct InputMapConfigView : public QTableView
{
  Q_OBJECT

public:
  InputMapConfigView(QWidget* parent = nullptr);

  void setModel(QAbstractItemModel* model) override;

protected:
  void keyPressEvent(QKeyEvent* e) override;

private:
  bool m_editing = false;
};

