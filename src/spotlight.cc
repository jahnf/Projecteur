// This file is part of Projecteur - https://github.com/jahnf/projecteur - See LICENSE.md and README.md
#include "spotlight.h"

#include "hidpp.h"
#include "logging.h"
#include "settings.h"
#include "virtualdevice.h"

#include <QSocketNotifier>
#include <QTimer>
#include <QVarLengthArray>
#include <QProcess>

#include <fcntl.h>
#include <sys/inotify.h>
#include <sys/ioctl.h>
#include <unistd.h>

DECLARE_LOGGING_CATEGORY(device)
DECLARE_LOGGING_CATEGORY(hid)
DECLARE_LOGGING_CATEGORY(input)

namespace {
  const auto hexId = logging::hexId;
} // --- end anonymous namespace

// -------------------------------------------------------------------------------------------------
Spotlight::Spotlight(QObject* parent, Options options, Settings* settings)
  : QObject(parent)
  , m_options(std::move(options))
  , m_activeTimer(new QTimer(this))
  , m_connectionTimer(new QTimer(this))
  , m_settings(settings)
{
  m_activeTimer->setSingleShot(true);
  m_activeTimer->setInterval(600);

  connect(m_activeTimer, &QTimer::timeout, this, [this](){
    setSpotActive(false);
  });

  if (m_options.enableUInput) {
    m_virtualDevice = VirtualDevice::create();
  }
  else {
    logInfo(device) << tr("Virtual device initialization was skipped.");
  }

  m_connectionTimer->setSingleShot(true);
  // From detecting a change from inotify, the device needs some time to be ready for open
  // TODO: This interval seems to work, but it is arbitrary - there should be a better way.
  m_connectionTimer->setInterval(800);

  connect(m_connectionTimer, &QTimer::timeout, this, [this]() {
    logDebug(device) << tr("New connection check triggered");
    connectDevices();
  });

  // Try to find already attached device(s) and connect to it.
  connectDevices();
  setupDevEventInotify();
}

// -------------------------------------------------------------------------------------------------
Spotlight::~Spotlight() = default;

// -------------------------------------------------------------------------------------------------
bool Spotlight::anySpotlightDeviceConnected() const
{
  for (const auto& dc : m_deviceConnections) {
    if (dc.second->subDeviceCount()) return true;
  }
  return false;
}

// -------------------------------------------------------------------------------------------------
uint32_t Spotlight::connectedDeviceCount() const
{
  uint32_t count = 0;
  for (const auto& dc : m_deviceConnections) {
    if (dc.second->subDeviceCount()) ++count;
  }
  return count;
}

// -------------------------------------------------------------------------------------------------
void Spotlight::setSpotActive(bool active)
{
  if (m_spotActive == active) return;
  m_spotActive = active;
  if (!m_spotActive) m_activeTimer->stop();
  emit spotActiveChanged(m_spotActive);
}

// -------------------------------------------------------------------------------------------------
std::shared_ptr<DeviceConnection> Spotlight::deviceConnection(const DeviceId& deviceId)
{
  const auto find_it = m_deviceConnections.find(deviceId);
  return (find_it != m_deviceConnections.end()) ? find_it->second : std::shared_ptr<DeviceConnection>();
}

// -------------------------------------------------------------------------------------------------
std::vector<Spotlight::ConnectedDeviceInfo> Spotlight::connectedDevices() const
{
  std::vector<ConnectedDeviceInfo> devices;
  devices.reserve(m_deviceConnections.size());
  for (const auto& dc : m_deviceConnections) {
    devices.emplace_back(ConnectedDeviceInfo{ dc.first, dc.second->deviceName() });
  }
  return devices;
}

