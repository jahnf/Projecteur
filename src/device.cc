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
  class i18n : public QObject {}; // for i18n and logging

  // -----------------------------------------------------------------------------------------------
  /// Open a hidraw subdevice and
  int openHidrawSubDevice(const DeviceScan::SubDevice& sd, const DeviceId& devId)
  {
    constexpr int errorResult = -1;
    const int devfd = ::open(sd.deviceFile.toLocal8Bit().constData(), O_RDWR|O_NONBLOCK , 0);

    if (devfd == errorResult) {
      logWarn(device) << i18n::tr("Cannot open hidraw device '%1' for read/write.").arg(sd.deviceFile);
      return errorResult;
    }

    { // Get Report Descriptor Size and Descriptor -- currently unused, but if it fails
      // we don't use the device
      int descriptorSize = 0;
      if (ioctl(devfd, HIDIOCGRDESCSIZE, &descriptorSize) < 0)
      {
        logWarn(device) << i18n::tr("Cannot retrieve report descriptor size of hidraw device '%1'.").arg(sd.deviceFile);
        ::close(devfd);
        return errorResult;
      }

      struct hidraw_report_descriptor reportDescriptor {};
      reportDescriptor.size = descriptorSize;
      if (ioctl(devfd, HIDIOCGRDESC, &reportDescriptor) < 0)
      {
        logWarn(device) << i18n::tr("Cannot retrieve report descriptor of hidraw device '%1'.").arg(sd.deviceFile);
        ::close(devfd);
        return errorResult;
      }
    }

    struct hidraw_devinfo devinfo {};
    // get the hidraw sub-device id info
    if (ioctl(devfd, HIDIOCGRAWINFO, &devinfo) < 0)
    {
      logWarn(device) << i18n::tr("Cannot get info from hidraw device '%1'.").arg(sd.deviceFile);
      ::close(devfd);
      return errorResult;
    };

    // Check against given device id
    if (static_cast<unsigned short>(devinfo.vendor) != devId.vendorId
        || static_cast<unsigned short>(devinfo.product) != devId.productId)
    {
      logDebug(device) << i18n::tr("Device id mismatch: %1 (%2:%3)")
                                .arg(sd.deviceFile, hexId(devinfo.vendor), hexId(devinfo.product));
      ::close(devfd);
      return errorResult;
    }

    return devfd;
  }
}

// -------------------------------------------------------------------------------------------------
DeviceConnection::DeviceConnection(const DeviceId& id, const QString& name,
                                   std::shared_ptr<VirtualDevice> vdev)
  : m_deviceId(id)
  , m_deviceName(name)
  , m_inputMapper(std::make_shared<InputMapper>(std::move(vdev)))
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
bool DeviceConnection::hasHidppSupport() const{
  // HID++ only for Logitech devices
  return m_deviceId.vendorId == 0x046d;
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
  const bool hasBattery = std::any_of(m_subDeviceConnections.cbegin(), m_subDeviceConnections.cend(),
                                      [](const auto& sd)
  {
    return (sd.second->type() == ConnectionType::Hidraw &&
            sd.second->mode() == ConnectionMode::ReadWrite &&
            sd.second->hasFlags(DeviceFlag::ReportBattery));
  });

  if (hasBattery && batteryData.length() == 3)
  {
    // Battery percent is only meaningful when battery is discharging. However, save them anyway.
    m_batteryInfo.currentLevel = static_cast<uint8_t>(batteryData.at(0) <= 100 ? batteryData.at(0): 100);
    m_batteryInfo.nextReportedLevel = static_cast<uint8_t>(batteryData.at(1) <= 100 ? batteryData.at(1): 100);
    m_batteryInfo.status = static_cast<BatteryStatus>((batteryData.at(2) <= 0x07) ? batteryData.at(2): 0x07);
  }
}

