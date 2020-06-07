// This file is part of Projecteur - https://github.com/jahnf/projecteur - See LICENSE.md and README.md
#include "actiondelegate.h"

#include "deviceinput.h"
#include "inputmapconfig.h"
#include "logging.h"
#include "nativekeyseqedit.h"
#include "projecteur-icons-def.h"

#include <QEvent>
#include <QLineEdit>
#include <QMenu>
#include <QPainter>

namespace  {
  namespace keysequence {
  //------------------------------------------------------------------------------------------------
  void paint(QPainter* p, const QStyleOptionViewItem& option, const KeySequenceAction* action)
  {
    const auto& fm = option.fontMetrics;
    const int xPos = (option.rect.height()-fm.height()) / 2;
    NativeKeySeqEdit::drawSequence(xPos, *p, option, action->keySequence);
  }

  //------------------------------------------------------------------------------------------------
  QSize sizeHint(const QStyleOptionViewItem& opt, const KeySequenceAction* action)
  {
    constexpr int verticalMargin = 3;
    constexpr int horizontalMargin = 3;
    const int h = opt.fontMetrics.height() + 2 * verticalMargin;
  #if (QT_VERSION >= QT_VERSION_CHECK(5, 11, 0))
    const int w = std::max(opt.fontMetrics.horizontalAdvance(ActionDelegate::tr("None")) + 2 * horizontalMargin,
                           opt.fontMetrics.horizontalAdvance(action->keySequence.toString()));
  #else
    const int w = std::max(opt.fontMetrics.width(ActionDelegate::tr("None")) + 2 * horizontalMargin,
                           opt.fontMetrics.width(action->keySequence.toString()));
  #endif
    return QSize(w, h);
  }
}

namespace cyclepresets {
  //------------------------------------------------------------------------------------------------
  void paint(QPainter* p, const QStyleOptionViewItem& option, const CyclePresetsAction* /*action*/)
  {
    const auto& fm = option.fontMetrics;
    const int xPos = (option.rect.height()-fm.height()) / 2;
    NativeKeySeqEdit::drawText(xPos, *p, option, "Cycle Presets");
  }

  //------------------------------------------------------------------------------------------------
  QSize sizeHint(const QStyleOptionViewItem& /*opt*/, const CyclePresetsAction* /*action*/)
  {
    return QSize(100,16);
  }
}
} // end anonymous namespace

//-------------------------------------------------------------------------------------------------
//-------------------------------------------------------------------------------------------------
void ActionDelegate::paint(QPainter* painter, const QStyleOptionViewItem& option,
                           const QModelIndex& index) const
{
  // Let QStyledItemDelegate handle drawing current focus inidicator and other basic stuff..
  QStyledItemDelegate::paint(painter, option, index);

  const auto imModel = qobject_cast<const InputMapConfigModel*>(index.model());
  if (!imModel) { return; }
  const auto& item = imModel->configData(index);
  if (!item.action) { return; }

  switch (item.action->type())
  {
  case Action::Type::KeySequence:
    keysequence::paint(painter, option, static_cast<KeySequenceAction*>(item.action.get()));
    break;
  case Action::Type::CyclePresets:
    cyclepresets::paint(painter, option, static_cast<CyclePresetsAction*>(item.action.get()));
    break;
  }
}

// -------------------------------------------------------------------------------------------------
QSize ActionDelegate::sizeHint(const QStyleOptionViewItem& opt, const QModelIndex& index) const
{
  const auto imModel = qobject_cast<const InputMapConfigModel*>(index.model());
  if (!imModel) return QStyledItemDelegate::sizeHint(opt, index);
  const auto& item = imModel->configData(index);
  if (!item.action) { return QStyledItemDelegate::sizeHint(opt, index); }

  switch (item.action->type())
  {
  case Action::Type::KeySequence:
    return keysequence::sizeHint(opt, static_cast<KeySequenceAction*>(item.action.get()));
  case Action::Type::CyclePresets:
    return cyclepresets::sizeHint(opt, static_cast<CyclePresetsAction*>(item.action.get()));
  }

  return QStyledItemDelegate::sizeHint(opt, index);
}

