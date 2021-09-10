// This file is part of Projecteur - https://github.com/jahnf/projecteur
// - See LICENSE.md and README.md

#include "spotlight.h"

#include "device.h"
#include "device-hidpp.h"
#include "logging.h"
#include "settings.h"
#include "virtualdevice.h"

#include <QSocketNotifier>
#include <QTimer>
#include <QVarLengthArray>

#include <fcntl.h>
#include <sys/inotify.h>
#include <sys/ioctl.h>
#include <unistd.h>

DECLARE_LOGGING_CATEGORY(device)
DECLARE_LOGGING_CATEGORY(hid)
DECLARE_LOGGING_CATEGORY(input)

namespace {
  const auto hexId = logging::hexId;

  // See details on workaround in onEventDataAvailable
  bool workaroundLogitechFirstMoveEvent = true;
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
    workaroundLogitechFirstMoveEvent = true;
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
            if (auto hidppCon = SubHidppConnection::create(scanSubDevice, *dc))
            {
              // Remove device on socketReadError
              QPointer<SubHidppConnection> connPtr(hidppCon.get());
              connect(&*hidppCon, &SubHidppConnection::socketReadError, this, [this, connPtr](){
                if (!connPtr) return;
                const bool anyConnectedBefore = anySpotlightDeviceConnected();
                connPtr->disconnect();
                QTimer::singleShot(0, this, [this, devicePath=connPtr->path(), anyConnectedBefore](){
                  removeDeviceConnection(devicePath);
                  if (!anySpotlightDeviceConnected() && anyConnectedBefore) {
                    emit anySpotlightDeviceConnectedChanged(false);
                  }
                });
              });

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
          else if (action->type() == Action::Type::ToggleSpotlight)
          {
            m_settings->setOverlayDisabled(!m_settings->overlayDisabled());
          }
          else if (action->type() == Action::Type::ScrollHorizontal || action->type() == Action::Type::ScrollVertical)
          {
            if (!m_virtualDevice) return;

            int param = 0;
            if (action->type() == Action::Type::ScrollHorizontal) param = static_cast<ScrollHorizontalAction*>(action.get())->param;
            if (action->type() == Action::Type::ScrollVertical) param = static_cast<ScrollVerticalAction*>(action.get())->param;

            uint16_t wheelCode = (action->type() == Action::Type::ScrollHorizontal) ? REL_HWHEEL : REL_WHEEL;
            const std::vector<input_event> scrollInputEvents = {{{}, EV_REL, wheelCode, param}, {{}, EV_SYN, SYN_REPORT, 0},};

            if (param) m_virtualDevice->emitEvents(scrollInputEvents);
          }
          else if (action->type() == Action::Type::VolumeControl)
          {
            if (!m_virtualDevice) return;

            auto param = static_cast<VolumeControlAction*>(action.get())->param;
            uint16_t keyCode = (param > 0)? KEY_VOLUMEUP: KEY_VOLUMEDOWN;
            const std::vector<input_event> curVolInputEvents = {{{}, EV_KEY, keyCode, abs(param)}, {{}, EV_SYN, SYN_REPORT, 0},
                                                                {{}, EV_KEY, keyCode, 0}, {{}, EV_SYN, SYN_REPORT, 0},};
            if (param) m_virtualDevice->emitEvents(curVolInputEvents);
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
        connection.disconnect();
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

        // Note: During a Next or Back button press the Logitech Spotlight device can send
        // move events via hid++ notifications. It seems that just when releasing the
        // next or back button sometimes a mouse move event 'leaks' through here as
        // relative input event causing the spotlight to be activated.
        // The workaround skips a first input move event from the logitech spotlight device.
        const bool isLogitechSpotlight = connection.deviceId().vendorId == 0x46d
          && (connection.deviceId().productId == 0xc53e || connection.deviceId().productId == 0xb503);
        const bool logitechIsFirst = isLogitechSpotlight && workaroundLogitechFirstMoveEvent;

        if (isLogitechSpotlight)
        {
          workaroundLogitechFirstMoveEvent = false;
          if(!logitechIsFirst) {
            if (!spotActive()) setSpotActive(true);
          }
        }
        else if (!m_activeTimer->isActive()) {
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

// // -------------------------------------------------------------------------------------------------
// void Spotlight::onHidppDataAvailable(int fd, SubHidppConnection& connection)
// {
//     // Wireless Notification from the Spotlight device
//     auto wnIndex = connection.getFeatureSet()->getFeatureIndex(FeatureCode::WirelessDeviceStatus);
//     if (wnIndex && readVal.at(2) == wnIndex) {    // Logitech spotlight presenter unit got online.
//       if (!connection.isOnline()) connection.initialize();
//     }

//     // Process reprogrammed keys : Next Hold and Back Hold
//     auto rcIndex = connection.getFeatureSet()->getFeatureIndex(FeatureCode::ReprogramControlsV4);
//     if (rcIndex && readVal.at(2) == rcIndex)     // Button (for which hold events are on) related message.
//     {
//       auto eventCode = static_cast<uint8_t>(readVal.at(3));
//       auto buttonCode = static_cast<uint8_t>(readVal.at(5));
//       if (eventCode == 0x00) {  // hold start/stop events
//         switch (buttonCode) {
//           case 0xda:
//             logDebug(hid) << "Next Hold Event ";
//             m_holdButtonStatus.setButton(HoldButtonStatus::HoldButtonType::Next);
//             break;
//           case 0xdc:
//             logDebug(hid) << "Back Hold Event ";
//             m_holdButtonStatus.setButton(HoldButtonStatus::HoldButtonType::Back);
//             break;
//           case 0x00:
//             // hold event over.
//             logDebug(hid) << "Hold Event over.";
//             m_holdButtonStatus.reset();
//         }
//       }
//       else if (eventCode == 0x10) {   // mouse move event
//         // Mouse data is sent as 4 byte information starting at 5th byte and ending at 8th.
//         // out of these 6th byte and 8th bytes are x and y relative change, respectively.
//         // Not sure about meaning of 5th and 7th bytes. However during testing
//         // the 5th byte shows horizonal scroll towards right if rel value is -1 otherwise left scroll (0)
//         // the 7th byte shows vertical scroll towards up if rel value is -1 otherwise down scroll (0)
//         auto byteToRel = [](int i){return ( (i<128) ? i : 256-i);};   // convert the byte to relative motion in x or y
//         int x = byteToRel(readVal.at(5));
//         int y = byteToRel(readVal.at(7));

//         //auto action = connection.inputMapper()->getAction(m_holdButtonStatus.keyEventSeq());
//         auto action = std::shared_ptr<Action>{};

//         if (action && !action->empty())
//         {
//           auto getReducedParam = [](int param, int limit=2){  // reduce the values from Spotlight device for better scroll behavior
//             int minVal=5;
//             if (abs(param) < minVal) return 0;        // ignore small device movement

//             auto sign = (param == 0)? 0: ((param > 0)? 1:-1);
//             return ((abs(param) > minVal*limit)? sign*minVal*limit : param)/minVal;    // limit return value between -limit to limit
//           };

//           if (action->type() == Action::Type::ScrollHorizontal)
//           {
//             const auto scrollHAction = static_cast<ScrollHorizontalAction*>(action.get());
//             scrollHAction->param = -(getReducedParam(x));
//           }
//           if (action->type() == Action::Type::ScrollVertical)
//           {
//             const auto scrollVAction = static_cast<ScrollVerticalAction*>(action.get());
//             scrollVAction->param = getReducedParam(y);
//           }
//           if(action->type() == Action::Type::VolumeControl)
//           {
//             const auto volumeControlAction = static_cast<VolumeControlAction*>(action.get());
//             volumeControlAction->param = -getReducedParam(y, 3);
//           }

//           // feed the keystroke to InputMapper and let it trigger the associated action
//           for (auto key_event: m_holdButtonStatus.keyEventSeq()) connection.inputMapper()->addEvents(key_event);
//         }
//         m_holdButtonStatus.addEvent();
//       }
//     }
//   }
// }

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