// -------------------------------------------------------------------------------------------------
SubDeviceConnectionDetails::SubDeviceConnectionDetails(const DeviceScan::SubDevice& sd,
                                                       const DeviceId& id, ConnectionType type,
                                                       ConnectionMode mode)
  : type(type), mode(mode), busType(id.busType), phys(sd.phys), devicePath(sd.deviceFile)
{}

// -------------------------------------------------------------------------------------------------
SubDeviceConnection::SubDeviceConnection(const DeviceScan::SubDevice& sd, const DeviceId& id,
                                         ConnectionType type, ConnectionMode mode)
  : m_details(sd, id, type, mode) {}

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
ssize_t SubDeviceConnection::sendData(const QByteArray&) {
  // do nothing for the base implementation
  return -1;
}

// -------------------------------------------------------------------------------------------------
ssize_t SubDeviceConnection::sendData(const void*, size_t) {
  // do nothing for the base implementation
  return -1;
}

// -------------------------------------------------------------------------------------------------
void SubDeviceConnection::sendVibrateCommand(uint8_t, uint8_t) {}

// -------------------------------------------------------------------------------------------------
SubEventConnection::SubEventConnection(Token, const DeviceScan::SubDevice& sd, const DeviceId& id)
  : SubDeviceConnection(sd, id, ConnectionType::Event, ConnectionMode::ReadOnly) {}

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

  auto connection = std::make_shared<SubEventConnection>(Token{}, sd, dc.deviceId());

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
  return connection;
}

// -------------------------------------------------------------------------------------------------
SubHidrawConnection::SubHidrawConnection(Token, const DeviceScan::SubDevice& sd, const DeviceId& id)
  : SubDeviceConnection(sd, id, ConnectionType::Hidraw, ConnectionMode::ReadWrite) {}

// -------------------------------------------------------------------------------------------------
std::shared_ptr<SubHidrawConnection> SubHidrawConnection::create(const DeviceScan::SubDevice& sd,
                                                                 const DeviceConnection& dc)
{
  const int devfd = openHidrawSubDevice(sd, dc.deviceId());
  if (devfd == -1) return std::shared_ptr<SubHidrawConnection>();

  auto connection = std::make_shared<SubHidrawConnection>(Token{}, sd, dc.deviceId());
  connection->createSocketNotifiers(devfd);

  connection->m_inputMapper = dc.inputMapper();
  connection->m_details.phys = sd.phys;
  return connection;
}

// -------------------------------------------------------------------------------------------------
void SubHidrawConnection::queryBatteryStatus() {}

// -------------------------------------------------------------------------------------------------
void SubDeviceConnection::pingSubDevice()
{
  constexpr uint8_t rootID = 0x00;  // root ID is always 0x00 in any logitech device
  const uint8_t pingCmd[] = {HIDPP::Bytes::SHORT_MSG, HIDPP::Bytes::MSG_TO_SPOTLIGHT, rootID, 0x1d, 0x00, 0x00, 0x5d};
  sendData(pingCmd, sizeof(pingCmd));
}

// -------------------------------------------------------------------------------------------------
void SubDeviceConnection::setHIDppProtocol(float version) {
  // Inform user about the online status of device.
  if (version > 0) {
    if (m_details.HIDppProtocolVer < 0) logInfo(hid) << "HID Device with path" << path() << tr("is now active with protocol version %1.").arg(version);
  } else {
    if (m_details.HIDppProtocolVer > 0) logInfo(hid) << "HID Device with path" << path() << "got deactivated.";
  }
  m_details.HIDppProtocolVer = version;
}

