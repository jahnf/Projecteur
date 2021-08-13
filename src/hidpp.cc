// This file is part of Projecteur - https://github.com/jahnf/projecteur - See LICENSE.md and README.md
#include "hidpp.h"
#include "logging.h"
#include "enum-helper.h"

#include <unistd.h>

#include <random>


DECLARE_LOGGING_CATEGORY(hid)

#define STRINGIFY(x) #x
#define ENUM_CASE_STRINGIFY(x) case x: return STRINGIFY(x)

// -------------------------------------------------------------------------------------------------
namespace {

  namespace Defaults {
    constexpr uint8_t HidppSoftwareId = 7;
  }

  // -- HID++ message offsets
  namespace Offset {
    constexpr uint32_t Type = 0;
    constexpr uint32_t DeviceIndex = 1;
    constexpr uint32_t SubId = 2;
    constexpr uint32_t FeatureIndex = SubId;
    constexpr uint32_t Address = 3;

    constexpr uint32_t ErrorSubId = 3;
    constexpr uint32_t ErrorFeatureIndex = ErrorSubId;
    constexpr uint32_t ErrorAddress = 4;
    constexpr uint32_t ErrorCode = 5;
  }

  namespace Defines {
    constexpr uint8_t ErrorShort = 0x8f;
    constexpr uint8_t ErrorLong = 0xff;
  }

  uint8_t funcSwIdToByte(uint8_t function, uint8_t swId) {
    return (swId & 0x0f)|((function & 0x0f) << 4);
  }

  uint8_t getRandomByte()
  {
    static std::mt19937 gen(std::random_device{}());
    std::uniform_int_distribution<uint8_t> distribution;
    return distribution(gen);
  }

  HIDPP::Message::Data getRandomPingPayload() {
    return {0, 0, getRandomByte()};
  }

}  // end anonymous namespace

const char* HidppConnectionInterface::toString(MsgResult res)
{
  switch(res) {
    ENUM_CASE_STRINGIFY(MsgResult::Ok);
    ENUM_CASE_STRINGIFY(MsgResult::InvalidFormat);
    ENUM_CASE_STRINGIFY(MsgResult::WriteError);
    ENUM_CASE_STRINGIFY(MsgResult::Timeout);
    ENUM_CASE_STRINGIFY(MsgResult::HidppError);
  }
  return "MsgResult::(unknown)";
}

