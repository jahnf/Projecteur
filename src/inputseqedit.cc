// This file is part of Projecteur - https://github.com/jahnf/projecteur - See LICENSE.md and README.md
#include "inputseqedit.h"

#include "deviceinput.h"
#include "inputmapconfig.h"
#include "logging.h"

#include <QApplication>
#include <QKeySequenceEdit>
#include <QLineEdit>
#include <QPaintEvent>
#include <QStaticText>
#include <QStyleOptionFrame>
#include <QStylePainter>
#include <QVBoxLayout>

namespace {
  // -----------------------------------------------------------------------------------------------
  int drawRecordingSymbol(int startX, QPainter& p, const QStyleOption& option)
  {
    const auto iconSize = option.fontMetrics.height();
    const auto marginTop = (option.rect.height() - iconSize) / 2;
    const QRect iconRect(startX, marginTop, iconSize, iconSize);

    p.save();
    p.setPen(Qt::lightGray);
    p.setBrush(QBrush(Qt::red));
    p.setRenderHint(QPainter::Antialiasing);
    p.drawEllipse(iconRect);
    p.restore();

    return iconRect.width();
  }

  // -----------------------------------------------------------------------------------------------
  int drawPlaceHolderText(int startX, QPainter& p, const QStyleOption& option, const QString& text)
  {
    #if (QT_VERSION >= QT_VERSION_CHECK(5, 11, 0))
      const auto width = option.fontMetrics.horizontalAdvance(text);
    #else
      const auto width = option.fontMetrics.width(text);
    #endif

    const auto r = QRect(startX + option.rect.left(), option.rect.top(),
                         width, option.rect.height());

    p.save();
    p.setPen(option.palette.color(QPalette::Disabled, QPalette::Text));
    QRect br;
    p.drawText(r, Qt::AlignLeft | Qt::AlignVCenter, text, &br);
    p.restore();

    return br.width();
  }

  // -----------------------------------------------------------------------------------------------
  int drawKeyEvent(int startX, QPainter& p, const QStyleOption& option, const KeyEvent& ke)
  {
    if (ke.empty()) return 0;

    const auto text = QString("[%1%2]")
                        .arg(ke.back().code, 0, 16)
                        .arg(ke.back().value ? QChar(0x2193) : QChar(0x2191));

    #if (QT_VERSION >= QT_VERSION_CHECK(5, 11, 0))
      const auto width = option.fontMetrics.horizontalAdvance(text);
    #else
      const auto width = option.fontMetrics.width(text);
    #endif

    const auto r = QRect(startX + option.rect.left(), option.rect.top(),
                         width, option.rect.height());

    p.save();

    if (option.state & QStyle::State_Selected)
      p.setPen(option.palette.color(QPalette::HighlightedText));
    else
      p.setPen(option.palette.color(QPalette::Text));

    QRect br;
    p.drawText(r, Qt::AlignLeft | Qt::AlignVCenter, text, &br);
    p.restore();

    return br.width();
  }


  // -----------------------------------------------------------------------------------------------
  int drawKeyEventSequence(int startX, QPainter& p, const QStyleOption& option,
                           const KeyEventSequence& kes, bool drawEmptyPlaceholder = true)
  {
    if (kes.empty())
    {
      if (!drawEmptyPlaceholder) { return 0; }

      p.save();
      p.setFont([&p](){ auto f = p.font(); f.setItalic(true); return f; }());
      if (option.state & QStyle::State_Selected)
        p.setPen(option.palette.color(QPalette::Disabled, QPalette::HighlightedText));
      else
        p.setPen(option.palette.color(QPalette::Disabled, QPalette::Text));

      static const QStaticText textNone(InputSeqEdit::tr("None"));
      const auto top = (option.rect.height() - textNone.size().height()) / 2;
      p.drawStaticText(startX + option.rect.left(), option.rect.top() + top, textNone);
      p.restore();
      return textNone.size().width();
    }

    int sequenceWidth = 0;
    #if (QT_VERSION >= QT_VERSION_CHECK(5, 11, 0))
      const auto paddingX = option.fontMetrics.horizontalAdvance(' ');
    #else
      const auto paddingX = option.fontMetrics.width(' ');
    #endif

    for (auto it = kes.cbegin(); it!=kes.cend(); ++it)
    {
      if (it != kes.cbegin()) sequenceWidth += paddingX;
      sequenceWidth += drawKeyEvent(startX + sequenceWidth, p, option, *it);
    }

    return sequenceWidth;
  }
}

