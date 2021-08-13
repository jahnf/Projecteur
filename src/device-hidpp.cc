// This file is part of Projecteur - https://github.com/jahnf/projecteur
// - See LICENSE.md and README.md

#include "device-hidpp.h"
#include "logging.h"

#include <unistd.h>

#include "deviceinput.h"

#include <QSocketNotifier>
#include <QTimer>

DECLARE_LOGGING_CATEGORY(hid)

// -------------------------------------------------------------------------------------------------
SubHidppConnection::SubHidppConnection(SubHidrawConnection::Token token,
                                       const DeviceScan::SubDevice& sd, const DeviceId& id)
  : SubHidrawConnection(token, sd)
  , m_featureSet(this)
  , m_busType(id.busType)
  , m_requestCleanupTimer(new QTimer(this))
{
  m_requestCleanupTimer->setInterval(500);
  m_requestCleanupTimer->setSingleShot(false);
  connect(m_requestCleanupTimer, &QTimer::timeout, this, &SubHidppConnection::clearTimedOutRequests);
}

// -------------------------------------------------------------------------------------------------
SubHidppConnection::~SubHidppConnection() = default;

// -------------------------------------------------------------------------------------------------
ssize_t SubHidppConnection::sendData(std::vector<uint8_t> data) {
  return sendData(HIDPP::Message(std::move(data)));
}

// -------------------------------------------------------------------------------------------------
ssize_t SubHidppConnection::sendData(HIDPP::Message msg) //
{
  constexpr ssize_t errorResult = -1;
  if (!msg.isValid()) {
    return errorResult;
  }

  // If the message have 0xff as second byte, it is meant for USB dongle hence,
  // should not be send when device is connected on bluetooth.
  //
  // Logitech Spotlight (USB) can receive data in two different length.
  //   1. Short (7 byte long starting with 0x10)
  //   2. Long (20 byte long starting with 0x11)
  // However, bluetooth connection only accepts data in long (20 byte) packets.
  // For converting standard short length data to long length data, change the first byte to 0x11
  // and pad the end of message with 0x00 to acheive the length of 20.

  if (m_busType == BusType::Bluetooth)
  {
    if (msg.deviceIndex() == HIDPP::DeviceIndex::DefaultDevice) {
      logWarn(hid) << "Invalid packet" << msg.hex() << "for spotlight connected on bluetooth.";
      return errorResult;
    }

    // For bluetooth always convert to a long message if we have a short message
    msg.convertToLong();
  }

  return SubHidrawConnection::sendData(msg.data(), msg.size());
}


// -------------------------------------------------------------------------------------------------
void SubHidppConnection::sendData(std::vector<uint8_t> data, SendResultCallback resultCb) {
  sendData(HIDPP::Message(std::move(data)), std::move(resultCb));
}

// -------------------------------------------------------------------------------------------------
void SubHidppConnection::sendData(HIDPP::Message msg, SendResultCallback resultCb) {
  postSelf([this, msg = std::move(msg), cb = std::move(resultCb)]() mutable {
    // Check for valid message format
    if (!msg.isValid()) {
      if (cb) cb(MsgResult::InvalidFormat);
      return;
    }

    if (m_busType == BusType::Bluetooth) {
      // For bluetooth always convert to a long message if we have a short message
      msg.convertToLong();
    }

    const auto result = SubHidrawConnection::sendData(msg.data(), msg.size());
    if (cb) {
      const bool success = (result >= 0 && static_cast<size_t>(result) == msg.size());
      cb(success ? MsgResult::Ok : MsgResult::WriteError);
    }
  });
}

// -------------------------------------------------------------------------------------------------
void SubHidppConnection::sendRequest(std::vector<uint8_t> data, RequestResultCallback responseCb) {
  sendRequest(HIDPP::Message(std::move(data)), std::move(responseCb));
}

