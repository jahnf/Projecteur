// This file is part of Projecteur - https://github.com/jahnf/projecteur - See LICENSE.md and README.md
#include "device.h"

#include "deviceinput.h"
#include "devicescan.h"
#include "hidpp.h"
#include "logging.h"

#include <QSocketNotifier>
#include <QTimer>

#include <fcntl.h>
#include <linux/hidraw.h>
#include <unistd.h>

LOGGING_CATEGORY(device, "device")
LOGGING_CATEGORY(hid, "HID")

namespace  {
  // -----------------------------------------------------------------------------------------------
  static const auto registeredComparator_ = QMetaType::registerComparators<DeviceId>();

  const auto hexId = logging::hexId;

  // -----------------------------------------------------------------------------------------------
  bool deviceHasHidppSupport(const DeviceId& id) {
    // HID++ only for Logitech devices
    return id.vendorId == 0x046d;
  }
}

// -------------------------------------------------------------------------------------------------
DeviceConnection::DeviceConnection(const DeviceId& id, const QString& name,
                                   std::shared_ptr<VirtualDevice> vdev)
  : m_deviceId(id)
  , m_deviceName(name)
  , m_inputMapper(std::make_shared<InputMapper>(std::move(vdev)))
  , m_featureSet(std::make_shared<FeatureSet>())
{
}

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
  connect(&*sdc, &SubDeviceConnection::flagsChanged, this, [this, path](){
    emit subDeviceFlagsChanged(m_deviceId, path);
  });

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
  for (const auto& sd: subDevices())
  {
    if (sd.second->type() == ConnectionType::Hidraw && sd.second->mode() == ConnectionMode::ReadWrite)
    {
      const auto hidrawConn = std::static_pointer_cast<SubHidrawConnection>(sd.second);
      hidrawConn->queryBatteryStatus();
    }
  }
}

// -------------------------------------------------------------------------------------------------
void DeviceConnection::setBatteryInfo(const QByteArray& batteryData)
{
  if (m_featureSet->supportFeatureCode(FeatureCode::BatteryStatus) && batteryData.length() == 3)
  {
    // Battery percent is only meaningful when battery is discharging. However, save them anyway.
    m_batteryInfo.currentLevel = static_cast<uint8_t>(batteryData.at(0) <= 100 ? batteryData.at(0): 100);
    m_batteryInfo.nextReportedLevel = static_cast<uint8_t>(batteryData.at(1) <= 100 ? batteryData.at(1): 100);
    m_batteryInfo.status = static_cast<BatteryStatus>((batteryData.at(2) <= 0x07) ? batteryData.at(2): 0x07);
  }
}

// -------------------------------------------------------------------------------------------------
SubDeviceConnection::SubDeviceConnection(const QString& path, ConnectionType type, ConnectionMode mode)
  : m_details(path, type, mode) {}

// -------------------------------------------------------------------------------------------------
SubDeviceConnection::~SubDeviceConnection() = default;

// -------------------------------------------------------------------------------------------------
DeviceFlags SubDeviceConnection::setFlags(DeviceFlags f, bool set)
{
  const auto previousFlags = flags();
  if (set) {
    m_details.deviceFlags |= f;
  } else {
    m_details.deviceFlags &= ~f;
  }

  if (m_details.deviceFlags != previousFlags) {
    emit flagsChanged(m_details.deviceFlags);
  }
  return m_details.deviceFlags;
}

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
void SubDeviceConnection::setNotifiersEnabled(bool enabled) {
  setReadNotifierEnabled(enabled);
  setWriteNotifierEnabled(enabled);
}

// -------------------------------------------------------------------------------------------------
void SubDeviceConnection::setReadNotifierEnabled(bool enabled) {
  if (m_readNotifier) m_readNotifier->setEnabled(enabled);
}