// -------------------------------------------------------------------------------------------------
InputSeqEdit::InputSeqEdit(QWidget* parent)
  : InputSeqEdit(nullptr, parent)
{}

// -------------------------------------------------------------------------------------------------
InputSeqEdit::InputSeqEdit(InputMapper* im, QWidget* parent)
  : QWidget(parent)
{
  setInputMapper(im);

  setFocusPolicy(Qt::StrongFocus); // Accept focus by tabbing and clicking
  setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
  setAttribute(Qt::WA_InputMethodEnabled, false);
  setAttribute(Qt::WA_MacShowFocusRect, true);
}

// -------------------------------------------------------------------------------------------------
InputSeqEdit::~InputSeqEdit()
{
}

// -------------------------------------------------------------------------------------------------
void InputSeqEdit::initStyleOption(QStyleOptionFrame& option) const
{
  option.initFrom(this);
  option.rect = contentsRect();
  option.lineWidth = style()->pixelMetric(QStyle::PM_DefaultFrameWidth, &option, this);
  option.midLineWidth = 0;
  option.state |= (QStyle::State_Sunken | QStyle::State_ReadOnly);
  option.features = QStyleOptionFrame::None;
}

// -------------------------------------------------------------------------------------------------
QSize InputSeqEdit::sizeHint() const
{
  // Adjusted from QLineEdit::sizeHint (Qt 5.9)
  ensurePolished();
  QFontMetrics fm(font());
  constexpr int verticalMargin = 3;
  constexpr int horizontalMargin = 3;
  const int h = fm.height() + 2 * verticalMargin;
  #if (QT_VERSION >= QT_VERSION_CHECK(5, 11, 0))
    const int w = fm.horizontalAdvance(QLatin1Char('x')) * 17 + 2 * horizontalMargin;
  #else
    const int w = fm.width(QLatin1Char('x')) * 17 + 2 * horizontalMargin;
  #endif

  QStyleOptionFrame opt;
  initStyleOption(opt);

  return (style()->sizeFromContents(QStyle::CT_LineEdit, &opt, QSize(w, h).
                                    expandedTo(QApplication::globalStrut()), this));
}

// -------------------------------------------------------------------------------------------------
void InputSeqEdit::paintEvent(QPaintEvent*)
{
  QStyleOptionFrame option;
  initStyleOption(option);

  QStylePainter p(this);
  p.drawPrimitive(QStyle::PE_PanelLineEdit, option);

  const bool recording = m_inputMapper && m_inputMapper->recordingMode();

  const auto& fm = option.fontMetrics;
  int xPos = (option.rect.height()-fm.height()) / 2;

  if (recording)
  {
    #if (QT_VERSION >= QT_VERSION_CHECK(5, 11, 0))
      const auto spacingX = option.fontMetrics.horizontalAdvance(' ');
    #else
      const auto spacingX = option.fontMetrics.width(' ');
    #endif

    xPos += drawRecordingSymbol(xPos, p, option) + spacingX;
    if (m_recordedSequence.empty()) {
      xPos += drawPlaceHolderText(xPos, p, option, tr("Press device button(s)...")) + spacingX;
    } else {
      xPos += drawKeyEventSequence(xPos, p, option, m_recordedSequence, false);
    }
  }
  else {
    xPos += drawKeyEventSequence(xPos, p, option, m_inputSequence);
  }
}

// -------------------------------------------------------------------------------------------------
const KeyEventSequence& InputSeqEdit::inputSequence() const
{
  return m_inputSequence;
}

// -------------------------------------------------------------------------------------------------
void InputSeqEdit::setInputSequence(const KeyEventSequence& is)
{
  if (is == m_inputSequence) return;

  m_inputSequence = is;
  update();
  emit inputSequenceChanged(m_inputSequence);
}

// -------------------------------------------------------------------------------------------------
void InputSeqEdit::clear()
{
  if (m_inputSequence.size() == 0) return;

  m_inputSequence.clear();
  update();
  emit inputSequenceChanged(m_inputSequence);
}