// -------------------------------------------------------------------------------------------------
void SubHidppConnection::sendRequest(HIDPP::Message msg, RequestResultCallback responseCb)
{
  postSelf([this, msg = std::move(msg), cb = std::move(responseCb)]() mutable
  {
    // Check for valid message format
    if (!msg.isValid()) {
      if (cb) cb(MsgResult::InvalidFormat, HIDPP::Message());
      return;
    }

    if (m_busType == BusType::Bluetooth) {
      // For bluetooth always convert to a long message if we have a short message
      msg.convertToLong();
    }

    // TODO: more early sanity checks?? device index in a valid range???

    sendData(msg, [this, msg](MsgResult result) {
      if (result == MsgResult::Ok) return;

      // error result, find msg in request list
      auto it = std::find_if(m_requests.begin(), m_requests.end(),
                             [&msg](const RequestEntry& entry) { return entry.request == msg; });

      if (it == m_requests.end()) {
        // TODO log warning send error for message without matching request
        return;
      }

      if (it->callBack) {
        it->callBack(result, HIDPP::Message());
      }
      m_requests.erase(it);
    });

    // Place request in request list with a timeout
    m_requests.emplace_back(RequestEntry{
      std::move(msg), std::chrono::steady_clock::now() + std::chrono::milliseconds{4000},
      std::move(cb)});

    // Run cleanup timer if not already active
    if (!m_requestCleanupTimer->isActive()) m_requestCleanupTimer->start();
  });
}

// -------------------------------------------------------------------------------------------------
void SubHidppConnection::sendDataBatch(DataBatch dataBatch, DataBatchResultCallback cb,
                                       bool continueOnError) {
  std::vector<MsgResult> results;
  results.reserve(dataBatch.size());
  sendDataBatch(std::move(dataBatch), std::move(cb), continueOnError, std::move(results));
}

// -------------------------------------------------------------------------------------------------
void SubHidppConnection::sendDataBatch(DataBatch dataBatch, DataBatchResultCallback cb,
                                       bool continueOnError, std::vector<MsgResult> results)
{
  postSelf([this, batch = std::move(dataBatch), batchCb = std::move(cb), continueOnError,
            results = std::move(results), coe = continueOnError]() mutable
  {
    if (batch.empty()) {
      if (batchCb) batchCb(std::move(results));
      return;
    }

    // Get item from queue and pop
    DataBatchItem queueItem(std::move(batch.front()));
    batch.pop();

    // Process queue item
    sendData(std::move(queueItem.message), makeSafeCallback(
    [this, batch = std::move(batch), results = std::move(results), coe,
     batchCb = std::move(batchCb), resultCb = std::move(queueItem.callback)]
    (MsgResult result) mutable
    {
      // Add result to results vector
      results.push_back(result);
      // If a result callback is set invoke it
      if (resultCb) resultCb(result);

      // If batch is empty or we got an error result and don't want to continue on
      // error (coe)
      if (batch.empty() || (result != MsgResult::Ok && !coe)) {
        if (batchCb) batchCb(std::move(results));
        return;
      }

      // continue processing the rest of the batch
      sendDataBatch(std::move(batch), std::move(batchCb), coe, std::move(results));
    }));
  });
}

// -------------------------------------------------------------------------------------------------
void SubHidppConnection::sendRequestBatch(RequestBatch requestBatch, RequestBatchResultCallback cb,
                                          bool continueOnError) {
  std::vector<MsgResult> results;
  results.reserve(requestBatch.size());
  sendRequestBatch(std::move(requestBatch), std::move(cb), continueOnError, std::move(results));
}

// -------------------------------------------------------------------------------------------------
void SubHidppConnection::sendRequestBatch(RequestBatch requestBatch, RequestBatchResultCallback cb,
                                          bool continueOnError, std::vector<MsgResult> results)
{
  postSelf([this, batch = std::move(requestBatch), batchCb = std::move(cb), continueOnError,
            results = std::move(results), coe = continueOnError]() mutable
  {
    if (batch.empty()) {
      if (batchCb) batchCb(std::move(results));
      return;
    }

    // Get item from queue and pop
    RequestBatchItem queueItem(std::move(batch.front()));
    batch.pop();

    // Process queue item
    sendRequest(std::move(queueItem.message), makeSafeCallback(
    [this, batch = std::move(batch), results = std::move(results), coe,
     batchCb = std::move(batchCb), resultCb = std::move(queueItem.callback)]
    (MsgResult result, HIDPP::Message replyMessage) mutable
    {
      // Add result to results vector
      results.push_back(result);
      // If a result callback is set invoke it
      if (resultCb) resultCb(result, std::move(replyMessage));

      // If batch is empty or we got an error result and don't want to continue on
      // error (coe)
      if (batch.empty() || (result != MsgResult::Ok && !coe)) {
        if (batchCb) batchCb(std::move(results));
        return;
      }

      // continue processing the rest of the batch
      sendRequestBatch(std::move(batch), std::move(batchCb), coe, std::move(results));
    }));
  });
}

