// This file is part of Projecteur - https://github.com/jahnf/projecteur
// - See LICENSE.md and README.md

#include "deviceinput.h"

#include "enum-helper.h"
#include "logging.h"
#include "settings.h"
#include "virtualdevice.h"

#include <algorithm>
#include <list>
#include <type_traits>

#include <QTimer>

#include <linux/input.h>

LOGGING_CATEGORY(input, "input")

namespace  {
  // -----------------------------------------------------------------------------------------------
  #if (QT_VERSION < QT_VERSION_CHECK(6, 0, 0))
  const auto registered_ = qRegisterMetaTypeStreamOperators<KeyEventSequence>()
                           && qRegisterMetaTypeStreamOperators<MappedAction>();
  #endif


  // -----------------------------------------------------------------------------------------------
  void addKeyToString(QString& str, const QString& key)
  {
    if (!str.isEmpty()) { str += QLatin1Char('+'); }
    str += key;
  }

  // -----------------------------------------------------------------------------------------------
  QKeySequence makeQKeySequence(const std::vector<int>& keys)
  {
    switch (keys.size()) {
    case 4: return QKeySequence(keys[0], keys[1], keys[2], keys[3]);
    case 3: return QKeySequence(keys[0], keys[1], keys[2]);
    case 2: return QKeySequence(keys[0], keys[1]);
    case 1: return QKeySequence(keys[0]);
    }
    return QKeySequence();
  }

  // -----------------------------------------------------------------------------------------------
  KeyEventSequence makeSpecialKeyEventSequence(uint16_t code)
  {
    // Special key event with 3 button presses of the same key,
    // which should not be able with real events
    KeyEvent pressed {
      {EV_KEY, code, 1},
      {EV_KEY, code, 1},
      {EV_KEY, code, 1},
    };

    return KeyEventSequence{std::move(pressed)};
  };
} // end anonymous namespace

// -------------------------------------------------------------------------------------------------
DeviceInputEvent::DeviceInputEvent(const struct input_event& ie)
  : type(ie.type), code(ie.code), value(ie.value) {}

bool DeviceInputEvent::operator==(const DeviceInputEvent& o) const {
  return std::tie(type,code,value) == std::tie(o.type,o.code,o.value);
}

bool DeviceInputEvent::operator!=(const DeviceInputEvent& o) const {
  return std::tie(type,code,value) != std::tie(o.type,o.code,o.value);
}

bool DeviceInputEvent::operator==(const input_event& o) const {
  return std::tie(type,code,value) == std::tie(o.type,o.code,o.value);
}

bool DeviceInputEvent::operator<(const DeviceInputEvent& o) const {
  return std::tie(type,code,value) < std::tie(o.type,o.code,o.value);
}

bool DeviceInputEvent::operator<(const input_event& o) const {
  return std::tie(type,code,value) < std::tie(o.type,o.code,o.value);
}

// -------------------------------------------------------------------------------------------------
QDataStream& operator<<(QDataStream& s, const DeviceInputEvent& die) {
  return s << die.type << die.code <<die.value;
}

QDataStream& operator>>(QDataStream& s, DeviceInputEvent& die) {
  return s >> die.type >> die.code >> die.value;
}

// -------------------------------------------------------------------------------------------------
QDebug operator<<(QDebug debug, const DeviceInputEvent &ie)
{
  QDebugStateSaver saver(debug);
  debug.nospace() << '{' << ie.type << ", " << ie.code << ", " << ie.value << '}';
  return debug;
}

// -------------------------------------------------------------------------------------------------
QDebug operator<<(QDebug debug, const KeyEvent &ke)
{
  QDebugStateSaver saver(debug);
  debug.nospace() << "[";
  for (const auto& e : ke) {
    debug.nospace() << e << ',';
  }
  debug.nospace() << "]";
  return debug;
}