// -------------------------------------------------------------------------------------------------
void SubHidppConnection::initialize()
{
  // Currently only HID++ devices need additional initializing
  if (!hasFlags(DeviceFlag::Hidpp)) return;

  constexpr int delay_ms = 20;
  int msgCount = 0;
  // Reset device: get rid of any device configuration by other programs -------
  if (m_details.busType == BusType::Usb)
  {
    QTimer::singleShot(delay_ms*msgCount, this, [this](){
      const uint8_t data[] = {HIDPP::Bytes::SHORT_MSG, HIDPP::Bytes::MSG_TO_USB_RECEIVER, HIDPP::Bytes::SHORT_GET_FEATURE, 0x00, 0x00, 0x00, 0x00};
      sendData(data, sizeof(data));
    });
    msgCount++;

    // Turn off software bit and keep the wireless notification bit on
    QTimer::singleShot(delay_ms*msgCount, this, [this](){
      constexpr uint8_t data[] = {HIDPP::Bytes::SHORT_MSG, HIDPP::Bytes::MSG_TO_USB_RECEIVER, HIDPP::Bytes::SHORT_SET_FEATURE, 0x00, 0x00, 0x01, 0x00};
      sendData(data, sizeof(data));});
    msgCount++;

    // Initialize USB dongle
    QTimer::singleShot(delay_ms*msgCount, this, [this](){
      constexpr uint8_t data[] = {HIDPP::Bytes::SHORT_MSG, HIDPP::Bytes::MSG_TO_USB_RECEIVER, HIDPP::Bytes::SHORT_SET_FEATURE, 0x02, 0x02, 0x00, 0x00};
      sendData(data, sizeof(data));});
    msgCount++;

    // Now enable both software and wireless notification bit
    QTimer::singleShot(delay_ms*msgCount, this, [this](){
      constexpr uint8_t data[] = {HIDPP::Bytes::SHORT_MSG, HIDPP::Bytes::MSG_TO_USB_RECEIVER, HIDPP::Bytes::SHORT_SET_FEATURE, 0x00, 0x00, 0x09, 0x00};
      sendData(data, sizeof(data));});
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
    if (m_featureSet->supportFeatureCode(FeatureCode::ReprogramControlsV4)) {
      auto& reservedInputs = m_inputMapper->getReservedInputs();
      reservedInputs.clear();
      featureFlags |= DeviceFlags::NextHold;
      featureFlags |= DeviceFlags::BackHold;
      reservedInputs.emplace_back(ReservedKeyEventSequence::NextHoldInfo);
      reservedInputs.emplace_back(ReservedKeyEventSequence::BackHoldInfo);
      logDebug(hid) << "SubDevice" << path() << "can send next and back hold event.";
    }
  } else {
    logWarn(hid) << "Loading FeatureSet for" << path() << "failed. Device might be inactive.";
    logInfo(hid) << "Press any button on device to activate it.";
  }
  setFlags(featureFlags, true);
  setReadNotifierEnabled(true);

  // Reset spotlight device
  if (m_featureSet->getFeatureCount()) {
    const auto resetID = m_featureSet->getFeatureID(FeatureCode::Reset);
    if (resetID) {
      QTimer::singleShot(delay_ms*msgCount, this, [this, resetID](){
        const uint8_t data[] = {HIDPP::Bytes::SHORT_MSG, HIDPP::Bytes::MSG_TO_SPOTLIGHT, resetID, 0x1d, 0x00, 0x00, 0x00};
        sendData(data, sizeof(data));});
      msgCount++;
    }
  }
  // Device Resetting complete -------------------------------------------------

  if (m_details.busType == BusType::Usb) {
    // Ping spotlight device for checking if is online
    // the response will have the version for HID++ protocol.
    QTimer::singleShot(delay_ms*msgCount, this, [this](){pingSubDevice();});
    msgCount++;
  } else if (m_details.busType == BusType::Bluetooth) {
    // Bluetooth connection do not respond to ping.
    // Hence, we are faking a ping response here.
    // Bluetooth connection mean HID++ v2.0+.
    // Setting version to 6.4: same as USB connection.
    setHIDppProtocol(6.4);
    emit receivedPingResponse();
  }

  // Enable Next and back button on hold functionality.
  const auto rcID = m_featureSet->getFeatureID(FeatureCode::ReprogramControlsV4);
  if (rcID) {
    if (hasFlags(DeviceFlags::NextHold)) {
      QTimer::singleShot(delay_ms*msgCount, this, [this, rcID](){
        const uint8_t data[] = {HIDPP::Bytes::SHORT_MSG, HIDPP::Bytes::MSG_TO_SPOTLIGHT, rcID, 0x3d, 0x00, 0xda, 0x33};
        sendData(data, sizeof(data));});
      msgCount++;
    }

    if (hasFlags(DeviceFlags::BackHold)) {
      QTimer::singleShot(delay_ms*msgCount, this, [this, rcID](){
        const uint8_t data[] = {HIDPP::Bytes::SHORT_MSG, HIDPP::Bytes::MSG_TO_SPOTLIGHT, rcID, 0x3d, 0x00, 0xdc, 0x33};
        sendData(data, sizeof(data));});
      msgCount++;
    }
  }
}

