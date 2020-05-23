// This file is part of Projecteur - https://github.com/jahnf/projecteur - See LICENSE.md and README.md
#include "deviceinput.h"

#include "logging.h"
#include "virtualdevice.h"

#include <map>
#include <set>

#include <QTimer>

LOGGING_CATEGORY(input, "input")

// -------------------------------------------------------------------------------------------------
QDebug operator<<(QDebug debug, const DeviceInputEvent &d)
{
  QDebugStateSaver saver(debug);
  debug.nospace() << '{' << d.type << ", " << d.code << ", " << d.value << '}';
  return debug;
}

QDebug operator<<(QDebug debug, const KeyEvent &ke)
{
  QDebugStateSaver saver(debug);
  debug.nospace() << "[";
  for (const auto& e : ke)
    debug.nospace() << e << ',';
  debug.nospace() << "]";
  return debug;
}

// -------------------------------------------------------------------------------------------------
namespace  {
  struct Next;
  // Map of Key event and the next possible key events.
  using SynKeyEventMap = std::map<const KeyEvent, std::unique_ptr<Next>>;
  using RefPair = SynKeyEventMap::value_type;
  // Set of references to the next possible key event.
  using RefSet = std::set<const RefPair*>;

  struct Next {
    int action = 0;  // TODO dummy - this is still a prototype
    RefSet next_events;
  };

  // Helper function
  size_t maxSequenceLength(const std::vector<KeyEventSequence>& kesv) {
    size_t maxLen = 0;
    for (const auto& kes: kesv)
      if (kes.size() > maxLen) maxLen = kes.size();
    return maxLen;
  }

  // Internal data structure for keeping track of key events and checking if a configured
  // key event sequence was pressed. Needs to be completely reconstructed/reconfigured
  // if the configuration changes.
  struct DeviceKeyMap
  {
    DeviceKeyMap(const std::vector<KeyEventSequence>& kesv = {}) { reconfigure(kesv); }

    enum Result : uint8_t {
      Miss, Valid, Hit, AmbigiouslyHit
    };

    Result feed(struct input_event input_events[], size_t num);

    auto state() const { return m_pos; }
    void resetState();
    void reconfigure(const std::vector<KeyEventSequence>& kesv = {});
    bool hasConfig() const { return m_keymaps.size(); }

  private:
    const RefPair* m_pos = nullptr;
    std::vector<SynKeyEventMap> m_keymaps;
  };
}

// -------------------------------------------------------------------------------------------------
DeviceKeyMap::Result DeviceKeyMap::feed(struct input_event input_events[], size_t num)
{
  if (!hasConfig()) return Result::Miss;

  if (m_pos == nullptr)
  {
    const auto find_it = m_keymaps[0].find(KeyEvent(input_events, input_events + num));
    if (find_it == m_keymaps[0].cend()) return Result::Miss;
    m_pos = &(*find_it);
  }
  else
  {
    if (!m_pos->second) return Result::Miss;

    const auto ke = KeyEvent(KeyEvent(input_events, input_events + num));
    const auto& set = m_pos->second->next_events;
    const auto find_it = std::find_if(set.cbegin(), set.cend(), [&ke](RefPair const* next_ptr) {
      return ke == next_ptr->first;
    });

    if (find_it == set.cend()) return Result::Miss;

    m_pos = (*find_it);
  }

  // Last KeyEvent in possible sequence...
  if (!m_pos->second || m_pos->second->next_events.empty()) {
    return Result::Hit;
  }

  // KeyEvent in Sequence has action attached, but there are other possible sequences...
  if (m_pos->second->action != 0) {
    return Result::AmbigiouslyHit;
  }

  return Result::Valid;
}

// -------------------------------------------------------------------------------------------------
void DeviceKeyMap::resetState()
{
  m_pos = nullptr;
}

// -------------------------------------------------------------------------------------------------
void DeviceKeyMap::reconfigure(const std::vector<KeyEventSequence>& kesv)
{
  m_keymaps.resize(maxSequenceLength(kesv));

  // -- clear maps + position
  for (auto& synKeyEventMap : m_keymaps) { synKeyEventMap.clear(); }
  m_pos = nullptr;

  // -- fill maps
  for (const auto& kes: kesv) {
    for (size_t i = 0; i < kes.size(); ++i) {
      m_keymaps[i].emplace(kes[i], nullptr);
    }
  }

  // -- fill references
  for (const auto& kes: kesv) {
    for (size_t i = 0; i < kes.size(); ++i)
    {
      const auto r = m_keymaps[i].equal_range(kes[i]);
      if (r.first == r.second) continue;
      auto& refobj = r.first->second;
      if (!refobj) {
        refobj = std::make_unique<Next>();
      }

      if (i == kes.size() - 1) // last keyevent in seq
      {
        // Set (placeholder/fake) action for now in this prototype.
        refobj->action = 1;
      }
      else if (i+1 < m_keymaps.size()) // if not last keyevent in seq
      {
        const auto r = m_keymaps[i+1].equal_range(kes[i+1]);
        if (r.first == r.second) continue;
        refobj->next_events.emplace(&(*r.first));
      }
    }
  }
}

