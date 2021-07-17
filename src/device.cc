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
void DeviceConnection::queryBatteryStatus()
{
  if (subDeviceCount() > 0) {
    for (const auto& sd: subDevices()) {
      if (sd.second->type() == ConnectionType::Hidraw && sd.second->mode() == ConnectionMode::ReadWrite) {
        sd.second->queryBatteryStatus();
      }
    }
  }
}

// -------------------------------------------------------------------------------------------------
void DeviceConnection::setBatteryInfo(QByteArray batteryData)
{
  if (batteryData.length() == 3)
  {
    m_batteryInfo.status = static_cast<BatteryStatus>(batteryData.at(2));
    m_batteryInfo.currentLevel = static_cast<uint8_t>(batteryData.at(0));
    m_batteryInfo.nextReportedLevel = static_cast<uint8_t>(batteryData.at(1));
  }
}

// -------------------------------------------------------------------------------------------------
SubDeviceConnection::SubDeviceConnection(const QString& path, ConnectionType type, ConnectionMode mode, BusType busType)
  : m_details(path, type, mode, busType) {}

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
  : SubDeviceConnection(path, ConnectionType::Event, ConnectionMode::ReadOnly, BusType::Unknown) {}

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
  connection->m_details.busType = dc.deviceId().busType;

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
  : SubDeviceConnection(path, ConnectionType::Hidraw, ConnectionMode::ReadWrite, BusType::Unknown) {}

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
  connection->m_details.busType = dc.deviceId().busType;

  fcntl(devfd, F_SETFL, fcntl(devfd, F_GETFL, 0) | O_NONBLOCK);
  if ((fcntl(devfd, F_GETFL, 0) & O_NONBLOCK) == O_NONBLOCK) {
    connection->m_details.deviceFlags |= DeviceFlag::NonBlocking;
  }

  // For now vibration is only supported for the Logitech Spotlight (USB and Bluetooth)
  if (dc.deviceId().vendorId == 0x46d && (dc.deviceId().productId == 0xc53e || dc.deviceId().productId == 0xb503)) {
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
  connection->initSubDevice();
  return connection;
}

// -------------------------------------------------------------------------------------------------
void SubDeviceConnection::initSubDevice()
{
  struct timespec ts;
  int msec = 50;
  ts.tv_sec = msec / 1000;
  ts.tv_nsec = (msec % 1000) * 1000000;

  resetSubDevice(ts);

  queryBatteryStatus();

  // Add other configuration to enable features in device
  // like enabling on Next and back button on hold functionality.
  // No intialization needed for Event Sub device
}

// -------------------------------------------------------------------------------------------------
void SubDeviceConnection::resetSubDevice(struct timespec delay)
{
  // Ping spotlight device for checking if is online
  pingSubDevice();
  ::nanosleep(&delay, &delay);

  // Reset device: Get rid of any device configuration by other programs
  // Reset USB dongle
  if (m_details.busType == BusType::Usb) {
    {const uint8_t data[] = {0x10, 0xff, 0x81, 0x00, 0x00, 0x00, 0x00};
    sendData(data, sizeof(data), false);}
    ::nanosleep(&delay, &delay);
    {const uint8_t data[] = {0x10, 0xff, 0x80, 0x00, 0x00, 0x01, 0x00};
    sendData(data, sizeof(data), false);}
    ::nanosleep(&delay, &delay);
  }

  // Reset spotlight device
  {const uint8_t data[] = {0x10, 0x01, 0x05, 0x1d, 0x00, 0x00, 0x00};
  sendData(data, sizeof(data), false);}
  ::nanosleep(&delay, &delay);
}

// -------------------------------------------------------------------------------------------------
void SubDeviceConnection::pingSubDevice()
{
  const uint8_t pingCmd[] = {0x10, 0x01, 0x00, 0x1d, 0x00, 0x00, 0x5d};
  sendData(pingCmd, sizeof(pingCmd), false);
}

// -------------------------------------------------------------------------------------------------
void SubDeviceConnection::queryBatteryStatus()
{
  // if we make battery feature request packet by sending {0x11, 0x01, 0x00, 0x0d, 0x10, 0x00 ... padded by 0x00}
  // batteryFeatureID is the fifth byte obtained by as device response.
  // batteryFeatureID may differ for different logitech devices and may change after firmware update.
  // last checked, batteryFeatureID was 0x06 for logitech spotlight.

  const uint8_t batteryFeatureID = 0x06;
  const uint8_t batteryCmd[] = {0x10, 0x01, batteryFeatureID, 0x0d, 0x00, 0x00, 0x00};
  sendData(batteryCmd, sizeof(batteryCmd), false);
}

// -------------------------------------------------------------------------------------------------
ssize_t SubDeviceConnection::sendData(const QByteArray& hidppMsg, bool checkDeviceOnline)
{
  ssize_t res = -1;

  // If the message have 0xff as second byte, it is meant for USB dongle hence,
  // should not be send when device is connected on bluetooth.
  //
  //
  // Logitech Spotlight (USB) can receive data in two different length.
  //   1. Short (10 byte long starting with 0x10)
  //   2. Long (20 byte long starting with 0x11)
  // However, bluetooth connection only accepts data in long (20 byte) packets.
  // For converting standard short length data to long length data, change the first byte to 0x11 and
  // pad the end of message with 0x00 to acheive the length of 20.

  QByteArray _hidppMsg(hidppMsg);
  if (m_details.busType == BusType::Bluetooth) {
    if (static_cast<uint8_t>(hidppMsg.at(1)) == 0xff){
      logDebug(hid) << "Invalid packet" << hidppMsg.toHex() << "for spotlight connected on bluetooth.";
      return res;
    }

    if (hidppMsg.at(0) == 0x10) {
      _hidppMsg.clear();
      _hidppMsg.append(0x11);
      _hidppMsg.append(hidppMsg.mid(1));
      QByteArray padding(20 - _hidppMsg.length(), 0);
      _hidppMsg.append(padding);
    }
  }

  bool isValidMsg = (_hidppMsg.length() == 7 && _hidppMsg.at(0) == 0x10);             // HID++ short message
  isValidMsg = isValidMsg || (_hidppMsg.length() == 20 && _hidppMsg.at(0) == 0x11);   // HID++ long message

  // If checkDeviceOnline is true then do not send the packet if device is not online/active.
  if (checkDeviceOnline && !isOnline()) {
    logInfo(hid) << "The device is not active. Activate it by pressing any button on device.";
    return res;
  }

  if (type() == ConnectionType::Hidraw && mode() == ConnectionMode::ReadWrite
          && m_writeNotifier && isValidMsg) {
    enableWrite();
    const auto notifier = socketWriteNotifier();
    res = ::write(notifier->socket(), _hidppMsg.data(), _hidppMsg.length());
    disableWrite();

    if (res == _hidppMsg.length()) {
      logDebug(hid) << "Write" << _hidppMsg.toHex() << "to" << path();
    } else {
      logWarn(hid) << "Writing to" << path() << "failed.";
    }
  }

  return res;
}

// -------------------------------------------------------------------------------------------------
ssize_t SubDeviceConnection::sendData(const void* hidppMsg, size_t hidppMsgLen, bool checkDeviceOnline)
{
  const QByteArray hidppMsgArr(reinterpret_cast<const char*>(hidppMsg), hidppMsgLen);
  return sendData(hidppMsgArr, checkDeviceOnline);
}