// -------------------------------------------------------------------------------------------------
std::shared_ptr<ScrollHorizontalAction> GlobalActions::scrollHorizontal()
{
  static auto scrollHorizontalAction = std::make_shared<ScrollHorizontalAction>();
  return scrollHorizontalAction;
}

// -------------------------------------------------------------------------------------------------
std::shared_ptr<ScrollVerticalAction> GlobalActions::scrollVertical()
{
  static auto scrollVerticalAction = std::make_shared<ScrollVerticalAction>();
  return scrollVerticalAction;
}

// -------------------------------------------------------------------------------------------------
std::shared_ptr<VolumeControlAction> GlobalActions::volumeControl()
{
  static auto volumeControlAction = std::make_shared<VolumeControlAction>();
  return volumeControlAction;
}

// -------------------------------------------------------------------------------------------------
QDataStream& operator>>(QDataStream& s, MappedAction& mia)
{
  const auto type = [&s](){
    auto type = to_integral(Action::Type::KeySequence);
    s >> type;
    return type;
  }();

  switch (to_enum<Action::Type>(type))
  {
  case Action::Type::KeySequence:
    mia.action = std::make_shared<KeySequenceAction>();
    return mia.action->load(s);
  case Action::Type::CyclePresets:
    mia.action = std::make_shared<CyclePresetsAction>();
    return mia.action->load(s);
  case Action::Type::ToggleSpotlight:
    mia.action = std::make_shared<ToggleSpotlightAction>();
    return mia.action->load(s);
  case Action::Type::ScrollHorizontal:
    mia.action = std::make_shared<ScrollHorizontalAction>();
    return mia.action->load(s);
  case Action::Type::ScrollVertical:
    mia.action = std::make_shared<ScrollVerticalAction>();
    return mia.action->load(s);
  case Action::Type::VolumeControl:
    mia.action = std::make_shared<VolumeControlAction>();
    return mia.action->load(s);
  }
  return s;
}

// -------------------------------------------------------------------------------------------------
bool MappedAction::operator==(const MappedAction& o) const
{
  if (!action && !o.action) { return true; }
  if (!action || !o.action) { return false; }
  if (action->type() != o.action->type()) { return false; }

  switch(action->type()) {
  case Action::Type::KeySequence:
    return (*static_cast<KeySequenceAction*>(action.get()))
           == (*static_cast<KeySequenceAction*>(o.action.get()));
  case Action::Type::CyclePresets:
    return (*static_cast<CyclePresetsAction*>(action.get()))
           == (*static_cast<CyclePresetsAction*>(o.action.get()));
  case Action::Type::ToggleSpotlight:
    return (*static_cast<ToggleSpotlightAction*>(action.get()))
           == (*static_cast<ToggleSpotlightAction*>(o.action.get()));
  case Action::Type::ScrollHorizontal:
    return (*static_cast<ScrollHorizontalAction*>(action.get()))
           == (*static_cast<ScrollHorizontalAction*>(o.action.get()));
  case Action::Type::ScrollVertical:
    return (*static_cast<ScrollVerticalAction*>(action.get()))
           == (*static_cast<ScrollVerticalAction*>(o.action.get()));
  case Action::Type::VolumeControl:
    return (*static_cast<VolumeControlAction*>(action.get()))
           == (*static_cast<VolumeControlAction*>(o.action.get()));
  }

  return false;
}

// -------------------------------------------------------------------------------------------------
QDataStream& operator<<(QDataStream& s, const MappedAction& mia) {
  s << static_cast<std::underlying_type_t<Action::Type>>(mia.action->type());
  return mia.action->save(s);
}

// -------------------------------------------------------------------------------------------------
namespace  {
  struct KeyEventItem {
    explicit KeyEventItem(KeyEvent ke = {}) : keyEvent(std::move(ke)) {}
    const KeyEvent keyEvent;
    std::shared_ptr<Action> action;
    std::vector<KeyEventItem*> nextMap;
  };

  struct DeviceKeyMap
  {
    explicit DeviceKeyMap(const InputMapConfig& config = {}) { reconfigure(config); }