namespace HIDPP {
// -------------------------------------------------------------------------------------------------

const char* toString(Error e)
{
  switch(e) {
    ENUM_CASE_STRINGIFY(Error::NoError);
    ENUM_CASE_STRINGIFY(Error::Unknown);
    ENUM_CASE_STRINGIFY(Error::InvalidArgument);
    ENUM_CASE_STRINGIFY(Error::OutOfRange);
    ENUM_CASE_STRINGIFY(Error::HWError);
    ENUM_CASE_STRINGIFY(Error::LogitechInternal);
    ENUM_CASE_STRINGIFY(Error::InvalidFeatureIndex);
    ENUM_CASE_STRINGIFY(Error::InvalidFunctionId);
    ENUM_CASE_STRINGIFY(Error::Busy);
    ENUM_CASE_STRINGIFY(Error::Unsupported);
  }
  return "Error::(unknown)";
}

// -------------------------------------------------------------------------------------------------

Message::Message() = default;

Message::Message(Type type)
  : Message(type, DeviceIndex::DefaultDevice, 0, 0, Defaults::HidppSoftwareId, {})
{}

Message::Message(std::vector<uint8_t>&& data) : m_data(std::move(data)) {}

Message::Message(Type type, uint8_t deviceIndex, uint8_t featureIndex, uint8_t function,
                 uint8_t swId, Data payload)
  : Message(Data{to_integral(type), deviceIndex, featureIndex, funcSwIdToByte(function, swId)})
{
  if (type == Type::Invalid) return;

  m_data.reserve(m_data.size() + payload.size());
  std::move(payload.begin(), payload.end(), std::back_inserter(m_data));

  if (type == Type::Long) m_data.resize(20, 0x0);
  else if (type == Type::Short) m_data.resize(7, 0x0);
}

Message::Message(Type type, uint8_t deviceIndex, uint8_t featureIndex, uint8_t function,
                 Data payload)
  : Message(type, deviceIndex, featureIndex, function, Defaults::HidppSoftwareId, std::move(payload))
{}

size_t Message::size() const
{
  if (isLong()) return 20;
  if (isShort()) return 7;
  return 0;
}

Message::Type Message::type() const
{
  if (isLong()) return Type::Long;
  if (isShort()) return Type::Short;
  return Type::Invalid;
}

bool Message::isValid() const { return isLong() || isShort(); }

bool Message::isShort() const {
  return (m_data.size() >= 7 && m_data[Offset::Type] == to_integral(Message::Type::Short));
}

bool Message::isLong() const {
  return (m_data.size() >= 20 && m_data[Offset::Type] == to_integral(Message::Type::Long));
}

bool Message::isError() const
{
  if (isShort() && m_data[Offset::SubId] == Defines::ErrorShort) {
    return true;
  }
  else if (isLong() && m_data[Offset::SubId] == Defines::ErrorLong) {
    return true;
  }
  return false;
}

uint8_t Message::errorSubId() const {
  return m_data[Offset::ErrorSubId];
}

uint8_t  Message::errorAddress() const {
  return m_data[Offset::ErrorAddress];
}

uint8_t  Message::errorFeatureIndex() const {
  return m_data[Offset::ErrorFeatureIndex];
}

uint8_t  Message::errorFunction() const {
  return ((m_data[Offset::ErrorAddress] & 0xf0) >> 4);
}

uint8_t  Message::errorSoftwareId() const {
  return (m_data[Offset::ErrorAddress] & 0x0f);
}

HIDPP::Error Message::errorCode() const {
  return to_enum<HIDPP::Error>(m_data[Offset::ErrorCode]);
}

uint8_t Message::deviceIndex() const {
  return m_data[Offset::DeviceIndex];
}

uint8_t Message::subId() const {
  return m_data[Offset::SubId];
}

uint8_t Message::address() const {
  return m_data[Offset::Address];
}

uint8_t Message::featureIndex() const {
  return m_data[Offset::Address];
}

uint8_t Message::function() const {
  return ((m_data[Offset::Address] & 0xf0) >> 4);
}

uint8_t Message::softwareId() const {
  return (m_data[Offset::Address] & 0x0f);
}

void Message::setSubId(uint8_t subId) {
  m_data[Offset::SubId] = subId;
}

void Message::setAddress(uint8_t address) {
  m_data[Offset::Address] = address;
}

void Message::setFeatureIndex(uint8_t featureIndex) {
  m_data[Offset::FeatureIndex] = featureIndex;
}

void Message::setFunction(uint8_t function) {
  m_data[Offset::Address] = ((function & 0x0f) << 4) | (m_data[Offset::Address] & 0x0f);
}

void Message::setSoftwareId(uint8_t softwareId) {
  m_data[Offset::Address] = (softwareId & 0x0f) | (m_data[Offset::Address] & 0xf0);
}

bool Message::isResponseTo(const Message& other) const
{
  if (!isValid() || !other.isValid()) return false;

  return deviceIndex() == other.deviceIndex()
         && subId() == other.subId()
         && address() == other.address();
}

bool Message::isErrorResponseTo(const Message& other) const
{
  if (!isValid() || !other.isValid()) return false;

  return deviceIndex() == other.deviceIndex()
         && errorSubId() == other.subId()
         && errorAddress() == other.address();
}

Message& Message::convertToLong()
{
  if (!isShort()) return *this;

  m_data.resize(20, 0);
  m_data[Offset::Type] = to_integral(Type::Long);
  return *this;
}

Message Message::toLong() const {
  return Message(*this).convertToLong();
}

QString Message::hex() const
{
  return qPrintable(QByteArray::fromRawData(
    reinterpret_cast<const char*>(m_data.data()), size()).toHex()
  );
}

// -------------------------------------------------------------------------------------------------
// -------------------------------------------------------------------------------------------------

FeatureSet::FeatureSet(HidppConnectionInterface* connection, QObject* parent)
  : QObject(parent)
  , m_connection(connection)
{}

// -------------------------------------------------------------------------------------------------

void FeatureSet::getProtocolVersion(std::function<void(MsgResult, Error, ProtocolVersion)> cb)
{
  if (!m_connection) {
    if (cb) cb(MsgResult::WriteError, Error::Unknown, {});
    return;
  }

  // Get wireless device 1 protocol version
  Message reqMsg(Message::Type::Short, DeviceIndex::WirelessDevice1, 0, 1, getRandomPingPayload());

  m_connection->sendRequest(std::move(reqMsg),
  makeSafeCallback([this, cb=std::move(cb)](MsgResult res, Message msg) {
    if (cb) {
      auto pv = (res == MsgResult::Ok) ? ProtocolVersion{ msg[4], msg[5] } : ProtocolVersion();
      cb(res, (res == MsgResult::HidppError) ? msg.errorCode() : Error::NoError, std::move(pv));
    }
  }));
}

void FeatureSet::getFeatureIndex(FeatureCode fc, std::function<void(MsgResult, uint8_t)> cb)
{
  postSelf([this, fc, cb=std::move(cb)]()
  {

  });
}

void FeatureSet::initFromDevice()
{
  if (m_connection == nullptr) return;

  getProtocolVersion(makeSafeCallback([this](MsgResult res, Error err, ProtocolVersion pv)
  {
    if (err == Error::NoError) {}
    logDebug(hid) << tr("ProtocolVersion = %2.%3 (%1)")
      .arg(m_connection->toString(res))
      .arg(int(pv.major))
      .arg(int(pv.minor));
    m_protocolVersion = std::move(pv);
  }));
}

// -------------------------------------------------------------------------------------------------


FeatureSet::State FeatureSet::state() const {
  return m_state;
}

// // -------------------------------------------------------------------------------------------------
// QByteArray FeatureSet::getResponseFromDevice(const QByteArray& expectedBytes)
// {
//   if (m_connection == nullptr) return QByteArray();

//   QByteArray readVal(20, 0);
//   int timeOut = 4; // time out just in case device did not reply;
//                    // 4 seconds time out is used by other programs like Solaar.
//   QTime timeOutTime = QTime::currentTime().addSecs(timeOut);
//   while(true) {
//     if(::read(m_fdHIDDevice, readVal.data(), readVal.length())) {
//       if (readVal.mid(1, 3) == expectedBytes) return readVal;
//       if (static_cast<uint8_t>(readVal.at(2)) == 0x8f) return readVal;  //Device not online
//       if (QTime::currentTime() >= timeOutTime) return QByteArray();
//     }
//   }
// }

uint8_t FeatureSet::getFeatureIndexFromDevice(FeatureCode /* fc */)
{
  if (m_connection == nullptr) return 0x00;
  // TODO implement

  // using MsgResult = HidppConnectionInterface::MsgResult;
  // const uint8_t fSetLSB = static_cast<uint8_t>(to_integral(fc) >> 8);
  // const uint8_t fSetMSB = static_cast<uint8_t>(to_integral(fc));

  // Message::Data featureReqMessage {
  //   HIDPP::Bytes::LONG_MSG, HIDPP::Bytes::MSG_TO_SPOTLIGHT, 0x00, getRandomFunctionCode(0x00),
  //   fSetLSB, fSetMSB, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00
  // };

  // m_connection->sendRequest(std::move(featureReqMessage),
  // makeSafeCallback([this](MsgResult result, Message msg) {
  //   if (result != MsgResult::Ok) {
  //     logDebug(hid) << tr("Failed to write feature request message to device.");
  //   }
  //   Q_UNUSED(msg);
  //   // TODO ?? getFeatureIndex with cb??
  // }));

  // const auto res = ::write(m_fdHIDDevice, featureReqMessage.data(), featureReqMessage.size());
  // if (res != featureReqMessage.size())
  // {
  //   logDebug(hid) << Hid_::tr("Failed to write feature request message to device.");
  //   return 0x00;
  // }

  // const auto response = getResponseFromDevice(featureReqMessage.mid(1, 3));
  // if (!response.length() || static_cast<uint8_t>(response.at(2)) == 0x8f) return 0x00;
  // uint8_t featureIndex = static_cast<uint8_t>(response.at(4));
  // TODO

  return 0x00; //featureIndex;
}

// -------------------------------------------------------------------------------------------------
uint8_t FeatureSet::getFeatureCountFromDevice(uint8_t /* featureSetIndex */)
{
  if (m_connection == nullptr) return 0x00;

  // Get Number of features (except Root Feature) supported
  // const auto featureCountReqMessage = make_QByteArray(HidppMsg{
  //   HIDPP::Bytes::LONG_MSG, HIDPP::Bytes::MSG_TO_SPOTLIGHT, featureSetIndex, getRandomFunctionCode(0x00),
  //   0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00
  // });
  // TODO implement

  // const auto res = ::write(m_fdHIDDevice, featureCountReqMessage.data(), featureCountReqMessage.size());
  // if (res != featureCountReqMessage.size())
  // {
  //   logDebug(hid) << Hid_::tr("Failed to write feature count request message to device.");
  //   return 0x00;
  // }

  // const auto response = getResponseFromDevice(featureCountReqMessage.mid(1, 3));
  // if (!response.length() || static_cast<uint8_t>(response.at(2)) == 0x8f) return 0x00;
  // uint8_t featureCount = static_cast<uint8_t>(response.at(4));

  // TODO return featureCount;
  return 0;
}

QByteArray FeatureSet::getFirmwareVersionFromDevice()
{
  if (m_connection == nullptr) return 0x00;

  // To get firmware details: first get Feature Index corresponding to Firmware feature code
  uint8_t fwIndex = getFeatureIndexFromDevice(FeatureCode::FirmwareVersion);
  if (!fwIndex) return QByteArray();

  // Get the number of firmwares (Main HID++ application, BootLoader, or Hardware) now
  // const auto fwCountReqMessage = make_QByteArray(HidppMsg{
  //   HIDPP::Bytes::LONG_MSG, HIDPP::Bytes::MSG_TO_SPOTLIGHT, fwIndex, getRandomFunctionCode(0x00),
  //   0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00
  // });

  // TODO implement
  // const auto res = ::write(m_fdHIDDevice, fwCountReqMessage.data(), fwCountReqMessage.size());
  // if (res != fwCountReqMessage.size())
  // {
  //   logDebug(hid) << Hid_::tr("Failed to write firmware count request message to device.");
  //   return 0x00;
  // }

  // TODO implement
  // const auto response = getResponseFromDevice(fwCountReqMessage.mid(1, 3));
  // if (!response.length() || static_cast<uint8_t>(response.at(2)) == 0x8f) return QByteArray();
  // const uint8_t fwCount = static_cast<uint8_t>(response.at(4));
  const uint8_t fwCount = 1;

  // TODO implement
  // The following info is not used currently; however, these commented lines are kept for future reference.
  // uint8_t connectionMode = static_cast<uint8_t>(response.at(10));
  // bool supportBluetooth = (connectionMode & 0x01);
  // bool supportBluetoothLE = (connectionMode & 0x02);  // true for Logitech Spotlight
  // bool supportUsbReceiver = (connectionMode & 0x04);  // true for Logitech Spotlight
  // bool supportUsbWired = (connectionMode & 0x08);
  // auto unitID = response.mid(5, 4);
  // auto modelIDs = response.mid(11, 8);
  // int count = 0;
  // if (supportBluetooth) { auto btmodelID = modelIDs.mid(count, 2); count += 2;}
  // if (supportBluetoothLE) { auto btlemodelID = modelIDs.mid(count, 2); count += 2;}
  // if (supportUsbReceiver) { auto wpmodelID = modelIDs.mid(count, 2); count += 2;}
  // if (supportUsbWired) { auto usbmodelID = modelIDs.mid(count, 2); count += 2;}

  // TODO implement
  // Iteratively find out firmware versions for all firmwares and get the firmware for main application
  for (uint8_t i = 0x00; i < fwCount; i++)
  {
    // const auto fwVerReqMessage = make_QByteArray(HidppMsg{
    //   HIDPP::Bytes::LONG_MSG, HIDPP::Bytes::MSG_TO_SPOTLIGHT, fwIndex, getRandomFunctionCode(0x10),
    //   i, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00
    // });

    // const auto res = ::write(m_fdHIDDevice, fwVerReqMessage.data(), fwVerReqMessage.length());
    // if (res != fwCountReqMessage.size())
    // {
    //   logDebug(hid) << Hid_::tr("Failed to write firmware request message to device (%1).")
    //                          .arg(int(i));
    //   return 0x00;
    // }
    // const auto fwResponse = getResponseFromDevice(fwVerReqMessage.mid(1, 3));
    // if (!fwResponse.length() || static_cast<uint8_t>(fwResponse.at(2)) == 0x8f) return QByteArray();
    // const auto fwType = (fwResponse.at(4) & 0x0f);  // 0 for main HID++ application, 1 for BootLoader, 2 for Hardware, 3-15 others
    // const auto fwVersion = fwResponse.mid(5, 7);
    // // Currently we are not interested in these details; however, these commented lines are kept for future reference.
    // //auto firmwareName = fwVersion.mid(0, 3).data();
    // //auto majorVesion = fwResponse.at(3);
    // //auto MinorVersion = fwResponse.at(4);
    // //auto build = fwResponse.mid(5);
    // if (fwType == 0)
    // {
    //   logDebug(hid) << "Main application firmware Version:" << fwVersion.toHex();
    //   return fwVersion;
    // }
  }
  return QByteArray();
}

// -------------------------------------------------------------------------------------------------
void FeatureSet::populateFeatureTable()
{
  if (m_connection == nullptr) return;

  // Get the firmware version
  const auto firmwareVersion = getFirmwareVersionFromDevice();
  if (!firmwareVersion.length()) return;

  // TODO:: Read and write cache file (settings most probably)
  // if the firmware details match with cached file; then load the FeatureTable from file
  // else read the entire feature table from the device
  QByteArray cacheFirmwareVersion;  // currently a dummy variable for Firmware Version from cache file.

  if (firmwareVersion == cacheFirmwareVersion)
  {
    // TODO: load the featureSet from the cache file
  }
  else
  {
    // For reading feature table from device
    // first get featureIndex for FeatureCode::FeatureSet
    // then we can get the number of features supported by the device (except Root Feature)
    const uint8_t featureSetIndex = getFeatureIndexFromDevice(FeatureCode::FeatureSet);
    if (!featureSetIndex) return;
    const uint8_t featureCount = getFeatureCountFromDevice(featureSetIndex);
    if (!featureCount) return;

    // Root feature is supported by all HID++ 2.0 device and has a featureIndex of 0 always.
    m_featureTable.insert({to_integral(FeatureCode::Root), 0x00});

    // Read Feature Code for other featureIndices from device.
    for (uint8_t featureIndex = 0x01; featureIndex <= featureCount; ++featureIndex)
    {
      // const auto featureCodeReqMsg = make_QByteArray(HidppMsg{
      //   HIDPP::Bytes::LONG_MSG, HIDPP::Bytes::MSG_TO_SPOTLIGHT, featureSetIndex, getRandomFunctionCode(0x10), featureIndex,
      //   0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00
      // });
      // const auto res = ::write(m_fdHIDDevice, featureCodeReqMsg.data(), featureCodeReqMsg.size());
      // if (res != featureCodeReqMsg.size()) {
      //   logDebug(hid) << Hid_::tr("Failed to write feature code request message to device.");
      //   return;
      // }

      // const auto response = getResponseFromDevice(featureCodeReqMsg.mid(1, 3));
      // if (!response.length() || static_cast<uint8_t>(response.at(2)) == 0x8f) {
      //   m_featureTable.clear();
      //   return;
      // }
      // const uint16_t featureCode = (static_cast<uint16_t>(response.at(4)) << 8) | static_cast<uint8_t>(response.at(5));
      // const uint8_t featureType = static_cast<uint8_t>(response.at(6));
      // const auto softwareHidden = (featureType & (1<<6));
      // const auto obsoleteFeature = (featureType & (1<<7));
      // if (!(softwareHidden) && !(obsoleteFeature)) m_featureTable.insert({featureCode, featureIndex});
    }
  }
}

// -------------------------------------------------------------------------------------------------
bool FeatureSet::supportFeatureCode(FeatureCode fc) const
{
  const auto featurePair = m_featureTable.find(to_integral(fc));
  return (featurePair != m_featureTable.end());
}

// -------------------------------------------------------------------------------------------------
uint8_t FeatureSet::getFeatureIndex(FeatureCode fc) const
{
  if (!supportFeatureCode(fc)) return 0x00;

  const auto featureInfo = m_featureTable.find(to_integral(fc));
  return featureInfo->second;
}

} // end namespace HIDPP