// -------------------------------------------------------------------------------------------------
struct InputMapper::Impl {
  Impl(std::shared_ptr<VirtualDevice> vdev) : m_vdev(std::move(vdev)) {}

  std::shared_ptr<VirtualDevice> m_vdev; // can be a nullptr if application is started without uinput
  DeviceKeyMap m_keymap;

  std::pair<DeviceKeyMap::Result, const RefPair*> m_lastState;
  std::vector<input_event> m_events;
  bool m_recordingMode = false;
};

// -------------------------------------------------------------------------------------------------
InputMapper::InputMapper(std::shared_ptr<VirtualDevice> virtualDevice, QObject* parent)
  : QObject(parent)
  , impl(std::make_unique<Impl>(Impl(virtualDevice)))
  , m_seqTimer(new QTimer(this))
{
//  std::vector<KeyEventSequence> keysequences;
//  impl->m_keymap.reconfigure(keysequences);

  m_seqTimer->setSingleShot(true);
  m_seqTimer->setInterval(200); // TODO make interval configurable

  connect(m_seqTimer, &QTimer::timeout, this, &InputMapper::sequenceTimeout);
}

// -------------------------------------------------------------------------------------------------
InputMapper::~InputMapper()
{
}

// -------------------------------------------------------------------------------------------------
std::shared_ptr<VirtualDevice> InputMapper::virtualDevice() const
{
  return impl->m_vdev;
}

// -------------------------------------------------------------------------------------------------
bool InputMapper::recordingMode() const
{
  return impl->m_recordingMode;
}

// -------------------------------------------------------------------------------------------------
void InputMapper::setRecordingMode(bool recording)
{
  if (impl->m_recordingMode == recording)
    return;

  impl->m_recordingMode = recording;
  emit recordingModeChanged(impl->m_recordingMode);
}

// -------------------------------------------------------------------------------------------------
void InputMapper::sequenceTimeout()
{
  if (impl->m_lastState.first == DeviceKeyMap::Result::Valid) {
    // last input event was part of a valid key sequence, but timeout hit...
    if (impl->m_vdev && impl->m_events.size())
    {
      impl->m_vdev->emitEvents(impl->m_events);
      impl->m_events.resize(0);
    }
    impl->m_keymap.resetState();
  }
  else if (impl->m_lastState.first == DeviceKeyMap::Result::AmbigiouslyHit) {
    // Last input could have triggered an action, but we needed to wait for the timeout, since
    // other sequences could have been possible.
    // TODO trigger actions(s) / inject mapped key(s)
    resetState();
  }
}

// -------------------------------------------------------------------------------------------------
void InputMapper::addEvents(struct input_event input_events[], size_t num)
{
  if (num == 0) {
    return;
  }

  if (!impl->m_keymap.hasConfig()) {
    if (impl->m_vdev) {
      impl->m_vdev->emitEvents(input_events, num);
    }
    return;
  }

  if (input_events[num-1].type != EV_SYN) {
    logWarning(input) << tr("Input mapper expects events seperated by SYN event.");
    return;
  } else if (num == 1) {
    logWarning(input) << tr("Ignoring single SYN event received.");
    return;
  }

  const auto res = impl->m_keymap.feed(input_events, num-1); // exclude syn event for keymap feed

  if (res == DeviceKeyMap::Result::Miss)
  { // key sequence miss, send all buffered events so far + current event
    m_seqTimer->stop();
    if (impl->m_vdev)
    {
      if (impl->m_events.size()) {
        impl->m_vdev->emitEvents(impl->m_events);
        impl->m_events.resize(0);
      }
      impl->m_vdev->emitEvents(input_events, num);
    }
    impl->m_keymap.resetState();
  }
  else if (res == DeviceKeyMap::Result::Valid)
  { // KeyEvent is part of valid key sequence.
    impl->m_lastState = std::make_pair(res, impl->m_keymap.state());
    m_seqTimer->start();
    if (impl->m_vdev) {
      impl->m_events.reserve(impl->m_events.size() + num);
      std::copy(input_events, input_events + num, std::back_inserter(impl->m_events));
    }
  }
  else if (res == DeviceKeyMap::Result::Hit)
  { // Found a valid key sequence
    m_seqTimer->stop();
    if (impl->m_vdev)
    {
      if (impl->m_events.size()) {
        impl->m_vdev->emitEvents(impl->m_events);
        impl->m_events.resize(0);
      }
      impl->m_vdev->emitEvents(input_events, num);
    }
    // TODO run action(s) / send mapped key events
    impl->m_keymap.resetState();
  }
  else if (res == DeviceKeyMap::Result::AmbigiouslyHit)
  { // Found a valid key sequence, but are still more valid sequences possible -> start timer
    impl->m_lastState = std::make_pair(res, impl->m_keymap.state());
    m_seqTimer->start();
    if (impl->m_vdev) {
      impl->m_events.reserve(impl->m_events.size() + num);
      std::copy(input_events, input_events + num, std::back_inserter(impl->m_events));
    }
  }
}

// -------------------------------------------------------------------------------------------------
void InputMapper::resetState()
{
  impl->m_keymap.resetState();
  impl->m_events.resize(0);
}