    enum Result : uint8_t {
      Miss, Valid, Hit, PartialHit
    };

    Result feed(const struct input_event input_events[], size_t num);

    auto state() const { return m_pos; }
    void resetState();
    void reconfigure(const InputMapConfig& config = {});
    bool hasConfig() const { return !m_rootItem.nextMap.empty(); }

  private:
    std::list<KeyEventItem> m_items;
    KeyEventItem m_rootItem;
    const KeyEventItem* m_pos = &m_rootItem;
  };
} // end anonymous namespace

// -------------------------------------------------------------------------------------------------
DeviceKeyMap::Result DeviceKeyMap::feed(const struct input_event input_events[], size_t num)
{
  if (!hasConfig()) { return Result::Miss; }
  if (!m_pos) { return Result::Miss; }

  const auto ke = KeyEvent(KeyEvent(input_events, input_events + num));
  const auto& nextMap = m_pos->nextMap;
  const auto find_it = std::find_if(nextMap.cbegin(), nextMap.cend(),
  [&ke](KeyEventItem const* next) {
    return next && ke == next->keyEvent;
  });

  if (find_it == nextMap.cend()) { return Result::Miss; }

  m_pos = (*find_it);

  // Last KeyEvent in possible sequence...
  if (m_pos->nextMap.empty()) {
    return Result::Hit;
  }

  // KeyEvent in Sequence has action attached, but there are other possible sequences...
  if (m_pos->action) {
    return Result::PartialHit;
  }

  return Result::Valid;
}

// -------------------------------------------------------------------------------------------------
void DeviceKeyMap::resetState()
{
  m_pos = &m_rootItem;
}

// -------------------------------------------------------------------------------------------------
void DeviceKeyMap::reconfigure(const InputMapConfig& config)
{
  // -- clear maps + state
  resetState();
  m_rootItem.nextMap.clear();
  m_items.clear();

    // -- fill keymaps
  for (const auto& configItem : config)
  {
    // sanity check
    if (!configItem.second.action) { continue; }

    KeyEventItem* previous = nullptr;
    KeyEventItem* current = &m_rootItem;
    const auto& kes = configItem.first;

    for (size_t i = 0; i < kes.size(); ++i) {
      const auto& keyEvent = kes[i];
      const auto it = std::find_if(current->nextMap.cbegin(), current->nextMap.cend(),
      [&keyEvent](const KeyEventItem* item) {
        return (item && item->keyEvent == keyEvent);
      });

      previous = current;

      if (it != current->nextMap.cend()) {
        current = *it;
      }
      else {
        // Create new item if not found
        m_items.emplace_back(KeyEventItem{keyEvent});
        current = &m_items.back();
        // link previous to current
        previous->nextMap.push_back(current);
      }

      // if last item in key event sequence
      if (i == kes.size() - 1) {
        current->action = configItem.second.action;
      }
    }
  }
}

// -------------------------------------------------------------------------------------------------
// -------------------------------------------------------------------------------------------------
NativeKeySequence::NativeKeySequence() = default;

// -------------------------------------------------------------------------------------------------
NativeKeySequence::NativeKeySequence(const std::vector<int>& qtKeys,
                                     std::vector<uint16_t>&& nativeModifiers,
                                     KeyEventSequence&& kes)
  : m_keySequence(makeQKeySequence(qtKeys))
  , m_nativeSequence(std::move(kes))
  , m_nativeModifiers(std::move(nativeModifiers))
{
}

// -------------------------------------------------------------------------------------------------
bool NativeKeySequence::operator==(const NativeKeySequence &other) const
{
  return m_keySequence == other.m_keySequence
         && m_nativeSequence == other.m_nativeSequence
         && m_nativeModifiers == other.m_nativeModifiers;
}

// -------------------------------------------------------------------------------------------------
bool NativeKeySequence::operator!=(const NativeKeySequence &other) const
{
  return m_keySequence != other.m_keySequence
         || m_nativeSequence != other.m_nativeSequence
         || m_nativeModifiers != other.m_nativeModifiers;
}

