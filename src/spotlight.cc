// This file is part of Projecteur - https://github.com/jahnf/projecteur - See LICENSE.md and README.md
#include "spotlight.h"

#include "deviceinput.h"
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
      [&scanSubDevice, &dc, this]() -> std::shared_ptr<SubDeviceConnection> {
        if (scanSubDevice.type == DeviceScan::SubDevice::Type::Event) {
          auto devCon = SubEventConnection::create(scanSubDevice, *dc);
          if (addInputEventHandler(devCon)) return devCon;
        } else if (scanSubDevice.type == DeviceScan::SubDevice::Type::Hidraw) {
          auto hidCon = SubHidrawConnection::create(scanSubDevice, *dc);
          if(addHIDInputHandler(hidCon)) {
            if (dc->deviceId().vendorId == 0x46d && hidCon->getFeatureSet()->getFeatureCount() == 0) {
              //reconnect device to get the feature table
              connect(hidCon.get(), &SubHidrawConnection::receivedPingResponse,
                      this, [this, hidCon](){
                  removeDeviceConnection(hidCon->path());
                  connectDevices();});
            } else {
              connect(hidCon.get(), &SubHidrawConnection::receivedBatteryInfo,
                    dc.get(), &DeviceConnection::setBatteryInfo);
              connect(hidCon.get(), &SubHidrawConnection::receivedPingResponse,
                    dc.get(), [this, dc](){emit deviceActivated(dc->deviceId(), dc->deviceName());});
            }
            return hidCon;
          }
          if(addHIDInputHandler(hidCon)) return hidCon;
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
  const bool isNonBlocking = !!(connection.flags() & DeviceFlag::NonBlocking);
  while (true)
  {
    auto& buf = connection.inputBuffer();
    auto& ev = buf.current();
    if (::read(fd, &ev, sizeof(ev)) != sizeof(ev))
    {
      if (errno != EAGAIN)
      {
        const bool anyConnectedBefore = anySpotlightDeviceConnected();
        connection.disable();
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
void Spotlight::onHIDDataAvailable(int fd, SubHidrawConnection& connection)
{
  QByteArray readVal(20, 0);
  if (::read(fd, static_cast<void *>(readVal.data()), readVal.length()) < 0)
  {
    if (errno != EAGAIN)
    {
      const bool anyConnectedBefore = anySpotlightDeviceConnected();
      connection.disable();
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
  if (!(readVal.at(0) == 0x10 || readVal.at(0) == 0x11)) {
    return;
  }

  logDebug(hid) << "Received" << readVal.toHex() << "from" << connection.path();

  if (readVal.at(0) == 0x10)    // Logitech HIDPP SHORT message: 7 byte long
  {
    if (readVal.at(2) == 0x41 && !!(readVal.at(3) & 0x04)) {    // wireless notification from USB dongle
      if (readVal.at(4) & (1<<6)) {    // connection between USB dongle and spotlight device broke
        connection.setHIDProtocol(-1);
      } else {                         // Logitech spotlight presenter unit got online and USB dongle acknowledged it.
        // currently it is off as I observed that device send two online packet
        // one with 0x10 and other with 0x11. Currently initsubDevice is triggered
        // on 0x11 packet.
        //connection.initSubDevice();
      }
    }
  }

  if (readVal.at(0) == 0x11)    // Logitech HIDPP LONG message: 20 byte long
  {
    if (readVal.at(2) == 0x00) {
      if (readVal.at(3) == 0x1d && readVal.at(6) == 0x5d) { // response to ping
        auto protocolVer = static_cast<uint8_t>(readVal.at(4)) + static_cast<uint8_t>(readVal.at(5))/10.0;
        connection.setHIDProtocol(protocolVer);
        if (connection.isOnline()) emit connection.receivedPingResponse();
      }
    }

    if (readVal.at(2) == 0x04) {    // Logitech spotlight presenter unit got online.
      connection.initSubDevice();
    }

    if (readVal.at(2) == 0x06 && readVal.at(3) == 0x0d) {  // Battery information packet
      QByteArray batteryData(readVal.mid(4, 3));
      emit connection.receivedBatteryInfo(batteryData);
    }

    // TODO: Process other packets

    if (readVal.at(2) == 0x09 && readVal.at(3) == 0x1d) {
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
bool Spotlight::addHIDInputHandler(std::shared_ptr<SubHidrawConnection> connection)
{
  if (!connection || connection->type() != ConnectionType::Hidraw || !connection->isConnected()) {
    return false;
  }

  QSocketNotifier* const readNotifier = connection->socketReadNotifier();
  connect(readNotifier, &QSocketNotifier::activated, this,
  [this, connection=std::move(connection)](int fd) {
    onHIDDataAvailable(fd, *connection.get());
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