// -------------------------------------------------------------------------------------------------
int Spotlight::connectDevices()
{
  const auto scanResult = DeviceScan::getDevices(m_options.additionalDevices);

  for (const auto& dev : scanResult.devices)
  {
    auto& dc = m_deviceConnections[dev.id];
    if (!dc) {
      dc = std::make_shared<DeviceConnection>(dev.id, dev.getName(), m_virtualDevice);
    }

    const bool anyConnectedBefore = anySpotlightDeviceConnected();
    for (const auto& scanSubDevice : dev.subDevices)
    {
      if (!scanSubDevice.deviceReadable)
      {
        logWarn(device) << tr("Sub-device not readable: %1 (%2:%3) %4")
          .arg(dc->deviceName(), hexId(dev.id.vendorId), hexId(dev.id.productId), scanSubDevice.deviceFile);
        continue;
      }
      if (dc->hasSubDevice(scanSubDevice.deviceFile)) continue;

      std::shared_ptr<SubDeviceConnection> subDeviceConnection =
      [&scanSubDevice, &dc, this]() -> std::shared_ptr<SubDeviceConnection>
      { // Input event sub devices
        if (scanSubDevice.type == DeviceScan::SubDevice::Type::Event) {
          auto devCon = SubEventConnection::create(scanSubDevice, *dc);
          if (addInputEventHandler(devCon)) return devCon;
        } // Hidraw sub devices
        else if (scanSubDevice.type == DeviceScan::SubDevice::Type::Hidraw)
        {
          if (dc->hasHidppSupport())
          {
            auto hidppCon = SubHidppConnection::create(scanSubDevice, *dc);
            if (addHidppInputHandler(hidppCon))
            {
              // connect to hidpp sub connection signals
              connect(&*hidppCon, &SubHidppConnection::receivedBatteryInfo,
                      dc.get(), &DeviceConnection::setBatteryInfo);
              auto hidppActivated = [this, dc]() {
                if (std::find(m_activeDeviceIds.cbegin(), m_activeDeviceIds.cend(),
                              dc->deviceId()) == m_activeDeviceIds.cend()) {
                  logInfo(device) << dc->deviceName() << "is now active.";
                  m_activeDeviceIds.emplace_back(dc->deviceId());
                  emit deviceActivated(dc->deviceId(), dc->deviceName());
                }
              };
              auto hidppDeactivated = [this, dc]() {
                auto it = std::find(m_activeDeviceIds.cbegin(), m_activeDeviceIds.cend(), dc->deviceId());
                if (it != m_activeDeviceIds.cend()) {
                  logInfo(device) << dc->deviceName() << "is deactivated.";
                  m_activeDeviceIds.erase(it);
                  emit deviceDeactivated(dc->deviceId(), dc->deviceName());
                }
              };
              connect(&*hidppCon, &SubHidppConnection::activated, dc.get(), hidppActivated);
              connect(&*hidppCon, &SubHidppConnection::deactivated, dc.get(), hidppDeactivated);
              connect(&*hidppCon, &SubHidppConnection::destroyed, dc.get(), hidppDeactivated);

              return hidppCon;
            }
          }
          else {
            return SubHidrawConnection::create(scanSubDevice, *dc);
          }
        }
        return std::shared_ptr<SubDeviceConnection>();
      }();

      if (!subDeviceConnection) continue;

      if (dc->subDeviceCount() == 0) {
        // Load Input mapping settings when first sub-device gets added.
        const auto im = dc->inputMapper().get();

        im->setKeyEventInterval(m_settings->deviceInputSeqInterval(dev.id));
        im->setConfiguration(m_settings->getDeviceInputMapConfig(dev.id));

        connect(im, &InputMapper::configurationChanged, this, [this, id=dev.id, im]() {
          m_settings->setDeviceInputMapConfig(id, im->configuration());
        });

        static QString lastPreset;

        connect(im, &InputMapper::actionMapped, this, [this](std::shared_ptr<Action> action)
        {
          if (!(action->isRepeated()) && m_holdButtonStatus.numEvents() > 0) return;

          static auto sign = [](int i) { return i/abs(i); };
          auto emitNativeKeySequence = [this](const NativeKeySequence& ks)
          {
            if (!m_virtualDevice) return;

            std::vector<input_event> events;
            events.reserve(5); // up to 3 modifier keys + 1 key + 1 syn event
            for (const auto& ke : ks.nativeSequence())
            {
              for (const auto& ie : ke)
                events.emplace_back(input_event{{}, ie.type, ie.code, ie.value});

              m_virtualDevice->emitEvents(events);
              events.resize(0);
            };
          };

          if (action->type() == Action::Type::KeySequence)
          {
            const auto keySequenceAction = static_cast<KeySequenceAction*>(action.get());
            logInfo(input) << "Emitting Key Sequence:" << keySequenceAction->keySequence.toString();
            emitNativeKeySequence(keySequenceAction->keySequence);
          }

          if (action->type() == Action::Type::CyclePresets)
          {
            auto it = std::find(m_settings->presets().cbegin(), m_settings->presets().cend(), lastPreset);
            if ((it == m_settings->presets().cend()) || (++it == m_settings->presets().cend())) {
              it = m_settings->presets().cbegin();
            }

            if (it != m_settings->presets().cend())
            {
              lastPreset = *it;
              m_settings->loadPreset(lastPreset);
            }
          }

          if (action->type() == Action::Type::ToggleSpotlight)
          {
            m_settings->setOverlayDisabled(!m_settings->overlayDisabled());
          }

          if (action->type() == Action::Type::ScrollHorizontal || action->type() == Action::Type::ScrollVertical)
          {
            if (!m_virtualDevice) return;

            int param = 0;
            uint16_t wheelType = (action->type() == Action::Type::ScrollHorizontal) ? REL_HWHEEL : REL_WHEEL;
            if (action->type() == Action::Type::ScrollHorizontal) param = static_cast<ScrollHorizontalAction*>(action.get())->param;
            if (action->type() == Action::Type::ScrollVertical) param = static_cast<ScrollVerticalAction*>(action.get())->param;

            if (param)
              for (int j=0; j<abs(param); j++)
                m_virtualDevice->emitEvents({{{},EV_REL, wheelType, sign(param)}});
          }

          if (action->type() == Action::Type::VolumeControl)
          {
            auto param = static_cast<VolumeControlAction*>(action.get())->param;
            if (param) QProcess::execute("amixer",
                                         QStringList({"set", "Master",
                                                      tr("%1\%%2").arg(abs(param)).arg(sign(param)==1?"+":"-"),
                                                      "-q"}));

          }
        });

        connect(m_settings, &Settings::presetLoaded, this, [](const QString& preset){
          lastPreset = preset;
        });
      }

      dc->addSubDevice(std::move(subDeviceConnection));
      if (dc->subDeviceCount() == 1)
      {
        QTimer::singleShot(0, this,
        [this, id = dev.id, devName = dc->deviceName(), anyConnectedBefore](){
          logInfo(device) << tr("Connected device: %1 (%2:%3)")
                             .arg(devName, hexId(id.vendorId), hexId(id.productId));
          emit deviceConnected(id, devName);
          if (!anyConnectedBefore) emit anySpotlightDeviceConnectedChanged(true);
        });
      }

      logDebug(device) << tr("Connected sub-device: %1 (%2:%3) %4")
                          .arg(dc->deviceName(), hexId(dev.id.vendorId),
                               hexId(dev.id.productId), scanSubDevice.deviceFile);
      emit subDeviceConnected(dev.id, dc->deviceName(), scanSubDevice.deviceFile);
    }

    if (dc->subDeviceCount() == 0) {
      m_deviceConnections.erase(dev.id);
    }
  }
  return m_deviceConnections.size();
}