// -------------------------------------------------------------------------------------------------
void NativeKeySequence::clear()
{
  m_keySequence = QKeySequence{};
  m_nativeModifiers.clear();
  m_nativeSequence.clear();
}

// -------------------------------------------------------------------------------------------------
int NativeKeySequence::count() const
{
  return qMax(m_keySequence.count(), static_cast<int>(m_nativeModifiers.size()));
}

// -------------------------------------------------------------------------------------------------
QString NativeKeySequence::toString() const
{
  QString seqString;
  const size_t size = count();
  for (size_t i = 0; i < size; ++i)
  {
    if (i > 0) { seqString += QLatin1String(", "); }

    #if (QT_VERSION < QT_VERSION_CHECK(6, 0, 0))
    const auto key = m_keySequence[i];
    #else
    const auto key = m_keySequence[i].key();
    #endif

    seqString += toString(key,
                          (i < m_nativeModifiers.size()) ? m_nativeModifiers[i]
                                                         : to_integral(Modifier::NoModifier));
  }
  return seqString;
}

// -------------------------------------------------------------------------------------------------
QString NativeKeySequence::toString(int qtKey, uint16_t nativeModifiers)
{
  QString keyStr;

  if (qtKey == 0) // Special case for manually created Key Sequences
  {
    if ((nativeModifiers & Modifier::LeftMeta) == Modifier::LeftMeta
        || (nativeModifiers & Modifier::RightMeta) == Modifier::RightMeta) {
      addKeyToString(keyStr, QLatin1String("Meta"));
    }

    if ((nativeModifiers & Modifier::LeftCtrl) == Modifier::LeftCtrl
        || (nativeModifiers & Modifier::RightCtrl) == Modifier::RightCtrl) {
      addKeyToString(keyStr, QLatin1String("Ctrl"));
    }

    if ((nativeModifiers & Modifier::LeftAlt) == Modifier::LeftAlt) {
      addKeyToString(keyStr, QLatin1String("Alt"));
    }

    if ((nativeModifiers & Modifier::RightAlt) == Modifier::RightAlt) {
      addKeyToString(keyStr, QLatin1String("AltGr"));
    }

    if ((nativeModifiers & Modifier::LeftShift) == Modifier::LeftShift
        || (nativeModifiers & Modifier::RightShift) == Modifier::RightShift) {
      addKeyToString(keyStr, QLatin1String("Shift"));
    }

    return keyStr;
  }

  if((qtKey & Qt::MetaModifier) == Qt::MetaModifier) {
    addKeyToString(keyStr, QLatin1String("Meta"));
  }

  if((qtKey & Qt::ControlModifier) == Qt::ControlModifier) {
    addKeyToString(keyStr, QLatin1String("Ctrl"));
  }

  if((qtKey & Qt::AltModifier) == Qt::AltModifier) {
    addKeyToString(keyStr, QLatin1String("Alt"));
  }

  if((qtKey & Qt::GroupSwitchModifier) == Qt::GroupSwitchModifier) {
    addKeyToString(keyStr, QLatin1String("AltGr"));
  }

  if((qtKey & Qt::ShiftModifier) == Qt::ShiftModifier) {
    addKeyToString(keyStr, QLatin1String("Shift"));
  }

  qtKey &= ~(Qt::ShiftModifier | Qt::ControlModifier | Qt::AltModifier
             | Qt::MetaModifier | Qt::KeypadModifier | Qt::GroupSwitchModifier);
  addKeyToString(keyStr, QKeySequence(qtKey).toString());

  return keyStr;
}

// -------------------------------------------------------------------------------------------------
QString NativeKeySequence::toString(const std::vector<int>& qtKeys,
                                    const std::vector<uint16_t>& nativeModifiers)
{
  QString seqString;
  const auto size = qtKeys.size();
  for (size_t i = 0; i < size; ++i)
  {
    if (i > 0) { seqString += QLatin1String(", "); }
    seqString += toString(qtKeys[i],
                          (i < nativeModifiers.size()) ? nativeModifiers[i]
                                                       : to_integral(Modifier::NoModifier));
  }
  return seqString;
}