// -------------------------------------------------------------------------------------------------
ssize_t SubHidrawConnection::sendData(const QByteArray& msg)
{
  constexpr ssize_t errorResult = -1;

  if (mode() != ConnectionMode::ReadWrite || !m_writeNotifier) { return errorResult; }

  const auto notifier = socketWriteNotifier();
  const auto res = ::write(notifier->socket(), msg.data(), msg.length());

  if (res == msg.length()) {
    logDebug(hid) << res << "bytes written to" << path() << "(" << msg.toHex() << ")";
  } else {
    logWarn(hid) << "Writing to" << path() << "failed.";
  }

  return res;
}

// -------------------------------------------------------------------------------------------------
ssize_t SubHidrawConnection::sendData(const void* msg, size_t msgLen)
{
  const QByteArray msgArr(reinterpret_cast<const char*>(msg), msgLen);
  return sendData(msgArr);
}

// -------------------------------------------------------------------------------------------------
void SubHidrawConnection::createSocketNotifiers(int fd)
{
  fcntl(fd, F_SETFL, fcntl(fd, F_GETFL, 0) | O_NONBLOCK);
  if ((fcntl(fd, F_GETFL, 0) & O_NONBLOCK) == O_NONBLOCK) {
    m_details.deviceFlags |= DeviceFlag::NonBlocking;
  }

  // Create read and write socket notifiers
  m_readNotifier = std::make_unique<QSocketNotifier>(fd, QSocketNotifier::Read);
  QSocketNotifier *const readNotifier = m_readNotifier.get();
  // Auto clean up and close descriptor on destruction of notifier
  connect(readNotifier, &QSocketNotifier::destroyed, [readNotifier]() {
    ::close(static_cast<int>(readNotifier->socket()));
  });

  m_writeNotifier = std::make_unique<QSocketNotifier>(fd, QSocketNotifier::Write);
  QSocketNotifier *const writeNotifier = m_writeNotifier.get();
  writeNotifier->setEnabled(false); // Disable write notifier by default
  // Auto clean up and close descriptor on destruction of notifier
  connect(writeNotifier, &QSocketNotifier::destroyed, [writeNotifier]() {
    ::close(static_cast<int>(writeNotifier->socket()));
  });
}

// -------------------------------------------------------------------------------------------------
SubHidppConnection::SubHidppConnection(SubHidrawConnection::Token token,
                                       const DeviceScan::SubDevice &sd, const DeviceId &id)
  : SubHidrawConnection(token, sd, id), m_featureSet(std::make_unique<HIDPP::FeatureSet>()) {}

// -------------------------------------------------------------------------------------------------
SubHidppConnection::~SubHidppConnection() = default;

