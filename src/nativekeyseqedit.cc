// This file is part of Projecteur - https://github.com/jahnf/projecteur - See LICENSE.md and README.md
#include "nativekeyseqedit.h"

#include "logging.h"

#include <linux/input.h>

#include <QApplication>
#include <QKeyEvent>
#include <QStaticText>
#include <QStyleOptionFrame>
#include <QStylePainter>
#include <QTimer>

namespace {
  // -----------------------------------------------------------------------------------------------
  constexpr int maxKeyCount = 4; // Same as QKeySequence

  // -----------------------------------------------------------------------------------------------
  QKeySequence makeQKeySequence(const std::vector<int>& keys) {
    switch (keys.size()) {
    case 4: return QKeySequence(keys[0], keys[1], keys[2], keys[3]);
    case 3: return QKeySequence(keys[0], keys[1], keys[2]);
    case 2: return QKeySequence(keys[0], keys[1]);
    case 1: return QKeySequence(keys[0]);
    }
    return QKeySequence();
  }

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
    const auto r = QRect(startX + option.rect.left(), option.rect.top(),
                         option.fontMetrics.width(text), option.rect.height());

    p.save();
    p.setPen(option.palette.color(QPalette::Disabled, QPalette::Text));
    QRect br;
    p.drawText(r, Qt::AlignLeft | Qt::AlignVCenter, text, &br);
    p.restore();

    return br.width();
  }

  // -----------------------------------------------------------------------------------------------
  int drawText(int startX, QPainter& p, const QStyleOption& option, const QString& text)
  {
    const auto r = QRect(startX + option.rect.left(), option.rect.top(),
                         option.fontMetrics.width(text), option.rect.height());

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
  int drawQKeySeqText(int startX, QPainter& p, const QStyleOption& option, const QKeySequence& qks,
                    bool drawEmptyPlaceholder = true)
  {
    if (qks.count() == 0)
    {
      if (!drawEmptyPlaceholder) { return 0; }

      p.save();
      p.setFont([&p](){ auto f = p.font(); f.setItalic(true); return f; }());
      if (option.state & QStyle::State_Selected)
        p.setPen(option.palette.color(QPalette::Disabled, QPalette::HighlightedText));
      else
        p.setPen(option.palette.color(QPalette::Disabled, QPalette::Text));

      static const QStaticText textNone(NativeKeySeqEdit::tr("None"));
      const auto top = (option.rect.height() - textNone.size().height()) / 2;
      p.drawStaticText(startX + option.rect.left(), option.rect.top() + top, textNone);
      p.restore();
      return textNone.size().width();
    }

    return drawText(startX, p, option, qks.toString());
  }
}

// -------------------------------------------------------------------------------------------------
NativeKeySequence::NativeKeySequence(QKeySequence&& ks, KeyEventSequence&& kes)
  : m_keySequence(std::move(ks))
  , m_nativeSequence(std::move(kes))
{}

// -------------------------------------------------------------------------------------------------
bool NativeKeySequence::operator==(const NativeKeySequence &other) const
{
  return m_keySequence == other.m_keySequence && m_nativeSequence == other.m_nativeSequence;
}

// -------------------------------------------------------------------------------------------------
bool NativeKeySequence::operator!=(const NativeKeySequence &other) const
{
  return m_keySequence != other.m_keySequence || m_nativeSequence != other.m_nativeSequence;
}

// -------------------------------------------------------------------------------------------------
void NativeKeySequence::clear()
{
  m_keySequence = QKeySequence{};
  m_nativeSequence.clear();
}

void NativeKeySequence::swap(NativeKeySequence& other)
{
  m_keySequence.swap(other.m_keySequence);
  m_nativeSequence.swap(other.m_nativeSequence);
}

// -------------------------------------------------------------------------------------------------
// -------------------------------------------------------------------------------------------------
NativeKeySeqEdit::NativeKeySeqEdit(QWidget* parent)
  : QWidget(parent)
  , m_timer(new QTimer(this))
{
  setFocusPolicy(Qt::StrongFocus); // Accept focus by tabbing and clicking
  setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
  setAttribute(Qt::WA_InputMethodEnabled, false);
  setAttribute(Qt::WA_MacShowFocusRect, true);

  m_timer->setSingleShot(true);
  m_timer->setInterval(950);
  connect(m_timer, &QTimer::timeout, this, [this](){ setRecording(false); });
}