// -------------------------------------------------------------------------------------------------
void NativeKeySequence::swap(NativeKeySequence& other)
{
  m_keySequence.swap(other.m_keySequence);
  m_nativeSequence.swap(other.m_nativeSequence);
  m_nativeModifiers.swap(other.m_nativeModifiers);
}

// -------------------------------------------------------------------------------------------------
const NativeKeySequence& NativeKeySequence::predefined::altTab()
{
  static const NativeKeySequence ks = [](){
    NativeKeySequence ks;
    ks.m_keySequence = QKeySequence::fromString("Alt+Tab");
    ks.m_nativeModifiers.push_back(NativeKeySequence::LeftAlt);
    KeyEvent pressed; KeyEvent released;
    pressed.emplace_back(EV_KEY, KEY_LEFTALT, 1);
    released.emplace_back(EV_KEY, KEY_LEFTALT, 0);
    pressed.emplace_back(EV_KEY, KEY_TAB, 1);
    released.emplace_back(EV_KEY, KEY_TAB, 0);
    pressed.emplace_back(EV_SYN, SYN_REPORT, 0);
    released.emplace_back(EV_SYN, SYN_REPORT, 0);
    ks.m_nativeSequence.emplace_back(std::move(pressed));
    ks.m_nativeSequence.emplace_back(std::move(released));
    return ks;
  }();
  return ks;
}

// -------------------------------------------------------------------------------------------------
const NativeKeySequence& NativeKeySequence::predefined::altF4()
{
  static const NativeKeySequence ks = [](){
    NativeKeySequence ks;
    ks.m_keySequence = QKeySequence::fromString("Alt+F4");
    ks.m_nativeModifiers.push_back(NativeKeySequence::LeftAlt);
    KeyEvent pressed; KeyEvent released;
    pressed.emplace_back(EV_KEY, KEY_LEFTALT, 1);
    released.emplace_back(EV_KEY, KEY_LEFTALT, 0);
    pressed.emplace_back(EV_KEY, KEY_F4, 1);
    released.emplace_back(EV_KEY, KEY_F4, 0);
    pressed.emplace_back(EV_SYN, SYN_REPORT, 0);
    released.emplace_back(EV_SYN, SYN_REPORT, 0);
    ks.m_nativeSequence.emplace_back(std::move(pressed));
    ks.m_nativeSequence.emplace_back(std::move(released));
    return ks;
  }();
  return ks;
}

// -------------------------------------------------------------------------------------------------
const NativeKeySequence& NativeKeySequence::predefined::meta()
{
  static const NativeKeySequence ks = [](){
    NativeKeySequence ks;
    ks.m_nativeModifiers.push_back(NativeKeySequence::LeftMeta);
    KeyEvent pressed; KeyEvent released;
    pressed.emplace_back(EV_KEY, KEY_LEFTMETA, 1);
    released.emplace_back(EV_KEY, KEY_LEFTMETA, 0);
    pressed.emplace_back(EV_SYN, SYN_REPORT, 0);
    released.emplace_back(EV_SYN, SYN_REPORT, 0);
    ks.m_nativeSequence.emplace_back(std::move(pressed));
    ks.m_nativeSequence.emplace_back(std::move(released));
    return ks;
  }();
  return ks;
}


// -------------------------------------------------------------------------------------------------
const char* toString(Action::Type at, bool withClass)
{
  using Type = Action::Type;
  switch (at) {
    ENUM_CASE_STRINGIFY3(Type, KeySequence, withClass);
    ENUM_CASE_STRINGIFY3(Type, CyclePresets, withClass);
    ENUM_CASE_STRINGIFY3(Type, ToggleSpotlight, withClass);
    ENUM_CASE_STRINGIFY3(Type, ScrollHorizontal, withClass);
    ENUM_CASE_STRINGIFY3(Type, ScrollVertical, withClass);
    ENUM_CASE_STRINGIFY3(Type, VolumeControl, withClass);
  }
  return withClass ? "Type::(unknown)" : "(unkown)";
}

