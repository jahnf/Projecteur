#include "qglobalshortcutx11.h"

#include <QCoreApplication>
#include <QX11Info>

#include <xcb/xcb.h>
#include <X11/Xlib.h>
#include <X11/keysym.h>


namespace {
  xcb_keycode_t toNativeKeycode(const Qt::Key k)
  {
    quint32 key = 0;
    if (k >= Qt::Key_F1 && k <= Qt::Key_F35) {
        key = XK_F1 + (k - Qt::Key_F1);
    } else if ((k >= Qt::Key_Space && k <= Qt::Key_QuoteLeft)
               || (k >= Qt::Key_BraceLeft && k <= Qt::Key_AsciiTilde)
               || (k >= Qt::Key_nobreakspace && k <= Qt::Key_ydiaeresis)) {
        key = k;
    }
    else
    {
      switch (k) {
      case Qt::Key_Escape:
        key = XK_Escape;
        break;
      case Qt::Key_Tab:
      case Qt::Key_Backtab:
        key = XK_Tab;
        break;
      case Qt::Key_Backspace:
        key = XK_BackSpace;
        break;
      case Qt::Key_Return:
      case Qt::Key_Enter:
        key = XK_Return;
        break;
      case Qt::Key_Insert:
        key = XK_Insert;
        break;
      case Qt::Key_Delete:
        key = XK_Delete;
        break;
      case Qt::Key_Pause:
        key = XK_Pause;
        break;
      case Qt::Key_Print:
        key = XK_Print;
        break;
      case Qt::Key_SysReq:
        key = XK_Sys_Req;
        break;
      case Qt::Key_Clear:
        key = XK_Clear;
        break;
      case Qt::Key_Home:
        key = XK_Home;
        break;
      case Qt::Key_End:
        key = XK_End;
        break;
      case Qt::Key_Left:
        key = XK_Left;
        break;
      case Qt::Key_Up:
        key = XK_Up;
        break;
      case Qt::Key_Right:
        key = XK_Right;
        break;
      case Qt::Key_Down:
        key = XK_Down;
        break;
      case Qt::Key_PageUp:
        key = XK_Page_Up;
        break;
      case Qt::Key_PageDown:
        key = XK_Page_Down;
        break;
      default:
        key = 0;
      }
    }
    return XKeysymToKeycode(QX11Info::display(), key);
  }

  quint16 toNativeModifiers(Qt::KeyboardModifiers m)
  {
    quint16 mods = Qt::NoModifier;
    if (m & Qt::ShiftModifier)
      mods |= ShiftMask;
    if (m & Qt::ControlModifier)
      mods |= ControlMask;
    if (m & Qt::AltModifier)
      mods |= Mod1Mask;
    if (m & Qt::MetaModifier)
      mods |= Mod4Mask;
    return mods;
  }

  Qt::Key getKey(const QKeySequence& keyseq)
  {
    if (keyseq.isEmpty())
      return Qt::Key(0);

    return Qt::Key(static_cast<unsigned>(keyseq[0]) & ~Qt::KeyboardModifierMask);
  }

  Qt::KeyboardModifiers getMods(const QKeySequence& keyseq)
  {
    if (keyseq.isEmpty())
      return Qt::KeyboardModifiers(Qt::NoModifier);

    return Qt::KeyboardModifiers(static_cast<unsigned>(keyseq[0]) & Qt::KeyboardModifierMask);
  }

  void X11registerKey(xcb_keycode_t k, quint16 m)
  {
    xcb_grab_key(QX11Info::connection(), 1, static_cast<xcb_window_t>(QX11Info::appRootWindow()),
                 m, k, XCB_GRAB_MODE_ASYNC, XCB_GRAB_MODE_ASYNC);
  }

  void X11unregisterKey(xcb_keycode_t k, quint16 m)
  {
    if(const auto rw = QX11Info::appRootWindow()) {
      xcb_ungrab_key(QX11Info::connection(), k,
                     static_cast<xcb_window_t>(QX11Info::appRootWindow()), m);
    }
  }
}

QGlobalShortcutX11::QGlobalShortcutX11(QObject* parent)
  : QGlobalShortcutX11(QKeySequence(), parent)
{
}

QGlobalShortcutX11::QGlobalShortcutX11(const QKeySequence& keyseq, QObject* parent)
  : QObject(parent)
{
  setKey(keyseq);
  connect(qApp, &QCoreApplication::aboutToQuit, [this](){ unsetKey(); });
}

QGlobalShortcutX11::~QGlobalShortcutX11()
{
  unsetKey();
}

QKeySequence QGlobalShortcutX11::key() const
{
  return m_keySeq;
}

void QGlobalShortcutX11::setKey(const QKeySequence& keyseq)
{
  if (m_keySeq == keyseq)
    return;

  if (keyseq.isEmpty()) {
    unsetKey();
  }
  else
  {
    const auto keycode = toNativeKeycode(getKey(keyseq));
    const auto mods = toNativeModifiers(getMods(keyseq));
    X11registerKey(keycode, mods);
    qApp->installNativeEventFilter(this);
    m_keySeq = keyseq;
    m_keyCode = keycode;
    m_keyMods = mods;
  }
}

void QGlobalShortcutX11::unsetKey()
{
  if (m_keySeq.isEmpty())
    return;

  qApp->removeNativeEventFilter(this);
  X11unregisterKey(m_keyCode, m_keyMods);
  m_keySeq = QKeySequence();
}

bool QGlobalShortcutX11::nativeEventFilter(const QByteArray& evType, void* msg, long* result)
{
  Q_UNUSED(evType)
  Q_UNUSED(result)

  const xcb_generic_event_t* e = static_cast<xcb_generic_event_t*>(msg);
  if ((e->response_type & ~0x80) == XCB_KEY_PRESS)
  {
    const xcb_key_press_event_t* ke = reinterpret_cast<const xcb_key_press_event_t*>(e);
//    xcb_get_keyboard_mapping_reply_t rep;
//    xcb_keysym_t* k = xcb_get_keyboard_mapping_keysyms(&rep);
    const xcb_keycode_t keycode = ke->detail;
    const quint16 mods = ke->state & (ShiftMask|ControlMask|Mod1Mask|Mod3Mask);

    if (m_keyCode == keycode && m_keyMods == mods)
      emit activated();
  }
  return false;
}