// -------------------------------------------------------------------------------------------------
void InputSeqEdit::mouseDoubleClickEvent(QMouseEvent* e)
{
  QWidget::mouseDoubleClickEvent(e);
  if (!m_inputMapper) return;
  e->accept();
  m_inputMapper->setRecordingMode(!m_inputMapper->recordingMode());
}

//-------------------------------------------------------------------------------------------------
void InputSeqEdit::keyPressEvent(QKeyEvent* e)
{
  if (e->key() == Qt::Key_Enter || e->key() == Qt::Key_Return)
  {
    m_inputMapper->setRecordingMode(!m_inputMapper->recordingMode());
    return;
  }
  else if (e->key() == Qt::Key_Escape)
  {
    if (m_inputMapper && m_inputMapper->recordingMode()) {
      m_inputMapper->setRecordingMode(false);
      return;
    }
  }
  else if (e->key() == Qt::Key_Delete)
  {
    if (m_inputMapper && m_inputMapper->recordingMode())
      m_inputMapper->setRecordingMode(false);
    else
      setInputSequence(KeyEventSequence{});
    return;
  }

  QWidget::keyPressEvent(e);
}

// -------------------------------------------------------------------------------------------------
void InputSeqEdit::keyReleaseEvent(QKeyEvent* e)
{
  QWidget::keyReleaseEvent(e);
}

// -------------------------------------------------------------------------------------------------
void InputSeqEdit::focusOutEvent(QFocusEvent* e)
{
  if (m_inputMapper)
    m_inputMapper->setRecordingMode(false);

  QWidget::focusOutEvent(e);
}

// -------------------------------------------------------------------------------------------------
void InputSeqEdit::setInputMapper(InputMapper* im)
{
  if (m_inputMapper == im) return;

  auto removeIm = [this](){
    if (m_inputMapper) {
      m_inputMapper->disconnect(this);
      this->disconnect(m_inputMapper);
    }
    m_inputMapper = nullptr;
  };

  removeIm();
  m_inputMapper = im;
  if (m_inputMapper == nullptr) return;

  connect(m_inputMapper, &InputMapper::destroyed, this,
  [removeIm=std::move(removeIm)](){
    removeIm();
  });

  connect(m_inputMapper, &InputMapper::recordingStarted, this, [this](){
    m_recordedSequence.clear();
  });

  connect(m_inputMapper, &InputMapper::recordingFinished, this, [this](bool canceled){
    if (!canceled) setInputSequence(m_recordedSequence);
    m_inputMapper->setRecordingMode(false);
    m_recordedSequence.clear();
  });

  connect(m_inputMapper, &InputMapper::recordingModeChanged, this, [this](bool recording){
    update();
    if (!recording) emit editingFinished(this);
  });

  connect(m_inputMapper, &InputMapper::keyEventRecorded, this, [this](const KeyEvent& ke){
    m_recordedSequence.push_back(ke);
    if (m_recordedSequence.size() >= m_maxRecordingLength) {
      setInputSequence(m_recordedSequence);
      m_inputMapper->setRecordingMode(false);
    } else {
      update();
    }
  });
}

// -------------------------------------------------------------------------------------------------
// -------------------------------------------------------------------------------------------------
void InputSeqDelegate::paint(QPainter* painter, const QStyleOptionViewItem& option,
                             const QModelIndex& index) const
{
  // Let QStyledItemDelegate handle drawing current focus inidicator and other basic stuff..
  QStyledItemDelegate::paint(painter, option, index);
  const auto imModel = qobject_cast<const InputMapConfigModel*>(index.model());
  if (!imModel) { return; }

  // Our custom drawing of the KeyEventSequence...
  const auto& fm = option.fontMetrics;
  const int xPos = (option.rect.height()-fm.height()) / 2;
  drawKeyEventSequence(xPos, *painter, option, imModel->configData(index).deviceSequence);
}

// -------------------------------------------------------------------------------------------------
QWidget* InputSeqDelegate::createEditor(QWidget* parent,
                                        const QStyleOptionViewItem& /*option*/,
                                        const QModelIndex& index) const

{
  if (const auto imModel = qobject_cast<const InputMapConfigModel*>(index.model()))
  {
    if (imModel->inputMapper()) imModel->inputMapper()->setRecordingMode(false);
    auto *editor = new InputSeqEdit(imModel->inputMapper(), parent);
    connect(editor, &InputSeqEdit::editingFinished, this, &InputSeqDelegate::commitAndCloseEditor);
    if (imModel->inputMapper()) imModel->inputMapper()->setRecordingMode(true);
    return editor;
  }

  return nullptr;
}

