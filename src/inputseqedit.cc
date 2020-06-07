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

#include <linux/input.h>

namespace {
  // -----------------------------------------------------------------------------------------------
  // Returns true if the second Key event equals the first one, with the first one
  // being the press and the second the release event.
  bool isButtonTap(const KeyEvent& first, const KeyEvent& second)
  {
    auto it1 = first.cbegin();
    const auto end1 = first.cend();
    auto it2 = second.cbegin();
    const auto end2 = second.cend();
    for( ; it1 != end1 && it2 != end2; ++it1, ++it2)
    {
      if (it1->type == EV_KEY) {
        if (it2->type != EV_KEY
            || it1->code != it2->code
            || it1->value != 1  // key event 1 press
            || it2->value != 0) // key event 2 release
        {
          return false;
        }
      }
      else if (*it1 != *it2) {
        return false;
      }
    }
    return (it1 == end1 && it2 == end2);
  }

  // -----------------------------------------------------------------------------------------------
  int drawKeyEvent(int startX, QPainter& p, const QStyleOption& option, const KeyEvent& ke,
                   bool buttonTap = false)
  {
    if (ke.empty()) return 0;

    static auto const pressChar = QChar(0x2193); // ↓
    static auto const releaseChar = QChar(0x2191); // ↑

    // TODO some devices (e.g. August WP 200) have buttons that send a key combination
    //      (modifiers + key) - this is ignored completely right now.
    const auto text = QString("[%1%2%3")
                         .arg(ke.back().code, 0, 16)
                         .arg(buttonTap ? pressChar
                                        : ke.back().value ? pressChar : releaseChar)
                         .arg(buttonTap ? "" : "]");

    const auto r = QRect(QPoint(startX + option.rect.left(), option.rect.top()),
                         option.rect.bottomRight());

    p.save();

    if (option.state & QStyle::State_Selected)
      p.setPen(option.palette.color(QPalette::HighlightedText));
    else
      p.setPen(option.palette.color(QPalette::Text));

    QRect br;
    p.drawText(r, Qt::AlignLeft | Qt::AlignVCenter, text, &br);

    if (buttonTap)
    {
      QRect br2; // draw down and up arrow closer together
      const auto t2 = QString("%2]").arg(releaseChar);
      const auto w = option.fontMetrics.rightBearing(pressChar)
                   + option.fontMetrics.leftBearing(releaseChar);
      p.drawText(r.adjusted(br.width() - w, 0, 0, 0), Qt::AlignLeft | Qt::AlignVCenter, t2, &br2);
      br.setWidth(br.width() + br2.width());
    }

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
      return InputSeqEdit::drawEmptyIndicator(startX, p, option);
    }

    int sequenceWidth = 0;
    const int paddingX = QStaticText(" ").size().width();
    for (auto it = kes.cbegin(); it!=kes.cend(); ++it)
    {
      if (it != kes.cbegin()) sequenceWidth += paddingX;
      if (startX + sequenceWidth >= option.rect.width()) break;

      const bool isTap = [&]()
      { // Check if this event and the next event represent a button press & release
        const auto next = std::next(it);
        if (next != kes.cend() && isButtonTap(*it, *next)) {
          it = next;
          return true;
        }
        return false;
      }();

      sequenceWidth += drawKeyEvent(startX + sequenceWidth, p, option, *it, isTap);
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
    const auto spacingX = QStaticText(" ").size().width();
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
int InputSeqEdit::drawRecordingSymbol(int startX, QPainter& p, const QStyleOption& option)
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

// -------------------------------------------------------------------------------------------------
int InputSeqEdit::drawPlaceHolderText(int startX, QPainter& p, const QStyleOption& option, const QString& text)
{
  const auto r = QRect(QPoint(startX + option.rect.left(), option.rect.top()),
                       option.rect.bottomRight());

  p.save();
  p.setPen(option.palette.color(QPalette::Disabled, QPalette::Text));
  QRect br;
  p.drawText(r, Qt::AlignLeft | Qt::AlignVCenter, text, &br);
  p.restore();

  return br.width();
}

// -------------------------------------------------------------------------------------------------
int InputSeqEdit::drawEmptyIndicator(int startX, QPainter& p, const QStyleOption& option)
{
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