// -------------------------------------------------------------------------------------------------
    ssize_t SubHidppConnection::sendData(const QByteArray &msg)
{
  constexpr ssize_t errorResult = -1;

  if (!HIDPP::isValidMessage(msg)) { return errorResult; }

  // If the message have 0xff as second byte, it is meant for USB dongle hence,
  // should not be send when device is connected on bluetooth.
  //
  // Logitech Spotlight (USB) can receive data in two different length.
  //   1. Short (7 byte long starting with 0x10)
  //   2. Long (20 byte long starting with 0x11)
  // However, bluetooth connection only accepts data in long (20 byte) packets.
  // For converting standard short length data to long length data, change the first byte to 0x11 and
  // pad the end of message with 0x00 to acheive the length of 20.

  if (m_details.busType == BusType::Bluetooth)
  {
    if (HIDPP::isMessageForUsb(msg))
    {
      logWarn(hid) << "Invalid packet" << msg.toHex() << "for spotlight connected on bluetooth.";
      return errorResult;
    }

    // For bluetooth always convert to a long message if we have a short message
    if (HIDPP::isValidShortMessage(msg)) {
      return SubHidrawConnection::sendData(HIDPP::shortToLongMsg(msg));
    }
  }

  return SubHidrawConnection::sendData(msg);
}

// -------------------------------------------------------------------------------------------------
std::shared_ptr<SubHidppConnection> SubHidppConnection::create(const DeviceScan::SubDevice &sd,
                                                                const DeviceConnection &dc)
{
  const int devfd = openHidrawSubDevice(sd, dc.deviceId());
  if (devfd == -1) return std::shared_ptr<SubHidppConnection>();

  auto connection = std::make_shared<SubHidppConnection>(Token{}, sd, dc.deviceId());

  // TODO feature set needs to be a member of a sub hidraw connection
  // connection->m_featureSet = dc.getFeatureSet();
  connection->m_featureSet->setHIDDeviceFileDescriptor(devfd);
  // ---

  if (dc.hasHidppSupport()) {
    connection->m_details.deviceFlags |= DeviceFlag::Hidpp;
  }

  connection->createSocketNotifiers(devfd);
  connection->m_inputMapper = dc.inputMapper();
  connection->postTask([c=&*connection](){ c->initialize(); });
  return connection;
}

// -------------------------------------------------------------------------------------------------
void SubHidppConnection::sendVibrateCommand(uint8_t intensity, uint8_t length)
{
  // TODO put in HIDPP
  // TODO generalize features and protocol for proprietary device features like vibration
  //      for not only the Spotlight device.
  //
  // Spotlight:
  //                                        present
  //                                        controlID   len         intensity
  // unsigned char vibrate[] = {0x10, 0x01, 0x09, 0x1d, 0x00, 0xe8, 0x80};

  const uint8_t pcID = getFeatureSet()->getFeatureID(FeatureCode::PresenterControl);
  if (pcID == 0x00) return;
  const uint8_t vibrateCmd[] = {HIDPP::Bytes::SHORT_MSG, HIDPP::Bytes::MSG_TO_SPOTLIGHT, pcID, 0x1d, length, 0xe8, intensity};
  sendData(vibrateCmd, sizeof(vibrateCmd));
}

// -------------------------------------------------------------------------------------------------
void SubHidppConnection::queryBatteryStatus()
{
  // TODO put parts in HIDPP
  if (hasFlags(DeviceFlag::ReportBattery))
  {
    const uint8_t batteryFeatureID = m_featureSet->getFeatureID(FeatureCode::BatteryStatus);
    if (batteryFeatureID)
    {
      const uint8_t batteryCmd[] = {HIDPP::Bytes::SHORT_MSG, HIDPP::Bytes::MSG_TO_SPOTLIGHT, batteryFeatureID, 0x0d, 0x00, 0x00, 0x00};
      sendData(batteryCmd, sizeof(batteryCmd));
    }
    setWriteNotifierEnabled(false);
  }
}

// -------------------------------------------------------------------------------------------------
const HIDPP::FeatureSet* SubHidppConnection::getFeatureSet()
{
  return &*m_featureSet;
}
