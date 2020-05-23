// This file is part of Projecteur - https://github.com/jahnf/projecteur - See LICENSE.md and README.md
#include "spotlight.h"

#include "deviceinput.h"
#include "virtualdevice.h"
#include "logging.h"

#include <QSocketNotifier>
#include <QTimer>
#include <QVarLengthArray>

#include <fcntl.h>
#include <sys/inotify.h>
#include <sys/ioctl.h>
#include <linux/input.h>
#include <unistd.h>
#include <vector>

DECLARE_LOGGING_CATEGORY(device)

namespace {
} // --- end anonymous namespace

// -------------------------------------------------------------------------------------------------
Spotlight::Spotlight(QObject* parent, Options options)
  : QObject(parent)
  , m_options(std::move(options))
  , m_activeTimer(new QTimer(this))
  , m_connectionTimer(new QTimer(this))
{
  m_activeTimer->setSingleShot(true);
  m_activeTimer->setInterval(600);

  connect(m_activeTimer, &QTimer::timeout, this, [this](){
    m_spotActive = false;
    emit spotActiveChanged(false);
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
  for (const auto& dc : m_deviceConnections)
  {
    for (const auto& c : dc.second->map) {
      if (c.second && c.second->isConnected())
        return true;
    }
  }
  return false;
}

// -------------------------------------------------------------------------------------------------
uint32_t Spotlight::connectedDeviceCount() const
{
  uint32_t count = 0;
  for (const auto& dc : m_deviceConnections)
  {
    for (const auto& c : dc.second->map) {
      if (c.second && c.second->isConnected()) {
        ++count; break;
      }
    }
  }
  return count;
}

// -------------------------------------------------------------------------------------------------
QList<Spotlight::ConnectedDeviceInfo> Spotlight::connectedDevices() const
{
  QList<ConnectedDeviceInfo> devices;
  for (const auto& dc : m_deviceConnections) {
    devices.push_back(ConnectedDeviceInfo{dc.first, dc.second->deviceName});
  }
  return devices;
}

// -------------------------------------------------------------------------------------------------
int Spotlight::connectDevices()
{
  const auto scanResult = DeviceScan::getDevices(m_options.additionalDevices);
  for (const auto& dev : scanResult.devices)
  {
    auto& deviceConnection = m_deviceConnections[dev.id];
    if (!deviceConnection) {
      deviceConnection = std::make_unique<DeviceConnection>(dev.id,
                                                            dev.userName.size() ? dev.userName : dev.name,
                                                            m_virtualDevice);
    }

    const bool anyConnectedBefore = anySpotlightDeviceConnected();
    for (const auto scanSubDevice : dev.subDevices)
    {
      if (scanSubDevice.type != DeviceScan::SubDevice::Type::Event) continue;
      if (!scanSubDevice.deviceReadable) continue;

      auto find_it = deviceConnection->map.find(scanSubDevice.deviceFile);

      // Check if a connection for the path exists...
      if (find_it != deviceConnection->map.end()
          && find_it->second
          && find_it->second->isConnected()) {
        continue;
      }
      else {
        auto subDeviceConnection = SubEventConnection::create(scanSubDevice, *deviceConnection);
        if (subDeviceConnection && subDeviceConnection->isConnected()
            && addInputEventHandler(subDeviceConnection))
        {
          deviceConnection->map[scanSubDevice.deviceFile] = std::move(subDeviceConnection);
          if (deviceConnection->map.size() == 1)
          {
            QTimer::singleShot(0, this,
            [this, id = dev.id, devName = deviceConnection->deviceName, anyConnectedBefore](){
              logInfo(device) << tr("Connected device: %1 (%2:%3)")
                                 .arg(devName)
                                 .arg(id.vendorId, 4, 16, QChar('0'))
                                 .arg(id.productId, 4, 16, QChar('0'));
              emit deviceConnected(id, devName);
              if (!anyConnectedBefore) emit anySpotlightDeviceConnectedChanged(true);
            });
          }
          logDebug(device) << tr("Connected sub-device: %1 (%2:%3) %4")
                              .arg(deviceConnection->deviceName)
                              .arg(dev.id.vendorId, 4, 16, QChar('0'))
                              .arg(dev.id.productId, 4, 16, QChar('0'))
                              .arg(scanSubDevice.deviceFile);
          emit subDeviceConnected(dev.id, deviceConnection->deviceName, scanSubDevice.deviceFile);
        }
        else {
          deviceConnection->map.erase(scanSubDevice.deviceFile);
        }
      }
    }
    if (deviceConnection->map.empty()) {
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

    auto& connInfo = dc_it->second;
    auto& connMap = connInfo->map;
    auto find_it = connMap.find(devicePath);

    if (find_it != connMap.end())
    {
      find_it->second->disconnect();
      logDebug(device) << tr("Disconnected sub-device: %1 (%2:%3) %4")
                          .arg(connInfo->deviceName).arg(dc_it->first.vendorId, 4, 16, QChar('0'))
                          .arg(dc_it->first.productId, 4, 16, QChar('0')).arg(devicePath);
      emit subDeviceDisconnected(dc_it->first, connInfo->deviceName, devicePath);
      connMap.erase(find_it);
    }

    if (connMap.empty())
    {
      logInfo(device) << tr("Disconnected device: %1 (%2:%3)")
                         .arg(connInfo->deviceName).arg(dc_it->first.vendorId, 4, 16, QChar('0'))
                         .arg(dc_it->first.productId, 4, 16, QChar('0'));
      emit deviceDisconnected(dc_it->first, connInfo->deviceName);
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
        connection.disable();
        if (!anySpotlightDeviceConnected()) {
          emit anySpotlightDeviceConnectedChanged(false);
        }
        QTimer::singleShot(0, this, [this, devicePath=connection.path()](){
          removeDeviceConnection(devicePath);
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
      if (isMouseMoveEvent) {
        if (!connection.inputMapper()->recordingMode()) // skip activation of spot in recording mode
        {
          if (!m_activeTimer->isActive()) {
            m_spotActive = true;
            emit spotActiveChanged(true);
          }
          m_activeTimer->start();
        }
        // Skip input mapping for mouse move events completely
        if (m_virtualDevice) m_virtualDevice->emitEvents(buf.data(), buf.pos());
      }
      else
      { // Forward events to input mapper for the device
        connection.inputMapper()->addEvents(buf.data(), buf.pos());
      }
      buf.reset();
    }
    else if(buf.pos() >= buf.size())
    { // No idea if this will ever happen, but log it to make sure we get notified.
      logWarning(device) << tr("Discarded %1 input events without EV_SYN.").arg(buf.size());
      connection.inputMapper()->resetState();
      buf.reset();
    }

    if (!isNonBlocking) break;
  } // end while loop
}

// -------------------------------------------------------------------------------------------------
bool Spotlight::addInputEventHandler(std::shared_ptr<SubEventConnection> connection)
{
  if (!connection || connection->type() != ConnectionType::Event || !connection->isConnected()) {
    return false;
  }

  QSocketNotifier* const notifier = connection->socketNotifier();
  connect(notifier, &QSocketNotifier::activated, this,
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
  connect(notifier, &QSocketNotifier::activated, [this](int fd)
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