// -------------------------------------------------------------------------------------------------
std::shared_ptr<SubHidppConnection> SubHidppConnection::create(const DeviceScan::SubDevice& sd,
                                                               const DeviceConnection& dc) {
  const int devfd = openHidrawSubDevice(sd, dc.deviceId());
  if (devfd == -1) return std::shared_ptr<SubHidppConnection>();

  auto connection = std::make_shared<SubHidppConnection>(Token{}, sd, dc.deviceId());
  if (dc.hasHidppSupport()) connection->m_details.deviceFlags |= DeviceFlag::Hidpp;

  connection->createSocketNotifiers(devfd);
  connection->m_inputMapper = dc.inputMapper();

  connect(connection->socketReadNotifier(), &QSocketNotifier::activated, &*connection,
          &SubHidppConnection::onHidppDataAvailable);

  connection->postTask([c = &*connection]() { c->initialize(); });
  return connection;
}

// -------------------------------------------------------------------------------------------------
void SubHidppConnection::sendVibrateCommand(uint8_t intensity, uint8_t length) {
  if (!hasFlags(DeviceFlags::Vibrate)) return;

  // TODO put in HIDPP
  // TODO generalize features and protocol for proprietary device features like vibration
  //      for not only the Spotlight device.
  //
  // Spotlight:
  //                                        present
  //                                        controlID   len         intensity
  // unsigned char vibrate[] = {0x10, 0x01, 0x09, 0x1d, 0x00, 0xe8, 0x80};

  length = length > 10 ? 10 : length; // length should be between 0 to 10.
  const uint8_t pcIndex = m_featureSet.getFeatureIndex(HIDPP::FeatureCode::PresenterControl);
  using namespace HIDPP;
  // const uint8_t vibrateCmd[] = {HIDPP::Bytes::SHORT_MSG,
  //                               HIDPP::Bytes::MSG_TO_SPOTLIGHT,
  //                               pcIndex,
  //                               m_featureSet.getRandomFunctionCode(0x10),
  //                               length,
  //                               0xe8,
  //                               intensity};
  Message vibrateMsg(Message::Type::Long, DeviceIndex::WirelessDevice1, pcIndex, 1, {
    length, 0xe8, intensity
  });
  if (pcIndex) sendData(std::move(vibrateMsg));
}

// -------------------------------------------------------------------------------------------------
void SubHidppConnection::queryBatteryStatus()
{
  // TODO put parts in HIDPP
  // if (hasFlags(DeviceFlag::ReportBattery)) {
  //   const uint8_t batteryFeatureIndex =
  //     m_featureSet.getFeatureIndex(HIDPP::FeatureCode::BatteryStatus);
  //   if (batteryFeatureIndex) {
  //     const uint8_t batteryCmd[] = {HIDPP::Bytes::SHORT_MSG,
  //                                   HIDPP::Bytes::MSG_TO_SPOTLIGHT,
  //                                   batteryFeatureIndex,
  //                                   m_featureSet.getRandomFunctionCode(0x00),
  //                                   0x00,
  //                                   0x00,
  //                                   0x00};
  //     sendData(batteryCmd, sizeof(batteryCmd));
  //   }
  // }
}

// -------------------------------------------------------------------------------------------------
void SubHidppConnection::setPointerSpeed(uint8_t
//level
)
{
  const uint8_t psIndex = m_featureSet.getFeatureIndex(HIDPP::FeatureCode::PointerSpeed);
  if (psIndex == 0x00) return;

  // level = (level > 0x09) ? 0x09 : level; // level should be in range of 0-9
  // uint8_t pointerSpeed = 0x10 & level; // pointer speed sent to device are between 0x10 - 0x19 (hence ten speed levels)
  // const uint8_t pointerSpeedCmd[] = {HIDPP::Bytes::SHORT_MSG,
  //                                    HIDPP::Bytes::MSG_TO_SPOTLIGHT,
  //                                    psIndex,
  //                                    m_featureSet.getRandomFunctionCode(0x10),
  //                                    pointerSpeed,
  //                                    0x00,
  //                                    0x00};
  // sendData(pointerSpeedCmd, sizeof(pointerSpeedCmd));
}

