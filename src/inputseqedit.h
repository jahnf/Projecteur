// This file is part of Projecteur - https://github.com/jahnf/projecteur - See LICENSE.md and README.md
#pragma once

#include "deviceinput.h"

#include <QStyledItemDelegate>
#include <QWidget>

// -------------------------------------------------------------------------------------------------
class QStyleOptionFrame;
class InputMapConfigModel;

// -------------------------------------------------------------------------------------------------
class InputSeqEdit : public QWidget
{
  Q_OBJECT

public:
  InputSeqEdit(QWidget* parent = nullptr);
  InputSeqEdit(InputMapper* im, QWidget* parent = nullptr);
  ~InputSeqEdit();

  QSize sizeHint() const override;

  void setInputMapper(InputMapper* im);

  const KeyEventSequence& inputSequence() const;
  void setInputSequence(const KeyEventSequence& is);

  void clear();

signals:
  void inputSequenceChanged(const KeyEventSequence& inputSequence);
  void editingFinished(InputSeqEdit*);

public:
  // Public static helpers - can be reused by other editors or delegates
  static int drawRecordingSymbol(int startX, QPainter& p, const QStyleOption& option);
  static int drawPlaceHolderText(int startX, QPainter& p, const QStyleOption& option, const QString& text);
  static int drawEmptyIndicator(int startX, QPainter& p, const QStyleOption& option);

protected:
  void paintEvent(QPaintEvent* e) override;
  void mouseDoubleClickEvent(QMouseEvent* e) override;
  void keyPressEvent(QKeyEvent* e) override;
  void keyReleaseEvent(QKeyEvent* e) override;
  void focusOutEvent(QFocusEvent* e) override;
  QStyleOptionFrame styleOption() const;

private:
  InputMapper* m_inputMapper = nullptr;
  KeyEventSequence m_inputSequence;
  KeyEventSequence m_recordedSequence;
  //  8 KeyEvents, also equals 4 Button Presses (press + release)
  static constexpr uint8_t m_maxRecordingLength = 8;
};


// -------------------------------------------------------------------------------------------------
class InputSeqDelegate : public QStyledItemDelegate
{
  Q_OBJECT

public:
  using QStyledItemDelegate::QStyledItemDelegate;

  void paint(QPainter*, const QStyleOptionViewItem&, const QModelIndex&) const override;
  QSize sizeHint(const QStyleOptionViewItem&, const QModelIndex&) const override;
  QWidget *createEditor(QWidget*, const QStyleOptionViewItem&, const QModelIndex&) const override;
  void setEditorData(QWidget* editor, const QModelIndex& index) const override;
  void setModelData(QWidget* editor, QAbstractItemModel*, const QModelIndex&) const override;
  void inputSeqContextMenu(QWidget* parent, InputMapConfigModel* model, const QModelIndex& index,
                           const QPoint& globalPos);

  static void drawCurrentIndicator(QPainter &p, const QStyleOption& option);

private:
  void commitAndCloseEditor(InputSeqEdit* editor);
};

