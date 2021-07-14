// This file is part of Projecteur - https://github.com/jahnf/projecteur - See LICENSE.md and README.md
#include "device.h"

#include "deviceinput.h"
#include "devicescan.h"
#include "logging.h"

#include <QSocketNotifier>

#include <fcntl.h>
#include <linux/hidraw.h>
#include <unistd.h>

LOGGING_CATEGORY(device, "device")
LOGGING_CATEGORY(hid, "HID")

namespace  {
  // -----------------------------------------------------------------------------------------------
  static const auto registeredComparator_ = QMetaType::registerComparators<DeviceId>();

  const auto hexId = logging::hexId;
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
                        .arg(m_deviceName, hexId(m_deviceId.vendorId),
                             hexId(m_deviceId.productId), path);
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
  if (type() == ConnectionType::Event)
    return (m_readNotifier && m_readNotifier->isEnabled());
  if (type() == ConnectionType::Hidraw)
    return (m_readNotifier && m_readNotifier->isEnabled()) && (m_writeNotifier);
  return false;
}

// -------------------------------------------------------------------------------------------------
void SubDeviceConnection::disconnect() {
  m_readNotifier.reset();
  m_writeNotifier.reset();
}

// -------------------------------------------------------------------------------------------------
void SubDeviceConnection::disable() {
  if (m_readNotifier) m_readNotifier->setEnabled(false);
  if (m_writeNotifier) m_writeNotifier->setEnabled(false);
}

// -------------------------------------------------------------------------------------------------
void SubDeviceConnection::disableWrite() {
  if (m_writeNotifier) m_writeNotifier->setEnabled(false);
}

// -------------------------------------------------------------------------------------------------
void SubDeviceConnection::enableWrite() {
  if (m_writeNotifier) m_writeNotifier->setEnabled(true);
}

// -------------------------------------------------------------------------------------------------
const std::shared_ptr<InputMapper>& SubDeviceConnection::inputMapper() const  {
  return m_inputMapper;
}

// -------------------------------------------------------------------------------------------------
QSocketNotifier* SubDeviceConnection::socketReadNotifier() {
  return m_readNotifier.get();
}

// -------------------------------------------------------------------------------------------------
QSocketNotifier* SubDeviceConnection::socketWriteNotifier() {
  return m_writeNotifier.get();
}

// -------------------------------------------------------------------------------------------------
SubEventConnection::SubEventConnection(Token, const QString& path)
  : SubDeviceConnection(path, ConnectionType::Event, ConnectionMode::ReadOnly) {}