// -------------------------------------------------------------------------------------------------

void SubHidppConnection::initUsbReceiver(std::function<void(MsgResult)> cb)
{
  if (m_busType != BusType::Usb)
  {
    if (cb) cb(MsgResult::Ok);
    return;
  }

  using namespace HIDPP;
  using Type = HIDPP::Message::Type;
  RequestBatch batch{{
    RequestBatchItem{
      // Reset device: get rid of any device configuration by other programs
      Message(Type::Short, DeviceIndex::DefaultDevice, Commands::GetRegister, 0, 0, {}),
      makeSafeCallback([](MsgResult result, HIDPP::Message /* msg */) {
        if (result == MsgResult::Ok) return;
        logWarn(hid) << tr("Usb Receiver init failure - %1").arg(toString(result));
      })
    },
    RequestBatchItem{
      // Turn off software bit and keep the wireless notification bit on
      Message(Type::Short, DeviceIndex::DefaultDevice, Commands::SetRegister, 0, 0, {0x00, 0x01, 0x00}),
      makeSafeCallback([](MsgResult result, HIDPP::Message /* msg */) {
        if (result == MsgResult::Ok) return;
        logWarn(hid) << tr("Usb Receiver init failure - %1").arg(toString(result));
      })
    },
    RequestBatchItem{
      // Initialize USB dongle
      Message(Type::Short, DeviceIndex::DefaultDevice, Commands::GetRegister, 0, 2, {}),
      makeSafeCallback([](MsgResult result, HIDPP::Message /* msg */) {
        if (result == MsgResult::Ok) return;
        logWarn(hid) << tr("Usb Receiver init failure - %1").arg(toString(result));
      })
    },
    RequestBatchItem{
      // ---
      Message(Type::Short, DeviceIndex::DefaultDevice, Commands::SetRegister, 0, 2, {0x02, 0x00, 0x00}),
      makeSafeCallback([](MsgResult result, HIDPP::Message /* msg */) {
        if (result == MsgResult::Ok) return;
        logWarn(hid) << tr("Usb Receiver init failure - %1").arg(toString(result));
      })
    },
    RequestBatchItem{
      // Now enable both software and wireless notification bit
      Message(Type::Short, DeviceIndex::DefaultDevice, Commands::SetRegister, 0, 0, {0x00, 0x09, 0x00}),
      makeSafeCallback([](MsgResult result, HIDPP::Message /* msg */) {
        if (result == MsgResult::Ok) return;
        logWarn(hid) << tr("Usb Receiver init failure - %1").arg(toString(result));
      })
    },
  }};

  sendRequestBatch(std::move(batch), [cb=std::move(cb)](std::vector<MsgResult> results) {
      if (cb) cb(results.back());
  });
}