// -------------------------------------------------------------------------------------------------
void Spotlight::removeDeviceConnection(const QString &devicePath)
{
  for (auto dc_it = m_deviceConnections.begin(); dc_it != m_deviceConnections.end(); )
  {
    if (!dc_it->second) {
      dc_it = m_deviceConnections.erase(dc_it);
      continue;
    }

    auto& dc = dc_it->second;
    if (dc->removeSubDevice(devicePath)) {
      emit subDeviceDisconnected(dc_it->first, dc->deviceName(), devicePath);
    }

    if (dc->subDeviceCount() == 0)
    {
      logInfo(device) << tr("Disconnected device: %1 (%2:%3)")
                         .arg(dc->deviceName(), hexId(dc_it->first.vendorId),
                              hexId(dc_it->first.productId));
      emit deviceDisconnected(dc_it->first, dc->deviceName());
      dc_it = m_deviceConnections.erase(dc_it);
    }
    else {
      ++dc_it;
    }
  }
}

// -------------------------------------------------------------------------------------------------
void Spotlight::onEventDataAvailable(int fd, SubEventConnection& connection)
{
  const bool isNonBlocking = connection.hasFlags(DeviceFlag::NonBlocking);
  while (true)
  {
    auto& buf = connection.inputBuffer();
    auto& ev = buf.current();
    if (::read(fd, &ev, sizeof(ev)) != sizeof(ev))
    {
      if (errno != EAGAIN)
      {
        const bool anyConnectedBefore = anySpotlightDeviceConnected();
        connection.setNotifiersEnabled(false);
        QTimer::singleShot(0, this, [this, devicePath=connection.path(), anyConnectedBefore](){
          removeDeviceConnection(devicePath);
          if (!anySpotlightDeviceConnected() && anyConnectedBefore) {
            emit anySpotlightDeviceConnectedChanged(false);
          }
        });
      }
      break;
    }
    ++buf;

    if (ev.type == EV_SYN)
    {
      // Check for relative events -> set Spotlight active
      const auto &first_ev = buf[0];
      const bool isMouseMoveEvent = first_ev.type == EV_REL
                                    && (first_ev.code == REL_X || first_ev.code == REL_Y);
      if (isMouseMoveEvent)
      { // Skip input mapping for mouse move events completely
        if (!m_activeTimer->isActive()) {
          setSpotActive(true);
        }
        m_activeTimer->start();
        if (m_virtualDevice) m_virtualDevice->emitEvents(buf.data(), buf.pos());
      }
      else
      { // Forward events to input mapper for the device
        connection.inputMapper()->addEvents(buf.data(), buf.pos());
      }
      buf.reset();
    }
    else if (buf.pos() >= buf.size())
    { // No idea if this will ever happen, but log it to make sure we get notified.
      logWarning(device) << tr("Discarded %1 input events without EV_SYN.").arg(buf.size());
      connection.inputMapper()->resetState();
      buf.reset();
    }

    if (!isNonBlocking) break;
  } // end while loop
}