// -------------------------------------------------------------------------------------------------
void InputSeqDelegate::commitAndCloseEditor(InputSeqEdit* editor)
{
  emit commitData(editor);
  emit closeEditor(editor);
}

// -------------------------------------------------------------------------------------------------
void InputSeqDelegate::setEditorData(QWidget* editor, const QModelIndex& index) const
{
  if (const auto seqEditor = qobject_cast<InputSeqEdit*>(editor))
  {
    if (const auto imModel = qobject_cast<const InputMapConfigModel*>(index.model()))
    {
      seqEditor->setInputSequence(imModel->configData(index).deviceSequence);
      emit editingStarted();
      return;
    }
  }

  QStyledItemDelegate::setEditorData(editor, index);
}

// -------------------------------------------------------------------------------------------------
void InputSeqDelegate::setModelData(QWidget* editor, QAbstractItemModel* model,
                                    const QModelIndex& index) const
{
  if (const auto seqEditor = qobject_cast<InputSeqEdit*>(editor))
  {
    if (const auto imModel = qobject_cast<InputMapConfigModel*>(model))
    {
      imModel->setInputSequence(index, seqEditor->inputSequence());
      return;
    }
  }

  QStyledItemDelegate::setModelData(editor, model, index);
}

// -------------------------------------------------------------------------------------------------
QSize InputSeqDelegate::sizeHint(const QStyleOptionViewItem& option,
                                 const QModelIndex& index) const
{
  if (const auto imModel = qobject_cast<const InputMapConfigModel*>(index.model()))
  {
    // TODO calc size hint from KeyEventSequence.....
    return QStyledItemDelegate::sizeHint(option, index);
  }
  return QStyledItemDelegate::sizeHint(option, index);
}

// -------------------------------------------------------------------------------------------------
// -------------------------------------------------------------------------------------------------
void QKeySequenceDelegate::paint(QPainter* painter, const QStyleOptionViewItem& option,
                                const QModelIndex& index) const
{
  QStyledItemDelegate::paint(painter, option, index);
}

// -------------------------------------------------------------------------------------------------
QSize QKeySequenceDelegate::sizeHint(const QStyleOptionViewItem& option, const QModelIndex& index) const
{
  return QStyledItemDelegate::sizeHint(option, index);
}

// -------------------------------------------------------------------------------------------------
QWidget* QKeySequenceDelegate::createEditor(QWidget* parent,
                                           const QStyleOptionViewItem& /*option*/,
                                           const QModelIndex& index) const
{
  if (const auto imModel = qobject_cast<const InputMapConfigModel*>(index.model()))
  {
    auto *editor = new QKeySequenceEdit(imModel->configData(index).mappedSequence.keySequence(), parent);
    connect(editor, &QKeySequenceEdit::editingFinished, this, &QKeySequenceDelegate::commitAndCloseEditor);
    return editor;
  }

  return nullptr;
}

// -------------------------------------------------------------------------------------------------
void QKeySequenceDelegate::setEditorData(QWidget* editor, const QModelIndex& index) const
{
  if (const auto seqEditor = qobject_cast<QKeySequenceEdit*>(editor))
  {
    if (const auto imModel = qobject_cast<const InputMapConfigModel*>(index.model()))
    {
      seqEditor->setKeySequence(imModel->configData(index).mappedSequence.keySequence());
      return;
    }
  }

  QStyledItemDelegate::setEditorData(editor, index);
}

// -------------------------------------------------------------------------------------------------
void QKeySequenceDelegate::setModelData(QWidget* editor, QAbstractItemModel* model,
                                       const QModelIndex& index) const
{
  if (const auto seqEditor = qobject_cast<QKeySequenceEdit*>(editor))
  {
    if (const auto imModel = qobject_cast<InputMapConfigModel*>(model))
    {
//      imModel->setKeySequence(index, seqEditor->keySequence());
      return;
    }
  }

  QStyledItemDelegate::setModelData(editor, model, index);
}

// -------------------------------------------------------------------------------------------------
void QKeySequenceDelegate::commitAndCloseEditor()
{
  const auto editor = qobject_cast<QKeySequenceEdit*>(sender());
  emit commitData(editor);
  emit closeEditor(editor);
}