// -------------------------------------------------------------------------------------------------
const NativeKeySequence& NativeKeySeqEdit::keySequence() const
{
  return m_nativeSequence;
}

// -------------------------------------------------------------------------------------------------
void NativeKeySeqEdit::setKeySequence(const NativeKeySequence& nks)
{
  if (nks == m_nativeSequence) return;

  m_nativeSequence = nks;
  update();
  emit keySequenceChanged(m_nativeSequence);
}

// -------------------------------------------------------------------------------------------------
void NativeKeySeqEdit::clear()
{
  if (m_nativeSequence.count() == 0) return;

  m_nativeSequence.clear();
  update();
  emit keySequenceChanged(m_nativeSequence);
}

// -------------------------------------------------------------------------------------------------
void NativeKeySeqEdit::initStyleOption(QStyleOptionFrame& option) const
{
  option.initFrom(this);
  option.rect = contentsRect();
  option.lineWidth = style()->pixelMetric(QStyle::PM_DefaultFrameWidth, &option, this);
  option.midLineWidth = 0;
  option.state |= (QStyle::State_Sunken | QStyle::State_ReadOnly);
  option.features = QStyleOptionFrame::None;
}

// -------------------------------------------------------------------------------------------------
QSize NativeKeySeqEdit::sizeHint() const
{
  // Adjusted from QLineEdit::sizeHint (Qt 5.9)
  ensurePolished();

  QStyleOptionFrame opt;
  initStyleOption(opt);

  constexpr int verticalMargin = 3;
  constexpr int horizontalMargin = 3;
  const int h = opt.fontMetrics.height() + 2 * verticalMargin;
  const int w = std::max(opt.fontMetrics.width(QLatin1Char('x')) * 17 + 2 * horizontalMargin,
                         opt.fontMetrics.width(m_nativeSequence.keySequence().toString()));

  return (style()->sizeFromContents(QStyle::CT_LineEdit, &opt, QSize(w, h).
                                    expandedTo(QApplication::globalStrut()), this));
}

// -------------------------------------------------------------------------------------------------
void NativeKeySeqEdit::paintEvent(QPaintEvent*)
{
  QStyleOptionFrame option;
  initStyleOption(option);

  QStylePainter p(this);
  p.drawPrimitive(QStyle::PE_PanelLineEdit, option);

  const auto& fm = option.fontMetrics;
  int xPos = (option.rect.height()-fm.height()) / 2;
  if (recording())
  {
    const auto spacingX = option.fontMetrics.width(' ');
    xPos += drawRecordingSymbol(xPos, p, option) + spacingX;
    if (m_recordedKeys.empty()) {
      xPos += drawPlaceHolderText(xPos, p, option, tr("Press shortcut..."));
    } else {
      xPos += drawQKeySeqText(xPos, p, option, m_recordedSequence, false);
      xPos += drawText(xPos, p, option, ", ...");
    }
  }
  else {
    xPos += drawQKeySeqText(xPos, p, option, m_nativeSequence.keySequence());
  }
}

// -------------------------------------------------------------------------------------------------
void NativeKeySeqEdit::mouseDoubleClickEvent(QMouseEvent* e)
{
  QWidget::mouseDoubleClickEvent(e);
  setRecording(!recording());
}

// -------------------------------------------------------------------------------------------------
void NativeKeySeqEdit::reset()
{
  m_timer->stop();
  m_recordedKeys.clear();
  m_recordedEvents.clear();
  m_lastKey = -1;
  m_recordedSequence = QKeySequence();
  m_nativeModifiers.clear();
}

// -------------------------------------------------------------------------------------------------
void NativeKeySeqEdit::setRecording(bool doRecord)
{
  if (m_recording == doRecord) return;

  m_recording = doRecord;

  if (m_recording)
  { // started recording mode
    reset();
  }
  else if (m_recordedKeys.size() > 0)
  { // finished recording
    NativeKeySequence recorded(std::move(m_recordedSequence), std::move(m_recordedEvents));
    if (recorded != m_nativeSequence) {
      m_nativeSequence.swap(recorded);
      emit keySequenceChanged(m_nativeSequence);
    }
    reset();
  }
  update();
  emit recordingChanged(m_recording);
}