// -------------------------------------------------------------------------------------------------
void Spotlight::onHidppDataAvailable(int fd, SubHidppConnection& connection)
{
  Q_UNUSED(fd);
  Q_UNUSED(connection);
  QByteArray readVal(20, 0);
  if (::read(fd, static_cast<void *>(readVal.data()), readVal.length()) < 0)
  {
    if (errno != EAGAIN)
    {
      const bool anyConnectedBefore = anySpotlightDeviceConnected();
      connection.setNotifiersEnabled(false);
      QTimer::singleShot(0, this, [this, devicePath=connection.path(), anyConnectedBefore](){
        removeDeviceConnection(devicePath);
        if (!anySpotlightDeviceConnected() && anyConnectedBefore) {
          emit anySpotlightDeviceConnectedChanged(false);
        }
      });
    }
    return;
  }

  // Only process HID++ packets (hence, the packets starting with 0x10 or 0x11)
  if (!(readVal.at(0) == HIDPP::Bytes::SHORT_MSG || readVal.at(0) == HIDPP::Bytes::LONG_MSG)) {
    return;
  }

  logDebug(hid) << "Received" << readVal.toHex() << "from" << connection.path();

  if (readVal.at(0) == HIDPP::Bytes::SHORT_MSG)    // Logitech HIDPP SHORT message: 7 byte long
  {
    // wireless notification from USB dongle
    if (readVal.at(2) == HIDPP::Bytes::SHORT_WIRELESS_NOTIFICATION_CODE) {
      auto connection_status = readVal.at(4) & (1<<6);  // should be zero for working connection between
                                                        // USB dongle and Spotlight device.
      if (connection_status) {    // connection between USB dongle and spotlight device broke
        connection.setHIDppProtocol(-1);
      } else {                         // Logitech spotlight presenter unit got online and USB dongle acknowledged it.
        if (!connection.isOnline()) connection.initialize();
      }
    }
  }

  if (readVal.at(0) == HIDPP::Bytes::LONG_MSG)    // Logitech HIDPP LONG message: 20 byte long
  {
    // response to ping
    auto rootIndex = connection.getFeatureSet()->getFeatureIndex(FeatureCode::Root);
    if (readVal.at(2) == rootIndex) {
      if (readVal.at(3) == connection.getFeatureSet()->getRandomFunctionCode(0x10) && readVal.at(6) == 0x5d) {
        auto protocolVer = static_cast<uint8_t>(readVal.at(4)) + static_cast<uint8_t>(readVal.at(5))/10.0;
        connection.setHIDppProtocol(protocolVer);
      }
    }

    // Wireless Notification from the Spotlight device
    auto wnIndex = connection.getFeatureSet()->getFeatureIndex(FeatureCode::WirelessDeviceStatus);
    if (wnIndex && readVal.at(2) == wnIndex) {    // Logitech spotlight presenter unit got online.
      if (!connection.isOnline()) connection.initialize();
    }

    // Battery packet processing: Device responded to BatteryStatus (0x1000) packet
    auto batteryIndex = connection.getFeatureSet()->getFeatureIndex(FeatureCode::BatteryStatus);
    if (batteryIndex && readVal.at(2) == batteryIndex &&
            readVal.at(3) == connection.getFeatureSet()->getRandomFunctionCode(0x00)) {  // Battery information packet
      QByteArray batteryData(readVal.mid(4, 3));
      emit connection.receivedBatteryInfo(batteryData);
    }

    // Process reprogrammed keys : Next Hold and Back Hold
    auto rcIndex = connection.getFeatureSet()->getFeatureIndex(FeatureCode::ReprogramControlsV4);
    if (rcIndex && readVal.at(2) == rcIndex)     // Button (for which hold events are on) related message.
    {
      auto eventCode = static_cast<uint8_t>(readVal.at(3));
      auto buttonCode = static_cast<uint8_t>(readVal.at(5));
      if (eventCode == 0x00) {  // hold start/stop events
        switch (buttonCode) {
          case 0xda:
            logDebug(hid) << "Next Hold Event ";
            m_holdButtonStatus.setButton(HoldButtonStatus::HoldButtonType::Next);
            break;
          case 0xdc:
            logDebug(hid) << "Back Hold Event ";
            m_holdButtonStatus.setButton(HoldButtonStatus::HoldButtonType::Back);
            break;
          case 0x00:
            // hold event over.
            logDebug(hid) << "Hold Event over.";
            m_holdButtonStatus.reset();
        }
      }
      else if (eventCode == 0x10) {   // mouse move event
        // Mouse data is sent as 4 byte information starting at 4th index and ending at 7th.
        // out of these 5th byte and 7th byte are x and y relative change, respectively.
        // the forth byte show horizonal scroll towards right if rel value is -1 otherwise left scroll (0)
        // the sixth byte show vertical scroll towards up if rel value is -1 otherwise down scroll (0)
        auto byteToRel = [](int i){return ( (i<128) ? i : 256-i);};   // convert the byte to relative motion in x or y
        int x = byteToRel(readVal.at(5));
        int y = byteToRel(readVal.at(7));
        auto action = connection.inputMapper()->getAction(m_holdButtonStatus.keyEventSeq());

        if (action && !action->empty())
        {
          if (action->type() == Action::Type::ScrollHorizontal)
          {
            const auto scrollHAction = static_cast<ScrollHorizontalAction*>(action.get());
            scrollHAction->param = -(abs(x) > 60? 60 : x)/20; // reduce the values from Spotlight device
          }
          if (action->type() == Action::Type::ScrollVertical)
          {
            const auto scrollVAction = static_cast<ScrollVerticalAction*>(action.get());
            scrollVAction->param = (abs(y) > 60? 60 : y)/20; // reduce the values from Spotlight device
          }
          if(action->type() == Action::Type::VolumeControl)
          {
            const auto volumeControlAction = static_cast<VolumeControlAction*>(action.get());
            volumeControlAction->param = -y/20; // reduce the values from Spotlight device
          }

          // feed the keystroke to InputMapper and let it trigger the associated action
          for (auto key_event: m_holdButtonStatus.keyEventSeq()) connection.inputMapper()->addEvents(key_event);
        }
        m_holdButtonStatus.addEvent();
      }
    }

    // Vibration response check
    const uint8_t pcIndex = connection.getFeatureSet()->getFeatureIndex(FeatureCode::PresenterControl);
    if (pcIndex && readVal.at(2) == pcIndex && readVal.at(3) == connection.getFeatureSet()->getRandomFunctionCode(0x10)) {
      logDebug(hid) << "Device acknowledged a vibration event.";
    }
  }
}