// -------------------------------------------------------------------------------------------------
void SubDeviceConnection::setWriteNotifierEnabled(bool enabled) {
  if (m_writeNotifier) m_writeNotifier->setEnabled(enabled);
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
  connection->m_deviceID = dc.deviceId();

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
  connection->m_deviceID = dc.deviceId();

  // TODO feature set needs to be a member of a sub hidraw connection
  connection->m_featureSet = dc.getFeatureSet();
  connection->m_featureSet->setHIDDeviceFileDescriptor(devfd);
  // ---

  fcntl(devfd, F_SETFL, fcntl(devfd, F_GETFL, 0) | O_NONBLOCK);
  if ((fcntl(devfd, F_GETFL, 0) & O_NONBLOCK) == O_NONBLOCK) {
    connection->m_details.deviceFlags |= DeviceFlag::NonBlocking;
  }

  if (deviceHasHidppSupport(dc.deviceId())) {
    connection->m_details.deviceFlags |= DeviceFlag::Hidpp;
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
  writeNotifier->setEnabled(false); // Disable write notifier by default
  // Auto clean up and close descriptor on destruction of notifier
  connect(writeNotifier, &QSocketNotifier::destroyed, [writeNotifier]() {
    ::close(static_cast<int>(writeNotifier->socket()));
  });

  connection->m_details.phys = sd.phys;
  return connection;
}

// -------------------------------------------------------------------------------------------------
void SubHidrawConnection::queryBatteryStatus()
{
  if (hasFlags(DeviceFlag::ReportBattery))
  {
    const uint8_t batteryFeatureID = m_featureSet->getFeatureID(FeatureCode::BatteryStatus);
    if (batteryFeatureID)
    {
      const uint8_t batteryCmd[] = {HIDPP_SHORT_MSG, MSG_TO_SPOTLIGHT, batteryFeatureID, 0x0d, 0x00, 0x00, 0x00};
      sendData(batteryCmd, sizeof(batteryCmd), false);
    }
  }
}

// -------------------------------------------------------------------------------------------------
void SubDeviceConnection::pingSubDevice()
{
  constexpr uint8_t rootID = 0x00;  // root ID is always 0x00 in any logitech device
  const uint8_t pingCmd[] = {HIDPP_SHORT_MSG, MSG_TO_SPOTLIGHT, rootID, 0x1d, 0x00, 0x00, 0x5d};
  sendData(pingCmd, sizeof(pingCmd), false);
}

// -------------------------------------------------------------------------------------------------
void SubDeviceConnection::setHIDProtocol(float version) {
  if (version > 0) {
    logDebug(hid) << path() << "is online with protocol version" << version ;
  } else {
    logDebug(hid) << "HID Device with path" << path() << "got deactivated.";
  }
  m_details.hidProtocolVer = version;
}

// -------------------------------------------------------------------------------------------------
void SubHidrawConnection::initialize()
{
  // Currently only HID++ devices need additional initializing
  if (!hasFlags(DeviceFlag::Hidpp)) return;

  constexpr int delay_ms = 20;
  int msgCount = 0;
  // Reset device: get rid of any device configuration by other programs -------
  if (m_deviceID.busType == BusType::Usb)
  {
    // Reset USB dongle
    QTimer::singleShot(delay_ms*msgCount, this, [this](){
      constexpr uint8_t data[] = {HIDPP_SHORT_MSG, MSG_TO_USB_RECEIVER, HIDPP_SHORT_GET_FEATURE, 0x00, 0x00, 0x00, 0x00};
      sendData(data, sizeof(data), false);});
    msgCount++;

    // Turn off software bit and keep the wireless notification bit on
    QTimer::singleShot(delay_ms*msgCount, this, [this](){
      constexpr uint8_t data[] = {HIDPP_SHORT_MSG, MSG_TO_USB_RECEIVER, HIDPP_SHORT_SET_FEATURE, 0x00, 0x00, 0x01, 0x00};
      sendData(data, sizeof(data), false);});
    msgCount++;

    // Initialize USB dongle
    QTimer::singleShot(delay_ms*msgCount, this, [this](){
      constexpr uint8_t data[] = {HIDPP_SHORT_MSG, MSG_TO_USB_RECEIVER, HIDPP_SHORT_SET_FEATURE, 0x02, 0x02, 0x00, 0x00};
      sendData(data, sizeof(data), false);});
    msgCount++;

    QTimer::singleShot(delay_ms*msgCount, this, [this](){
      constexpr uint8_t data[] = {HIDPP_SHORT_MSG, MSG_TO_USB_RECEIVER, HIDPP_SHORT_SET_FEATURE, 0x00, 0x00, 0x09, 0x00};
      sendData(data, sizeof(data), false);});
    msgCount++;
  }

  DeviceFlags featureFlags = DeviceFlag::NoFlags;
  // Read HID++ FeatureSet (Feature ID and Feature Code pairs) from logitech device
  setNotifiersEnabled(false);
  if (m_featureSet->getFeatureCount() == 0) m_featureSet->populateFeatureTable();
  if (m_featureSet->getFeatureCount()) {
    logDebug(hid) << "Loaded" << m_featureSet->getFeatureCount() << "features for" << path();
    if (m_featureSet->supportFeatureCode(FeatureCode::PresenterControl)) {
      featureFlags |= DeviceFlag::Vibrate;
      logDebug(hid) << "SubDevice" << path() << "reported Vibration capabilities.";
    }
    if (m_featureSet->supportFeatureCode(FeatureCode::BatteryStatus)) {
      featureFlags |= DeviceFlag::ReportBattery;
      logDebug(hid) << "SubDevice" << path() << "can communicate battery information.";
    }
  } else {
    logWarn(hid) << "Loading FeatureSet for" << path() << "failed. Device might be inactive.";
    logInfo(hid) << "Press any button on device to activate it.";
  }
  setReadNotifierEnabled(true);

  // Reset spotlight device
  if (m_featureSet->getFeatureCount()) {
    const auto resetID = m_featureSet->getFeatureID(FeatureCode::Reset);
    if (resetID) {
      QTimer::singleShot(delay_ms*msgCount, this, [this, resetID](){
        const uint8_t data[] = {HIDPP_SHORT_MSG, MSG_TO_SPOTLIGHT, resetID, 0x1d, 0x00, 0x00, 0x00};
        sendData(data, sizeof(data), false);});
      msgCount++;
    }
  }
  // Device Resetting complete -------------------------------------------------

  if (m_deviceID.busType == BusType::Usb) {
    // Ping spotlight device for checking if is online
    // the response will have the version for HID++ protocol.
    QTimer::singleShot(delay_ms*msgCount, this, [this](){pingSubDevice();});
    msgCount++;
  } else if (m_deviceID.busType == BusType::Bluetooth) {
    // Bluetooth connection mean HID++ v2.0+.
    // Setting version to 6.4: same as USB connection.
    setHIDProtocol(6.4);
  }

  setFlags(featureFlags, true);
  // Add other configuration to enable features in device
  // like enabling on Next and back button on hold functionality.
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
  if (m_deviceID.busType == BusType::Bluetooth) {
    if (static_cast<uint8_t>(hidppMsg.at(1)) == MSG_TO_USB_RECEIVER){
      logDebug(hid) << "Invalid packet" << hidppMsg.toHex() << "for spotlight connected on bluetooth.";
      return res;
    }

    if (hidppMsg.at(0) == HIDPP_SHORT_MSG) {
      _hidppMsg = HIDPP::shortToLongMsg(hidppMsg);
    }
  }

  bool isValidMsg = (_hidppMsg.length() == 7 && _hidppMsg.at(0) == HIDPP_SHORT_MSG);             // HID++ short message
  isValidMsg = isValidMsg || (_hidppMsg.length() == 20 && _hidppMsg.at(0) == HIDPP_LONG_MSG);   // HID++ long message

  // If checkDeviceOnline is true then do not send the packet if device is not online/active.
  if (checkDeviceOnline && !isOnline()) {
    logInfo(hid) << "The device is not active. Activate it by pressing any button on device.";
    return res;
  }

  if (type() == ConnectionType::Hidraw && mode() == ConnectionMode::ReadWrite
          && m_writeNotifier && isValidMsg)
  {
    const auto notifier = socketWriteNotifier();
    res = ::write(notifier->socket(), _hidppMsg.data(), _hidppMsg.length());

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
