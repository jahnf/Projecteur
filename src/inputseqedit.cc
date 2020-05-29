// This file is part of Projecteur - https://github.com/jahnf/projecteur - See LICENSE.md and README.md
#include "inputseqedit.h"

#include "deviceinput.h"
#include "logging.h"

#include <QLineEdit>
#include <QPaintEvent>
#include <QStyleOptionFrame>
#include <QStylePainter>
#include <QVBoxLayout>

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
  setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
  setAttribute(Qt::WA_InputMethodEnabled, false);
  setAttribute(Qt::WA_MacShowFocusRect, true);

  // TODO: Start recording on "Enter-Key", DblClick ....
  // TODO: Cancel recording on focus out, or ESC
}

// -------------------------------------------------------------------------------------------------
void InputSeqEdit::initStyleOption(QStyleOptionFrame& option) const
{
  option.initFrom(this);
  option.rect = contentsRect();
  option.lineWidth = style()->pixelMetric(QStyle::PM_DefaultFrameWidth, &option, this);
  option.midLineWidth = 0;
  option.state |= QStyle::State_Sunken;
  option.state |= QStyle::State_ReadOnly; // TODO: is this necessary? (borrowed from QLineEdit)
  option.features = QStyleOptionFrame::None;
}

// -------------------------------------------------------------------------------------------------
void InputSeqEdit::paintEvent(QPaintEvent*)
{
  QStyleOptionFrame option;
  initStyleOption(option);

  QStylePainter p(this);
  p.drawPrimitive(QStyle::PE_PanelLineEdit, option);

  // TODO: Paint KeyEventSequence...
  // TODO: If recording show record indicator
  //       + placeholder text if nothing recorded yet || recorded events in current recording.
  if (m_inputMapper && m_inputMapper->recordingMode())
  {
    p.setPen(Qt::red);
    p.setBrush(QBrush(Qt::red));
    p.drawRect(5,5,16,16);
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
void InputSeqEdit::mouseDoubleClickEvent(QMouseEvent* e)
{
  QWidget::mouseDoubleClickEvent(e);
  if (!m_inputMapper) return;

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
    if (m_inputMapper) m_inputMapper->setRecordingMode(false);
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
void InputSeqEdit::focusOutEvent(QFocusEvent*)
{
  if (m_inputMapper) m_inputMapper->setRecordingMode(false);
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

  // TODO connect signals from/to
  connect(m_inputMapper, &InputMapper::recordingStarted, this, [this](){
    qDebug() << "Recording started...";
    m_recordedSequence.clear();
  });

  connect(m_inputMapper, &InputMapper::recordingFinished, this, [this](bool canceled){
    qDebug() << "Recording finished..." << canceled;
    m_inputMapper->setRecordingMode(false);
    if (!canceled) setInputSequence(m_recordedSequence);
  });

  connect(m_inputMapper, &InputMapper::recordingModeChanged, this, [this](bool recording){
    qDebug() << "Recording mode... " << recording;
    update();
  });

  connect(m_inputMapper, &InputMapper::keyEventRecorded, this, [this](const KeyEvent& ke){
    qDebug() << "Recorded... " << ke;
    m_recordedSequence.push_back(ke);
    if (m_recordedSequence.size() >= m_maxRecordingLength) {
      setInputSequence(m_recordedSequence);
      m_inputMapper->setRecordingMode(false);
    }
  });
}