// -------------------------------------------------------------------------------------------------
// -------------------------------------------------------------------------------------------------
struct InputMapper::Impl
{
  Impl(InputMapper* parent, std::shared_ptr<VirtualDevice> vdev);

  void sequenceTimeout();
  void resetState();
  void record(const struct input_event input_events[], size_t num);
  void emitNativeKeySequence(const NativeKeySequence& ks);
  void execAction(const std::shared_ptr<Action>& action, DeviceKeyMap::Result r);

  InputMapper* m_parent = nullptr;
  std::shared_ptr<VirtualDevice> m_vdev; // can be a nullptr if application is started without uinput
  QTimer* m_seqTimer = nullptr;
  DeviceKeyMap m_keymap;

  std::pair<DeviceKeyMap::Result, const KeyEventItem*> m_lastState;
  std::vector<input_event> m_events;
  InputMapConfig m_config;
  bool m_recordingMode = false;

  ReservedInputs m_reservedInputs;
};

// -------------------------------------------------------------------------------------------------
InputMapper::Impl::Impl(InputMapper* parent, std::shared_ptr<VirtualDevice> vdev)
  : m_parent(parent)
  , m_vdev(std::move(vdev))
  , m_seqTimer(new QTimer(parent))
{
  constexpr int defaultSequenceIntervalMs = 250;
  m_seqTimer->setSingleShot(true);
  m_seqTimer->setInterval(defaultSequenceIntervalMs);
  connect(m_seqTimer, &QTimer::timeout, parent, [this](){ sequenceTimeout(); });
}

// -------------------------------------------------------------------------------------------------
void InputMapper::Impl::execAction(const std::shared_ptr<Action>& action, DeviceKeyMap::Result r)
{
  if (!action || action->empty()) { return; }

  logDebug(input) << "Input map execAction, type =" << toString(action->type())
                  << ", partial_hit =" << (r == DeviceKeyMap::Result::PartialHit);

  if (action->type() == Action::Type::KeySequence)
  {
    const auto keySequenceAction = static_cast<KeySequenceAction*>(action.get());
    logDebug(input) << "Emitting Key Sequence:" << keySequenceAction->keySequence.toString();
    emitNativeKeySequence(keySequenceAction->keySequence);
  }
  else
  {
    emit m_parent->actionMapped(action);
  }
}

// -------------------------------------------------------------------------------------------------
void InputMapper::Impl::sequenceTimeout()
{
  if(m_recordingMode)
  {
    emit m_parent->recordingFinished(false);
    return;
  }

  if (m_lastState.first == DeviceKeyMap::Result::Valid) {
    // Last input event was part of a valid key sequence, but timeout hit
    // So we emit our stored event so far to the virtual device
    if (m_vdev && !m_events.empty())
    {
      m_vdev->emitEvents(m_events);
    }
    resetState();
  }
  else if (m_lastState.first == DeviceKeyMap::Result::PartialHit) {
    // Last input could have triggered an action, but we needed to wait for the timeout, since
    // other sequences could have been possible.
    if (m_lastState.second)
    {
      execAction(m_lastState.second->action, DeviceKeyMap::Result::PartialHit);
    }
    else if (m_vdev && !m_events.empty())
    {
      m_vdev->emitEvents(m_events);
      m_events.resize(0);
    }
    resetState();
  }
}

// -------------------------------------------------------------------------------------------------
void InputMapper::Impl::resetState()
{
  m_keymap.resetState();
  m_events.resize(0);
}