// -------------------------------------------------------------------------------------------------
void SubHidppConnection::initialize()
{
  if (!hasFlags(DeviceFlag::Hidpp)) return;

  // TODO set state: not_initialized, initializing, initialized for the subdevice

  initUsbReceiver(makeSafeCallback([this](MsgResult res)
  {
    if (res != MsgResult::Ok) {
      // TODO log error - re-schedule init?
    }
    m_featureSet.initFromDevice();
  }));

  // DeviceFlags featureFlags = DeviceFlag::NoFlags;
  // Read HID++ FeatureSet (Feature ID and Feature Code pairs) from logitech device
  // setNotifiersEnabled(false);
  // setReadNotifierEnabled(false); // TODO remove ... implement populatefeaturetable via async..
  // if (m_featureSet.getFeatureCount() == 0) m_featureSet.populateFeatureTable();
  // if (m_featureSet.getFeatureCount()) {
  //   logDebug(hid) << "Loaded" << m_featureSet.getFeatureCount() << "features for" << path();
  //   if (m_featureSet.supportFeatureCode(HIDPP::FeatureCode::PresenterControl)) {
  //     featureFlags |= DeviceFlag::Vibrate;
  //     logDebug(hid) << "SubDevice" << path() << "reported Vibration capabilities.";
  //   }
  //   if (m_featureSet.supportFeatureCode(HIDPP::FeatureCode::BatteryStatus)) {
  //     featureFlags |= DeviceFlag::ReportBattery;
  //     logDebug(hid) << "SubDevice" << path() << "can communicate battery information.";
  //   }
  //   if (m_featureSet.supportFeatureCode(HIDPP::FeatureCode::ReprogramControlsV4)) {
  //     auto& reservedInputs = m_inputMapper->getReservedInputs();
  //     reservedInputs.clear();
  //     featureFlags |= DeviceFlags::NextHold;
  //     featureFlags |= DeviceFlags::BackHold;
  //     reservedInputs.emplace_back(ReservedKeyEventSequence::NextHoldInfo);
  //     reservedInputs.emplace_back(ReservedKeyEventSequence::BackHoldInfo);
  //     logDebug(hid) << "SubDevice" << path() << "can send next and back hold event.";
  //   }
  //   if (m_featureSet.supportFeatureCode(HIDPP::FeatureCode::PointerSpeed)) {
  //     featureFlags |= DeviceFlags::PointerSpeed;
  //   }
  // }
  // else {
  //   logWarn(hid) << "Loading FeatureSet for" << path() << "failed.";
  //   logInfo(hid) << "Device might be inactive. Press any button on device to activate it.";
  // }
  // setFlags(featureFlags, true);
  // setReadNotifierEnabled(true);

  // TODO: implement
  // // Reset spotlight device
  // if (m_featureSet.getFeatureCount()) {
  //   const auto resetIndex = m_featureSet.getFeatureIndex(FeatureCode::Reset);
  //   if (resetIndex) {
  //     QTimer::singleShot(delay_ms*msgCount, this, [this, resetIndex](){
  //       const uint8_t data[] = {HIDPP::Bytes::SHORT_MSG, HIDPP::Bytes::MSG_TO_SPOTLIGHT,
  //       resetIndex, m_featureSet.getRandomFunctionCode(0x10), 0x00, 0x00, 0x00}; sendData(data,
  //       sizeof(data));});
  //     msgCount++;
  //   }
  // }
  // Device Resetting complete -------------------------------------------------

  // TODO: implement
  // if (m_busType == BusType::Usb) {
  //   // Ping spotlight device for checking if is online
  //   // the response will have the version for HID++ protocol.
  //   QTimer::singleShot(delay_ms*msgCount, this, [this](){pingSubDevice();});
  //   msgCount++;
  // } else if (m_busType == BusType::Bluetooth) {
  //   // Bluetooth connection do not respond to ping.
  //   // Hence, we are faking a ping response here.
  //   // Bluetooth connection mean HID++ v2.0+.
  //   // Setting version to 6.4: same as USB connection.
  //   setHIDppProtocol(6.4);
  // }

  // TODO implement
  // Enable Next and back button on hold functionality.
  const auto rcIndex = m_featureSet.getFeatureIndex(HIDPP::FeatureCode::ReprogramControlsV4);
  if (rcIndex) {
    //   if (hasFlags(DeviceFlags::NextHold)) {
    //     QTimer::singleShot(delay_ms*msgCount, this, [this, rcIndex](){
    //       const uint8_t data[] = {HIDPP::Bytes::LONG_MSG, HIDPP::Bytes::MSG_TO_SPOTLIGHT,
    //       rcIndex, m_featureSet.getRandomFunctionCode(0x30), 0x00, 0xda, 0x33,
    //                              0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    //                              0x00, 0x00, 0x00};
    //       sendData(data, sizeof(data));});
    //     msgCount++;
    //   }

    //   if (hasFlags(DeviceFlags::BackHold)) {
    //     QTimer::singleShot(delay_ms*7777777msgCount, this, [this, rcIndex](){
    //       const uint8_t data[] = {HIDPP::Bytes::LONG_MSG, HIDPP::Bytes::MSG_TO_SPOTLIGHT,
    //       rcIndex, m_featureSet.getRandomFunctionCode(0x30), 0x00, 0xdc, 0x33,
    //                              0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    //                              0x00, 0x00, 0x00};
    //       sendData(data, sizeof(data));});
    //     msgCount++;
    //   }
  }

  // Reset pointer speed to default level of 0x04 (5th level)
  if (hasFlags(DeviceFlags::PointerSpeed)) setPointerSpeed(0x04);


  // m_featureSet.initFromDevice();
}