// -------------------------------------------------------------------------------------------------
QWidget* ActionDelegate::createEditor(QWidget* parent, const Action* action) const
{
  switch (action->type())
  {
  case Action::Type::KeySequence: {
    const auto editor = new NativeKeySeqEdit(parent);
    connect(editor, &NativeKeySeqEdit::editingFinished, this, &ActionDelegate::commitAndCloseEditor);
    return editor;
  }
  case Action::Type::CyclePresets:
    const auto editor = new QLineEdit(parent);
    connect(editor, &QLineEdit::editingFinished, this, &ActionDelegate::commitAndCloseEditor_);
    return editor;
  }
  return nullptr;
}

// -------------------------------------------------------------------------------------------------
QWidget* ActionDelegate::createEditor(QWidget* parent, const QStyleOptionViewItem& /*option*/,
                                      const QModelIndex& index) const

{
  const auto imModel = qobject_cast<const InputMapConfigModel*>(index.model());
  if (!imModel) return nullptr;
  const auto& item = imModel->configData(index);
  if (!item.action) { return nullptr; }

  return createEditor(parent, item.action.get());
}

// -------------------------------------------------------------------------------------------------
void ActionDelegate::setEditorData(QWidget* editor, const QModelIndex& index) const
{
  if (const auto seqEditor = qobject_cast<NativeKeySeqEdit*>(editor))
  {
    if (const auto imModel = qobject_cast<const InputMapConfigModel*>(index.model()))
    {
      const auto& item = imModel->configData(index);
      const auto action = static_cast<KeySequenceAction*>(item.action.get());
      seqEditor->setKeySequence(action->keySequence);
      seqEditor->setRecording(true);
      return;
    }
  }
  // TODO check for cyclepresets editor/type

  QStyledItemDelegate::setEditorData(editor, index);
}

// -------------------------------------------------------------------------------------------------
void ActionDelegate::setModelData(QWidget* editor, QAbstractItemModel* model,
                                  const QModelIndex& index) const
{
  if (const auto seqEditor = qobject_cast<NativeKeySeqEdit*>(editor)) {
    if (const auto imModel = qobject_cast<InputMapConfigModel*>(model)) {
      imModel->setKeySequence(index, seqEditor->keySequence());
      return;
    }
  }
  // TODO check for cyclePresets editor /type

  QStyledItemDelegate::setModelData(editor, model, index);
}

// -------------------------------------------------------------------------------------------------
bool ActionDelegate::eventFilter(QObject* obj, QEvent* ev)
{
  if (ev->type() == QEvent::KeyPress)
  {
    // let all key press events pass through to editor,
    // otherwise some keys cannot be recorded as a key sequence (e.g. [Tab] and [Esc])
    if (qobject_cast<NativeKeySeqEdit*>(obj)) return false;
  }
  return QStyledItemDelegate::eventFilter(obj,ev);
}

// -------------------------------------------------------------------------------------------------
void ActionDelegate::commitAndCloseEditor(QWidget* editor)
{
  emit commitData(editor);
  emit closeEditor(editor);
}

// -------------------------------------------------------------------------------------------------
void ActionDelegate::commitAndCloseEditor_()
{
  commitAndCloseEditor(qobject_cast<QWidget*>(sender()));
}

// -------------------------------------------------------------------------------------------------
void ActionDelegate::actionContextMenu(QWidget* parent, InputMapConfigModel* model,
                                       const QModelIndex& index, const QPoint& globalPos)
{
  if (!index.isValid() || !model) return;
  const auto& item = model->configData(index);
  if (!item.action || item.action->type() != Action::Type::KeySequence) return;

  QMenu* menu = new QMenu(parent);
  const std::vector<const NativeKeySequence*> predefinedKeys = {
    &NativeKeySequence::predefined::altTab(),
    &NativeKeySequence::predefined::altF4(),
    &NativeKeySequence::predefined::meta(),
  };

  for (const auto ks : predefinedKeys) {
    const auto qaction = menu->addAction(ks->toString());
    connect(qaction, &QAction::triggered, this, [model, index, ks](){
      model->setKeySequence(index, *ks);
    });
  }

  menu->exec(globalPos);
  menu->deleteLater();
}