// -------------------------------------------------------------------------------------------------
void InputMapper::Impl::emitNativeKeySequence(const NativeKeySequence& ks)
{
  if (!m_vdev) { return; }

  std::vector<input_event> events;
  events.reserve(5); // up to 3 modifier keys + 1 key + 1 syn event
  for (const auto& ke : ks.nativeSequence())
  {
    for (const auto& ie : ke) {
      events.emplace_back(input_event{{}, ie.type, ie.code, ie.value});
    }
    m_vdev->emitEvents(events);
    events.resize(0);
  }
}

// -------------------------------------------------------------------------------------------------
void InputMapper::Impl::record(const struct input_event input_events[], size_t num)
{
  const auto ev = KeyEvent(input_events, input_events + num);

  if (!m_seqTimer->isActive()) {
    emit m_parent->recordingStarted();
  }
  m_seqTimer->start();
  emit m_parent->keyEventRecorded(ev);
}

// -------------------------------------------------------------------------------------------------
// -------------------------------------------------------------------------------------------------
InputMapper::InputMapper(std::shared_ptr<VirtualDevice> virtualDevice, QObject* parent)
  : QObject(parent)
  , impl(std::make_unique<Impl>(this, std::move(virtualDevice)))
{}

// -------------------------------------------------------------------------------------------------
InputMapper::~InputMapper() = default;

// -------------------------------------------------------------------------------------------------
std::shared_ptr<VirtualDevice> InputMapper::virtualDevice() const
{
  return impl->m_vdev;
}

// -------------------------------------------------------------------------------------------------
bool InputMapper::hasVirtualDevice() const
{
  return !!(impl->m_vdev);
}

// -------------------------------------------------------------------------------------------------
bool InputMapper::recordingMode() const
{
  return impl->m_recordingMode;
}

// -------------------------------------------------------------------------------------------------
void InputMapper::setRecordingMode(bool recording)
{
  if (impl->m_recordingMode == recording) { return; }

  const auto wasRecording = (impl->m_recordingMode && impl->m_seqTimer->isActive());
  impl->m_recordingMode = recording;

  if (wasRecording) { emit recordingFinished(true); }
  impl->m_seqTimer->stop();
  resetState();
  emit recordingModeChanged(impl->m_recordingMode);
}

// -------------------------------------------------------------------------------------------------
int InputMapper::keyEventInterval() const
{
  return impl->m_seqTimer->interval();
}

// -------------------------------------------------------------------------------------------------
void InputMapper::setKeyEventInterval(int interval)
{
  impl->m_seqTimer->setInterval(std::min(Settings::inputSequenceIntervalRange().max,
                                std::max(Settings::inputSequenceIntervalRange().min, interval)));
}

// -------------------------------------------------------------------------------------------------
void InputMapper::addEvents(const input_event* input_events, size_t num)
{
  if (num == 0 || (!impl->m_vdev)) { return; }

  // If no key mapping is configured ...
  if (!impl->m_recordingMode && !impl->m_keymap.hasConfig()) {
    // ... forward events to virtual device if it exists...
    impl->m_vdev->emitEvents(input_events, num);
    return;
  }

  if (input_events[num-1].type != EV_SYN) {
    logWarning(input) << tr("Input mapper expects events separated by SYN event.");
    return;
  }

  if (num == 1) {
    logWarning(input) << tr("Ignoring single SYN event received.");
    return;
  }

  // For mouse button press ignore MSC_SCAN events
  if (num == 3
      && input_events[1].type == EV_KEY
      && (input_events[1].code == BTN_LEFT
          || input_events[1].code == BTN_RIGHT
          || input_events[1].code == BTN_MIDDLE)
      && input_events[0].type == EV_MSC && input_events[0].code == MSC_SCAN)
  {
    ++input_events; --num;
  }

  if (impl->m_recordingMode)
  {
    logDebug(input) << "Recorded device event:" << KeyEvent{input_events, input_events + num - 1};
    impl->record(input_events, num-1); // exclude closing syn event for recording
    return;
  }

  const auto res = impl->m_keymap.feed(input_events, num-1); // exclude syn event for keymap feed

  // Add current events to the buffered events
  impl->m_events.reserve(impl->m_events.size() + num);
  std::copy(input_events, input_events + num, std::back_inserter(impl->m_events));

  if (res == DeviceKeyMap::Result::Miss)
  { // key sequence miss, send all buffered events so far
    impl->m_seqTimer->stop();
    impl->m_vdev->emitEvents(impl->m_events);

    impl->resetState();
  }
  else if (res == DeviceKeyMap::Result::Hit)
  { // Found a valid key sequence
    impl->m_seqTimer->stop();
    if (const auto pos = impl->m_keymap.state()) {
      impl->execAction(pos->action, res);
    }
    else {
      impl->m_vdev->emitEvents(impl->m_events);
    }

    impl->resetState();
  }
  else if (res == DeviceKeyMap::Result::Valid || res == DeviceKeyMap::Result::PartialHit)
  { // KeyEvent is either a part of valid key sequence or Partial Hit.
    // In both case, save the current state and start timer
    impl->m_lastState = std::make_pair(res, impl->m_keymap.state());
    impl->m_seqTimer->start();
  }
}