// -------------------------------------------------------------------------------------------------
void SubHidppConnection::pingSubDevice() {
  // constexpr uint8_t rootIndex = 0x00; // root Index is always 0x00 in any logitech device
  // const uint8_t pingCmd[]     = {HIDPP::Bytes::SHORT_MSG,
  //                            HIDPP::Bytes::MSG_TO_SPOTLIGHT,
  //                            rootIndex,
  //                            m_featureSet.getRandomFunctionCode(0x10),
  //                            0x00,
  //                            0x00,
  //                            0x5d};
  // sendData(pingCmd, sizeof(pingCmd));
}

// // -------------------------------------------------------------------------------------------------
// void SubHidppConnection::setHIDppProtocol(float version) {
//   // Inform user about the online status of device.
//   if (version > 0) {
//     if (HIDppProtocolVer < 0) {
//       logDebug(hid) << "HID++ Device with path" << path() << "is now active.";
//       logDebug(hid) << "HID++ protocol version" << tr("%1.").arg(version);
//       emit activated();
//     }
//   }
//   else {
//     if (HIDppProtocolVer > 0) {
//       logDebug(hid) << "HID++ Device with path" << path() << "got deactivated.";
//       emit deactivated();
//     }
//   }
//   HIDppProtocolVer = version;
// }

// -------------------------------------------------------------------------------------------------
void SubHidppConnection::onHidppDataAvailable(int fd)
{
  HIDPP::Message msg(std::vector<uint8_t>(20));
  const auto res = ::read(fd, msg.data(), msg.dataSize());
  if (res < 0) {
    if (errno != EAGAIN) {
      emit socketReadError(errno);
    }
    return;
  }

  if (!msg.isValid()) {
    logDebug(hid) << tr("Received invalid HID++ message "
                        "'%1' from %2").arg(qPrintable(msg.hex()), path());
    return;
  }

  if (msg.isError()) {
    // Find first matching request for the incoming error reply
    const auto it =
      std::find_if(m_requests.begin(), m_requests.end(), [&msg](const RequestEntry& requestEntry) {
        return msg.isErrorResponseTo(requestEntry.request);
      });

    if (it != m_requests.end())
    {
      logDebug(hid) << tr("Received hiddpp error with code = %1 on")
                       .arg(to_integral(msg.errorCode())) << path() << "(" << msg.hex() << ")";
      if (it->callBack) {
        it->callBack(MsgResult::HidppError, std::move(msg));
      }
      m_requests.erase(it);
    }
    else {
      logWarn(hid) << tr("Received error hidpp message '%1' "
                         "without matching request.").arg(qPrintable(msg.hex()));
    }
    return;
  }

  // Find first matching request for the incoming reply
  const auto it =
    std::find_if(m_requests.begin(), m_requests.end(), [&msg](const RequestEntry& requestEntry) {
      return msg.isResponseTo(requestEntry.request);
    });

  if (it != m_requests.end())
  {
    // Found matching request
    logDebug(hid) << tr("Received %1 bytes on").arg(msg.size()) << path()
                  << "(" << msg.hex() << ")";
    if (it->callBack) {
      it->callBack(MsgResult::Ok, std::move(msg));
    }
    m_requests.erase(it);
  }
  else {
    // TODO check for device event messages, that don't require a request
    logWarn(hid) << tr("Received hidpp message "
                       "'%1' without matching request.").arg(qPrintable(msg.hex()));
  }
}

// -------------------------------------------------------------------------------------------------
void SubHidppConnection::clearTimedOutRequests() {
  const auto now = std::chrono::steady_clock::now();
  m_requests.remove_if([&now](const RequestEntry& entry) {
    if (now <= entry.validUntil) {
      return false;
    }
    if (entry.callBack) {
      entry.callBack(MsgResult::Timeout, HIDPP::Message());
    }
    return true;
  });

  if (m_requests.empty()) {
    m_requestCleanupTimer->stop();
  }
}