//-------------------------------------------------------------------------------------------------
bool NativeKeySeqEdit::event(QEvent* e)
{
  switch (e->type())
  {
  case QEvent::KeyPress: {
    const auto ke = static_cast<QKeyEvent*>(e);
    if (recording() && (ke->key() == Qt::Key_Tab || ke->key() == Qt::Key_Backtab))
    {
      keyPressEvent(ke);
      e->accept();
      return true;
    }
    break;
  }
  case QEvent::Shortcut:
    return true;
  case QEvent::ShortcutOverride:
    e->accept();
    return true;
  default :
    break;
  }
  return QWidget::event(e);
}

//-------------------------------------------------------------------------------------------------
void NativeKeySeqEdit::recordKeyPressEvent(QKeyEvent* e)
{
  int key = m_lastKey = e->key();

  if (key == Qt::Key_Control
      || key == Qt::Key_Shift
      || key == Qt::Key_Meta
      || key == Qt::Key_Alt
      || key == Qt::Key_AltGr)
  {
    m_nativeModifiers.push_back(e->nativeScanCode() - 8); // See comment below about the -8;
    return;
  }

  if (key == Qt::Key_unknown) {
    return;
  }

  if (m_recordedKeys.size() >= maxKeyCount) {
    setRecording(false);
    return;
  }

  key |= translateModifiers(e->modifiers());

  m_recordedKeys.push_back(key);
  m_recordedSequence = makeQKeySequence(m_recordedKeys);

  // TODO Verify that (nativeScanCode - 8) equals the codes from input-event-codes.h on
  // all Linux desktops.. (not only xcb..) - comes from #define MIN_KEYCODE 8 in evdev.c
  KeyEvent pressed; KeyEvent released;
  for (const auto modifierKey : m_nativeModifiers)
  {
    pressed.emplace_back(EV_KEY, modifierKey, 1);
    released.emplace_back(EV_KEY, modifierKey, 0);
  }
  pressed.emplace_back(EV_KEY, e->nativeScanCode()-8, 1);
  released.emplace_back(EV_KEY, e->nativeScanCode()-8, 0);
  pressed.emplace_back(EV_SYN, SYN_REPORT, 0);
  released.emplace_back(EV_SYN, SYN_REPORT, 0);

  m_recordedEvents.emplace_back(std::move(pressed));
  m_recordedEvents.emplace_back(std::move(released));

  m_nativeModifiers.clear();

  update();
  e->accept();

  if (m_recordedKeys.size() >= maxKeyCount) {
    setRecording(false);
  }
}

//-------------------------------------------------------------------------------------------------
void NativeKeySeqEdit::keyPressEvent(QKeyEvent* e)
{
  if (!recording())
  {
    if (e->key() == Qt::Key_Enter || e->key() == Qt::Key_Return)
    {
      setRecording(true);
      return;
    }
    else if (e->key() == Qt::Key_Delete)
    {
      clear();
      return;
    }
    QWidget::keyPressEvent(e);
    return;
  }

  recordKeyPressEvent(e);
}

//-------------------------------------------------------------------------------------------------
void NativeKeySeqEdit::keyReleaseEvent(QKeyEvent* e)
{
  if (recording())
  {
    if (m_recordedKeys.size() && m_lastKey == e->key()) {
      if (m_recordedKeys.size() < maxKeyCount) {
        m_timer->start();
      } else {
        setRecording(false);
      }
    }
    return;
  }

  QWidget::keyReleaseEvent(e);
}

//-------------------------------------------------------------------------------------------------
void NativeKeySeqEdit::focusOutEvent(QFocusEvent* e)
{
  setRecording(false);
  QWidget::focusOutEvent(e);
}

int NativeKeySeqEdit::translateModifiers(Qt::KeyboardModifiers state)
{
  int result = 0;
  if (state & Qt::ControlModifier)    result |= Qt::ControlModifier;
  if (state & Qt::MetaModifier)       result |= Qt::MetaModifier;
  if (state & Qt::AltModifier)        result |= Qt::AltModifier;
  if (state & Qt::ShiftModifier)      result |= Qt::ShiftModifier;
  if (state & Qt::GroupSwitchModifier)  result |= Qt::GroupSwitchModifier;
  return result;
}