// -------------------------------------------------------------------------------------------------
bool Spotlight::addInputEventHandler(std::shared_ptr<SubEventConnection> connection)
{
  if (!connection || connection->type() != ConnectionType::Event || !connection->isConnected()) {
    return false;
  }

  QSocketNotifier* const readNotifier = connection->socketReadNotifier();
  connect(readNotifier, &QSocketNotifier::activated, this,
  [this, connection=std::move(connection)](int fd) {
    onEventDataAvailable(fd, *connection.get());
  });

  return true;
}

// -------------------------------------------------------------------------------------------------
bool Spotlight::addHidppInputHandler(std::shared_ptr<SubHidppConnection> connection)
{
  if (!connection || connection->type() != ConnectionType::Hidraw
      || !connection->isConnected() || !connection->hasFlags(DeviceFlag::Hidpp))
  {
    return false;
  }

  QSocketNotifier* const readNotifier = connection->socketReadNotifier();
  connect(readNotifier, &QSocketNotifier::activated, this,
  [this, connection=std::move(connection)](int fd) {
    onHidppDataAvailable(fd, *connection.get());
  });

  return true;
}

// -------------------------------------------------------------------------------------------------
bool Spotlight::setupDevEventInotify()
{
  int fd = -1;
#if defined(IN_CLOEXEC)
  fd = inotify_init1(IN_CLOEXEC);
#endif
  if (fd == -1)
  {
    fd = inotify_init();
    if (fd == -1) {
      logError(device) << tr("inotify_init() failed. Detection of new attached devices will not work.");
      return false;
    }
  }
  fcntl(fd, F_SETFD, FD_CLOEXEC);
  const int wd = inotify_add_watch(fd, "/dev/input", IN_CREATE | IN_DELETE);

  if (wd < 0) {
    logError(device) << tr("inotify_add_watch for /dev/input returned with failure.");
    return false;
  }

  const auto notifier = new QSocketNotifier(fd, QSocketNotifier::Read, this);
  connect(notifier, &QSocketNotifier::activated, this, [this](int fd)
  {
    int bytesAvaibable = 0;
    if (ioctl(fd, FIONREAD, &bytesAvaibable) < 0 || bytesAvaibable <= 0) {
      return; // Error or no bytes available
    }
    QVarLengthArray<char, 2048> buffer(bytesAvaibable);
    const auto bytesRead = read(fd, buffer.data(), static_cast<size_t>(bytesAvaibable));
    const char* at = buffer.data();
    const char* const end = at + bytesRead;
    while (at < end)
    {
      const auto event = reinterpret_cast<const inotify_event*>(at);

      if ((event->mask & (IN_CREATE)) && QString(event->name).startsWith("event"))
      {
        // Trigger new device scan and connect if a new event device was created.
        m_connectionTimer->start();
      }

      at += sizeof(inotify_event) + event->len;
    }
  });

  connect(notifier, &QSocketNotifier::destroyed, [notifier]() {
    ::close(static_cast<int>(notifier->socket()));
  });
  return true;
}