// -------------------------------------------------------------------------------------------------
std::shared_ptr<SubEventConnection> SubEventConnection::create(const DeviceScan::SubDevice& sd,
                                                               const DeviceConnection& dc)
{
  const int evfd = ::open(sd.deviceFile.toLocal8Bit().constData(), O_RDONLY, 0);

  if (evfd == -1) {
    logWarn(device) << tr("Cannot open event device '%1' for read.").arg(sd.deviceFile);
    return std::shared_ptr<SubEventConnection>();
  }

  struct input_id id{};
  ioctl(evfd, EVIOCGID, &id); // get the event sub-device id

  // Check against given device id
  if (id.vendor != dc.deviceId().vendorId || id.product != dc.deviceId().productId)
  {
    ::close(evfd);
    logDebug(device) << tr("Device id mismatch: %1 (%2:%3)")
                        .arg(sd.deviceFile, hexId(id.vendor), hexId(id.product));
    return std::shared_ptr<SubEventConnection>();
  }

  unsigned long bitmask = 0;
  if (ioctl(evfd, EVIOCGBIT(0, sizeof(bitmask)), &bitmask) < 0)
  {
    ::close(evfd);
    logWarn(device) << tr("Cannot get device properties: %1 (%2:%3)")
                       .arg(sd.deviceFile, hexId(id.vendor), hexId(id.product));
    return std::shared_ptr<SubEventConnection>();
  }

  auto connection = std::make_shared<SubEventConnection>(Token{}, sd.deviceFile);

  if (!!(bitmask & (1 << EV_SYN))) connection->m_details.deviceFlags |= DeviceFlag::SynEvents;
  if (!!(bitmask & (1 << EV_REP))) connection->m_details.deviceFlags |= DeviceFlag::RepEvents;
  if (!!(bitmask & (1 << EV_KEY))) connection->m_details.deviceFlags |= DeviceFlag::KeyEvents;
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

  fcntl(evfd, F_SETFL, fcntl(evfd, F_GETFL, 0) | O_NONBLOCK);
  if ((fcntl(evfd, F_GETFL, 0) & O_NONBLOCK) == O_NONBLOCK) {
    connection->m_details.deviceFlags |= DeviceFlag::NonBlocking;
  }

  // Create socket notifier
  connection->m_readNotifier = std::make_unique<QSocketNotifier>(evfd, QSocketNotifier::Read);
  QSocketNotifier* const notifier = connection->m_readNotifier.get();
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

// -------------------------------------------------------------------------------------------------
SubHidrawConnection::SubHidrawConnection(Token, const QString& path)
  : SubDeviceConnection(path, ConnectionType::Hidraw, ConnectionMode::ReadWrite) {}

// -------------------------------------------------------------------------------------------------
std::shared_ptr<SubHidrawConnection> SubHidrawConnection::create(const DeviceScan::SubDevice& sd,
                                                                 const DeviceConnection& dc)
{
  const int devfd = ::open(sd.deviceFile.toLocal8Bit().constData(), O_RDWR|O_NONBLOCK , 0);

  if (devfd == -1) {
    logWarn(device) << tr("Cannot open hidraw device '%1' for read/write.").arg(sd.deviceFile);
    return std::shared_ptr<SubHidrawConnection>();
  }

  int descriptorSize = 0;
  // Get Report Descriptor Size
  if (ioctl(devfd, HIDIOCGRDESCSIZE, &descriptorSize) < 0) {
    logWarn(device) << tr("Cannot retrieve report descriptor size of hidraw device '%1'.").arg(sd.deviceFile);
    return std::shared_ptr<SubHidrawConnection>();
  }

  struct hidraw_report_descriptor reportDescriptor{};
  reportDescriptor.size = descriptorSize;
  if (ioctl(devfd, HIDIOCGRDESC, &reportDescriptor) < 0) {
    logWarn(device) << tr("Cannot retrieve report descriptor of hidraw device '%1'.").arg(sd.deviceFile);
    return std::shared_ptr<SubHidrawConnection>();
  }

  struct hidraw_devinfo devinfo{};
  // get the hidraw sub-device id info
  if (ioctl(devfd, HIDIOCGRAWINFO, &devinfo) < 0) {
    logWarn(device) << tr("Cannot get info from hidraw device '%1'.").arg(sd.deviceFile);
    return std::shared_ptr<SubHidrawConnection>();
  };

  // Check against given device id
  if (static_cast<unsigned short>(devinfo.vendor) != dc.deviceId().vendorId
      || static_cast<unsigned short>(devinfo.product) != dc.deviceId().productId)
  {
    ::close(devfd);
    logDebug(device) << tr("Device id mismatch: %1 (%2:%3)")
                        .arg(sd.deviceFile, hexId(devinfo.vendor), hexId(devinfo.product));
    return std::shared_ptr<SubHidrawConnection>();
  }

  auto connection = std::make_shared<SubHidrawConnection>(Token{}, sd.deviceFile);

  fcntl(devfd, F_SETFL, fcntl(devfd, F_GETFL, 0) | O_NONBLOCK);
  if ((fcntl(devfd, F_GETFL, 0) & O_NONBLOCK) == O_NONBLOCK) {
    connection->m_details.deviceFlags |= DeviceFlag::NonBlocking;
  }

  // For now vibration is only supported for the Logitech Spotlight (USB)
  // TODO A more generic approach
  if (dc.deviceId().vendorId == 0x46d && dc.deviceId().productId == 0xc53e) {
    connection->m_details.deviceFlags |= DeviceFlag::Vibrate;
  }

  // Create read and write socket notifiers
  connection->m_readNotifier = std::make_unique<QSocketNotifier>(devfd, QSocketNotifier::Read);
  QSocketNotifier* const readNotifier = connection->m_readNotifier.get();
  // Auto clean up and close descriptor on destruction of notifier
  connect(readNotifier, &QSocketNotifier::destroyed, [readNotifier]() {
    ::close(static_cast<int>(readNotifier->socket()));
  });

  connection->m_writeNotifier = std::make_unique<QSocketNotifier>(devfd, QSocketNotifier::Write);
  QSocketNotifier* const writeNotifier = connection->m_writeNotifier.get();
  // Auto clean up and close descriptor on destruction of notifier
  connect(writeNotifier, &QSocketNotifier::destroyed, [writeNotifier]() {
    ::close(static_cast<int>(writeNotifier->socket()));
  });

  connection->m_details.phys = sd.phys;
  connection->disableWrite(); // disable write notifier
  return connection;
}

// -------------------------------------------------------------------------------------------------
ssize_t SubDeviceConnection::sendData(const QByteArray& hidppMsg)
{
  ssize_t res = -1;
  bool isValidMsg = (hidppMsg.length() == 7 && hidppMsg.at(0) == 0x10);             // HID++ short message
  isValidMsg = isValidMsg || (hidppMsg.length() == 20 && hidppMsg.at(0) == 0x11);   // HID++ long message

  if (type() == ConnectionType::Hidraw && mode() == ConnectionMode::ReadWrite
          && m_writeNotifier && isValidMsg)
  {
    enableWrite();
    const auto notifier = socketWriteNotifier();
    res = ::write(notifier->socket(), hidppMsg.data(), hidppMsg.length());
    logDebug(hid) << "Write" << hidppMsg.toHex(':') << "to" << path();
    disableWrite();
  }

  return res;
}


// -------------------------------------------------------------------------------------------------
ssize_t SubDeviceConnection::sendData(const void* hidppMsg, size_t hidppMsgLen)
{
  const QByteArray hidppMsgArr(reinterpret_cast<const char*>(hidppMsg), hidppMsgLen);

  return sendData(hidppMsgArr);
}