// -------------------------------------------------------------------------------------------------
void InputMapper::addEvents(KeyEvent key_event)
{
  if (key_event.empty()) { addEvents({}, 0); }

  static const auto to_input_event = [](const DeviceInputEvent& de){
    struct input_event ie = {{}, de.type, de.code, de.value};
    return ie;
  };

  // If key_event do not have SYN event at end, add SYN event before sending to inputMapper.
  if (key_event.back().type != EV_SYN) { key_event.emplace_back(EV_SYN, SYN_REPORT, 0); }

  std::vector<struct input_event> events;
  events.reserve(key_event.size());
  for (const auto& dev_input_event : key_event) {
    events.emplace_back(to_input_event(dev_input_event));
  }
  addEvents(events.data(), events.size());
}

// -------------------------------------------------------------------------------------------------
void InputMapper::resetState()
{
  impl->resetState();
}

// -------------------------------------------------------------------------------------------------
void InputMapper::setConfiguration(const InputMapConfig& config)
{
  if (config == impl->m_config) { return; }

  impl->m_config = config;
  impl->resetState();
  impl->m_keymap.reconfigure(impl->m_config);
  emit configurationChanged();
}

// -------------------------------------------------------------------------------------------------
void InputMapper::setConfiguration(InputMapConfig&& config)
{
  if (config == impl->m_config) { return; }

  impl->m_config.swap(config);
  impl->resetState();
  impl->m_keymap.reconfigure(impl->m_config);
  emit configurationChanged();
}

// -------------------------------------------------------------------------------------------------
const InputMapConfig& InputMapper::configuration() const
{
  return impl->m_config;
}

// -------------------------------------------------------------------------------------------------
InputMapper::ReservedInputs& InputMapper::specialInputs()
{
  return impl->m_reservedInputs;
}

// -------------------------------------------------------------------------------------------------
namespace SpecialKeys
{
// -------------------------------------------------------------------------------------------------
const std::map<Key, SpecialKeyEventSeqInfo>&  keyEventSequenceMap()
{
  static const std::map<Key, SpecialKeyEventSeqInfo> keyMap {
    {Key::BackHoldMove, {InputMapper::tr("Back Hold Move"),
      makeSpecialKeyEventSequence(to_integral(Key::BackHoldMove))}},
    {Key::NextHoldMove, {InputMapper::tr("Next Hold Move"),
      makeSpecialKeyEventSequence(to_integral(Key::NextHoldMove))}},
  };
  return keyMap;
}

// -------------------------------------------------------------------------------------------------
const SpecialKeyEventSeqInfo& eventSequenceInfo(SpecialKeys::Key key)
{
  const auto it = keyEventSequenceMap().find(key);
  if (it != keyEventSequenceMap().cend()) {
    return it->second;
  }

  static const SpecialKeyEventSeqInfo notFound;
  return notFound;
}

} // end namespace SpecialKeys