//-------------------------------------------------------------------------------------------------
//-------------------------------------------------------------------------------------------------
void ActionTypeDelegate::paint(QPainter* painter, const QStyleOptionViewItem& option,
                               const QModelIndex& index) const
{
  // Let QStyledItemDelegate handle drawing current focus inidicator and other basic stuff..
  QStyledItemDelegate::paint(painter, option, index);

  const auto imModel = qobject_cast<const InputMapConfigModel*>(index.model());
  if (!imModel) { return; }
  const auto& item = imModel->configData(index);
  if (!item.action) { return; }

  const auto symbol = [&item]() -> unsigned int {
    switch(item.action->type()) {
    case Action::Type::KeySequence: return Font::Icon::keyboard_4;
    case Action::Type::CyclePresets: return Font::Icon::connection_8;
    }
    return 0;
  }();

  if (symbol != 0)
    drawActionTypeSymbol(0, *painter, option, symbol);
}

// -------------------------------------------------------------------------------------------------
void ActionTypeDelegate::actionContextMenu(QWidget* parent, InputMapConfigModel* model,
                                           const QModelIndex& index, const QPoint& globalPos)
{
  if (!index.isValid() || !model) return;
  const auto& item = model->configData(index);
  if (!item.action) return;

  struct actionEntry {
    Action::Type type;
    QChar symbol;
    QString text;
    QIcon icon = {};
  };

  static std::vector<actionEntry> items {
    {Action::Type::KeySequence, Font::Icon::keyboard_4, tr("Key Sequence")},
    {Action::Type::CyclePresets, Font::Icon::connection_8, tr("Cycle Presets")},
  };

  static bool initIcons = []()
  {
    Q_UNUSED(initIcons)
    QFont iconFont("projecteur-icons");
    constexpr int iconSize = 16;
    iconFont.setPixelSize(iconSize);
    for (auto& item : items)
    {
      QImage img(QSize(iconSize, iconSize), QImage::Format::Format_ARGB32_Premultiplied);
      img.fill(Qt::transparent);
      QPainter p(&img);
      p.setFont(iconFont);
      QRect(0, 0, img.width(), img.height());
      p.drawText(QRect(0, 0, img.width(), img.height()),
                 Qt::AlignHCenter | Qt::AlignVCenter, QString(item.symbol));
      item.icon = QIcon(QPixmap::fromImage(img));
    }
    return true;
  }();

  QMenu* menu = new QMenu(parent);

  for (const auto item : items) {
    const auto qaction = menu->addAction(item.icon, item.text);
    connect(qaction, &QAction::triggered, this, [model, index, type=item.type](){
      model->setItemActionType(index, type);
    });
  }

  menu->exec(globalPos);
  menu->deleteLater();
}

// -------------------------------------------------------------------------------------------------
int ActionTypeDelegate::drawActionTypeSymbol(int startX, QPainter& p,
                                             const QStyleOptionViewItem& option, const QChar& symbol)
{
  const auto r = QRect(QPoint(startX + option.rect.left(), option.rect.top()),
                       option.rect.bottomRight());

  QFont iconFont("projecteur-icons");
  iconFont.setPixelSize(qMin(option.rect.height(), option.rect.width()) - 4);

  p.save();
  p.setFont(iconFont);
  p.setRenderHint(QPainter::Antialiasing, true);

  if (option.state & QStyle::State_Selected)
    p.setPen(option.palette.color(QPalette::HighlightedText));
  else
    p.setPen(option.palette.color(QPalette::Text));

  QRect br;
  p.drawText(r, Qt::AlignHCenter | Qt::AlignVCenter, QString(symbol), &br);
  p.restore();

  return br.width();
}
