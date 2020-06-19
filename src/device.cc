// This file is part of Projecteur - https://github.com/jahnf/projecteur - See LICENSE.md and README.md
#include "device.h"

#include "deviceinput.h"
#include "devicescan.h"
#include "logging.h"

#include <QSocketNotifier>

#include <fcntl.h>
#include <linux/input.h>
#include <unistd.h>


LOGGING_CATEGORY(device, "device")

namespace  {
  // -----------------------------------------------------------------------------------------------
  static auto registeredComparator_ = QMetaType::registerComparators<DeviceId>();
}

// -------------------------------------------------------------------------------------------------
DeviceConnection::DeviceConnection(const DeviceId& id, const QString& name,
                                   std::shared_ptr<VirtualDevice> vdev)
  : m_deviceId(id), m_deviceName(name), m_inputMapper(std::make_shared<InputMapper>(std::move(vdev))){}

// -------------------------------------------------------------------------------------------------
DeviceConnection::~DeviceConnection() = default;

// -------------------------------------------------------------------------------------------------
bool DeviceConnection::hasSubDevice(const QString& path) const
{
  const auto find_it = m_subDeviceConnections.find(path);
  return (find_it != m_subDeviceConnections.end() && find_it->second && find_it->second->isConnected());
}

// -------------------------------------------------------------------------------------------------
void DeviceConnection::addSubDevice(std::shared_ptr<SubDeviceConnection> sdc)
{
  if (!sdc) return;

  const auto path = sdc->path();
  m_subDeviceConnections[path] = std::move(sdc);
  emit subDeviceConnected(m_deviceId, path);
}

// -------------------------------------------------------------------------------------------------
bool DeviceConnection::removeSubDevice(const QString& path)
{
  auto find_it = m_subDeviceConnections.find(path);
  if (find_it != m_subDeviceConnections.end())
  {
    if (find_it->second) { find_it->second->disconnect(); } // Important
    logDebug(device) << tr("Disconnected sub-device: %1 (%2:%3) %4")
                        .arg(m_deviceName).arg(m_deviceId.vendorId, 4, 16, QChar('0'))
                        .arg(m_deviceId.productId, 4, 16, QChar('0')).arg(path);
    emit subDeviceDisconnected(m_deviceId, path);
    m_subDeviceConnections.erase(find_it);
    return true;
  }
  return false;
}

// -------------------------------------------------------------------------------------------------
SubDeviceConnection::SubDeviceConnection(const QString& path, ConnectionType type, ConnectionMode mode)
  : m_details(path, type, mode) {}

// -------------------------------------------------------------------------------------------------
SubDeviceConnection::~SubDeviceConnection() = default;

// -------------------------------------------------------------------------------------------------
bool SubDeviceConnection::isConnected() const {
  return m_notifier && m_notifier->isEnabled();
}

// -------------------------------------------------------------------------------------------------
void SubDeviceConnection::disconnect() {
  m_notifier.reset();
}

// -------------------------------------------------------------------------------------------------
void SubDeviceConnection::disable() {
  if (m_notifier) m_notifier->setEnabled(false);
}

// -------------------------------------------------------------------------------------------------
const std::shared_ptr<InputMapper>& SubDeviceConnection::inputMapper() const  {
  return m_inputMapper;
}

// -------------------------------------------------------------------------------------------------
QSocketNotifier* SubDeviceConnection::socketNotifier() {
  return m_notifier.get();
}

// -------------------------------------------------------------------------------------------------
SubEventConnection::SubEventConnection(Token, const QString& path)
  : SubDeviceConnection(path, ConnectionType::Event, ConnectionMode::ReadOnly) {}

// -------------------------------------------------------------------------------------------------
std::shared_ptr<SubEventConnection> SubEventConnection::create(const DeviceScan::SubDevice& sd,
                                                               const DeviceConnection& dc)
{
  const int evfd = ::open(sd.deviceFile.toLocal8Bit().constData(), O_RDONLY, 0);

  struct input_id id{};
  ioctl(evfd, EVIOCGID, &id); // get the event sub-device id

  // Check against given device id
  if ( id.vendor != dc.deviceId().vendorId || id.product != dc.deviceId().productId)
  {
    ::close(evfd);
    logDebug(device) << tr("Device id mismatch: %1 (%2:%3)")
                        .arg(sd.deviceFile)
                        .arg(id.vendor, 4, 16, QChar('0'))
                        .arg(id.product, 4, 16, QChar('0'));
    return std::shared_ptr<SubEventConnection>();
  }

  unsigned long bitmask = 0;
  if (ioctl(evfd, EVIOCGBIT(0, sizeof(bitmask)), &bitmask) < 0)
  {
    ::close(evfd);
    logWarn(device) << tr("Cannot get device properties: %1 (%2:%3)")
                        .arg(sd.deviceFile)
                        .arg(id.vendor, 4, 16, QChar('0'))
                        .arg(id.product, 4, 16, QChar('0'));
    return std::shared_ptr<SubEventConnection>();
  }

  auto connection = std::make_shared<SubEventConnection>(Token{}, sd.deviceFile);

  connection->m_details.grabbed = [&dc, evfd, &sd]()
  {
    // Grab device inputs if a virtual device exists.
    if (dc.inputMapper()->virtualDevice())
    {
      const int res = ioctl(evfd, EVIOCGRAB, 1);
      if (res == 0) { return true; }

      // Grab not successful
      logError(device) << tr("Error grabbing device: %1 (return value: %2)").arg(sd.deviceFile).arg(res);
      ioctl(evfd, EVIOCGRAB, 0);
    }
    return false;
  }();

  if (!!(bitmask & (1 << EV_SYN))) connection->m_details.deviceFlags |= DeviceFlag::SynEvents;
  if (!!(bitmask & (1 << EV_REP))) connection->m_details.deviceFlags |= DeviceFlag::RepEvents;
  if (!!(bitmask & (1 << EV_REL)))
  {
    unsigned long relEvents = 0;
    ioctl(evfd, EVIOCGBIT(EV_REL, sizeof(relEvents)), &relEvents);
    const bool hasRelXEvents = !!(relEvents & (1 << REL_X));
    const bool hasRelYEvents = !!(relEvents & (1 << REL_Y));
    if (hasRelXEvents && hasRelYEvents) {
      connection->m_details.deviceFlags |= DeviceFlag::RelativeEvents;
    }
  }

  fcntl(evfd, F_SETFL, fcntl(evfd, F_GETFL, 0) | O_NONBLOCK);
  if ((fcntl(evfd, F_GETFL, 0) & O_NONBLOCK) == O_NONBLOCK) {
    connection->m_details.deviceFlags |= DeviceFlag::NonBlocking;
  }

  // Create socket notifier
  connection->m_notifier = std::make_unique<QSocketNotifier>(evfd, QSocketNotifier::Read);
  QSocketNotifier* const notifier = connection->m_notifier.get();
  // Auto clean up and close descriptor on destruction of notifier
  connect(notifier, &QSocketNotifier::destroyed, [grabbed = connection->m_details.grabbed, notifier]() {
    if (grabbed) {
      ioctl(static_cast<int>(notifier->socket()), EVIOCGRAB, 0);
    }
    ::close(static_cast<int>(notifier->socket()));
  });

  connection->m_inputMapper = dc.inputMapper();
  connection->m_details.phys = sd.phys;

  return connection;
}
