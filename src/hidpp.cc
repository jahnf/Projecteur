// This file is part of Projecteur - https://github.com/jahnf/projecteur
// - See LICENSE.md and README.md

#include "hidpp.h"
#include "logging.h"
#include "enum-helper.h"

#include <unistd.h>

#include <memory>
#include <random>

DECLARE_LOGGING_CATEGORY(hid)

namespace {
  // -----------------------------------------------------------------------------------------------
  namespace Defaults {
    constexpr uint8_t HidppSoftwareId = 7;
  }

  // -----------------------------------------------------------------------------------------------
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

    constexpr uint32_t Payload = 4;

    constexpr uint32_t FwType = Payload;
    constexpr uint32_t FwPrefix = FwType + 1;
    constexpr uint32_t FwVersion = FwPrefix + 3;
    constexpr uint32_t FwBuild = FwVersion + 2;
  }

  // -----------------------------------------------------------------------------------------------
  namespace Defines {
    constexpr uint8_t ErrorShort = 0x8f;
    constexpr uint8_t ErrorLong = 0xff;
  }

  // -----------------------------------------------------------------------------------------------
  uint8_t funcSwIdToByte(uint8_t function, uint8_t swId) {
    return (swId & 0x0f)|((function & 0x0f) << 4);
  }

  // -----------------------------------------------------------------------------------------------
  uint8_t getRandomByte()
  {
    static std::mt19937 gen(std::random_device{}());
    std::uniform_int_distribution<uint8_t> distribution;
    return distribution(gen);
  }
}  // end anonymous namespace

// -------------------------------------------------------------------------------------------------
const char* toString(HidppConnectionInterface::MsgResult res)
{
  using MsgResult = HidppConnectionInterface::MsgResult;
  switch(res) {
    ENUM_CASE_STRINGIFY(MsgResult::Ok);
    ENUM_CASE_STRINGIFY(MsgResult::InvalidFormat);
    ENUM_CASE_STRINGIFY(MsgResult::WriteError);
    ENUM_CASE_STRINGIFY(MsgResult::Timeout);
    ENUM_CASE_STRINGIFY(MsgResult::HidppError);
    ENUM_CASE_STRINGIFY(MsgResult::FeatureNotSupported);
  }
  return "MsgResult::(unknown)";
}

// -------------------------------------------------------------------------------------------------
const char* toString(HIDPP::Error e)
{
  using Error = HIDPP::Error;
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

namespace HIDPP {
// -------------------------------------------------------------------------------------------------
Message::Data getRandomPingPayload() {
  return {0, 0, getRandomByte()};
}

// -------------------------------------------------------------------------------------------------
Message::Message() = default;

// -------------------------------------------------------------------------------------------------
Message::Message(Type type)
  : Message(type, DeviceIndex::DefaultDevice, 0, 0, Defaults::HidppSoftwareId, {})
{}

// -------------------------------------------------------------------------------------------------
Message::Message(std::vector<uint8_t>&& data) : m_data(std::move(data)) {}

// -------------------------------------------------------------------------------------------------
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

// -------------------------------------------------------------------------------------------------
Message::Message(Type type, uint8_t deviceIndex, uint8_t featureIndex, uint8_t function,
                 Data payload)
  : Message(type, deviceIndex, featureIndex, function, Defaults::HidppSoftwareId, std::move(payload))
{}

// -------------------------------------------------------------------------------------------------
Message::Message(Type type, uint8_t deviceIndex, uint8_t featureIndex, Data payload)
  : Message(type, deviceIndex, featureIndex, 0, Defaults::HidppSoftwareId, std::move(payload))
{}

// -------------------------------------------------------------------------------------------------
Message::Message(Type type, uint8_t deviceIndex, Data payload)
  : Message(type, deviceIndex, 0, 0, Defaults::HidppSoftwareId, std::move(payload))
{}

// -------------------------------------------------------------------------------------------------
size_t Message::size() const
{
  if (isLong()) return 20;
  if (isShort()) return 7;
  return 0;
}

// -------------------------------------------------------------------------------------------------
Message::Type Message::type() const
{
  if (isLong()) return Type::Long;
  if (isShort()) return Type::Short;
  return Type::Invalid;
}

// -------------------------------------------------------------------------------------------------
bool Message::isValid() const { return isLong() || isShort(); }

// -------------------------------------------------------------------------------------------------
bool Message::isShort() const {
  return (m_data.size() >= 7 && m_data[Offset::Type] == to_integral(Message::Type::Short));
}

// -------------------------------------------------------------------------------------------------
bool Message::isLong() const {
  return (m_data.size() >= 20 && m_data[Offset::Type] == to_integral(Message::Type::Long));
}

// -------------------------------------------------------------------------------------------------
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

// -------------------------------------------------------------------------------------------------
uint8_t Message::errorSubId() const {
  return m_data[Offset::ErrorSubId];
}

// -------------------------------------------------------------------------------------------------
uint8_t  Message::errorAddress() const {
  return m_data[Offset::ErrorAddress];
}

// -------------------------------------------------------------------------------------------------
uint8_t  Message::errorFeatureIndex() const {
  return m_data[Offset::ErrorFeatureIndex];
}

// -------------------------------------------------------------------------------------------------
uint8_t  Message::errorFunction() const {
  return ((m_data[Offset::ErrorAddress] & 0xf0) >> 4);
}

// -------------------------------------------------------------------------------------------------
uint8_t  Message::errorSoftwareId() const {
  return (m_data[Offset::ErrorAddress] & 0x0f);
}

// -------------------------------------------------------------------------------------------------
HIDPP::Error Message::errorCode() const {
  return to_enum<HIDPP::Error>(m_data[Offset::ErrorCode]);
}

// -------------------------------------------------------------------------------------------------
uint8_t Message::deviceIndex() const {
  return m_data[Offset::DeviceIndex];
}

// -------------------------------------------------------------------------------------------------
uint8_t Message::subId() const {
  return m_data[Offset::SubId];
}

// -------------------------------------------------------------------------------------------------
uint8_t Message::address() const {
  return m_data[Offset::Address];
}

// -------------------------------------------------------------------------------------------------
uint8_t Message::featureIndex() const {
  return m_data[Offset::Address];
}

// -------------------------------------------------------------------------------------------------
uint8_t Message::function() const {
  return ((m_data[Offset::Address] & 0xf0) >> 4);
}

// -------------------------------------------------------------------------------------------------
uint8_t Message::softwareId() const {
  return (m_data[Offset::Address] & 0x0f);
}

// -------------------------------------------------------------------------------------------------
void Message::setSubId(uint8_t subId) {
  m_data[Offset::SubId] = subId;
}

// -------------------------------------------------------------------------------------------------
void Message::setAddress(uint8_t address) {
  m_data[Offset::Address] = address;
}

// -------------------------------------------------------------------------------------------------
void Message::setFeatureIndex(uint8_t featureIndex) {
  m_data[Offset::FeatureIndex] = featureIndex;
}

// -------------------------------------------------------------------------------------------------
void Message::setFunction(uint8_t function) {
  m_data[Offset::Address] = ((function & 0x0f) << 4) | (m_data[Offset::Address] & 0x0f);
}

// -------------------------------------------------------------------------------------------------
void Message::setSoftwareId(uint8_t softwareId) {
  m_data[Offset::Address] = (softwareId & 0x0f) | (m_data[Offset::Address] & 0xf0);
}

// -------------------------------------------------------------------------------------------------
bool Message::isResponseTo(const Message& other) const
{
  if (!isValid() || !other.isValid()) return false;

  return deviceIndex() == other.deviceIndex()
         && subId() == other.subId()
         && address() == other.address();
}

// -------------------------------------------------------------------------------------------------
bool Message::isErrorResponseTo(const Message& other) const
{
  if (!isValid() || !other.isValid()) return false;

  return deviceIndex() == other.deviceIndex()
         && errorSubId() == other.subId()
         && errorAddress() == other.address();
}

// -------------------------------------------------------------------------------------------------
Message& Message::convertToLong()
{
  if (!isShort()) return *this;

  // Resize data vector, pad with zeroes.
  m_data.resize(20, 0);
  m_data[Offset::Type] = to_integral(Type::Long);
  return *this;
}

// -------------------------------------------------------------------------------------------------
Message Message::toLong() const {
  return Message(*this).convertToLong();
}

// -------------------------------------------------------------------------------------------------
QString Message::hex() const
{
  return qPrintable(QByteArray::fromRawData(
    reinterpret_cast<const char*>(m_data.data()), isValid() ? size() : m_data.size()).toHex()
  );
}

// =================================================================================================
FeatureSet::FeatureSet(HidppConnectionInterface* connection, QObject* parent)
  : QObject(parent)
  , m_connection(connection)
{}

// -------------------------------------------------------------------------------------------------
FeatureSet::State FeatureSet::state() const {
  return m_state;
}

// -------------------------------------------------------------------------------------------------
void FeatureSet::setState(State s)
{
  if (s == m_state) return;

  m_state = s;
  emit stateChanged(m_state);
}

// -------------------------------------------------------------------------------------------------
void FeatureSet::getFeatureIndex(FeatureCode fc, std::function<void(MsgResult, uint8_t)> cb)
{
  postSelf([this, fc, cb=std::move(cb)]()
  {
    if (m_connection == nullptr)
    {
      if (cb) cb(MsgResult::WriteError, 0);
      return;
    }

    const uint8_t fcLSB = static_cast<uint8_t>(to_integral(fc) >> 8);
    const uint8_t fcMSB = static_cast<uint8_t>(to_integral(fc) & 0x00ff);

    Message featureIndexReqMsg(Message::Type::Long, DeviceIndex::WirelessDevice1,
                               Message::Data{fcLSB, fcMSB});

    m_connection->sendRequest(std::move(featureIndexReqMsg),
    [cb=std::move(cb), fc, fcLSB, fcMSB](MsgResult result, Message&& msg)
    {
      logDebug(hid) << tr("getFeatureIndex(%1) => %2, %3")
                       .arg(to_integral(fc)).arg(toString(result)).arg(msg[4]);
      if (cb) cb(result, (result != MsgResult::Ok) ? 0 : msg[4]);
    });
  });
}

// -------------------------------------------------------------------------------------------------
void FeatureSet::getFeatureCount(std::function<void(MsgResult, uint8_t, uint8_t)> cb)
{
  getFeatureIndex(FeatureCode::FeatureSet, makeSafeCallback(
  [this, cb=std::move(cb)](MsgResult res, uint8_t featureIndex)
  {
    if (res != MsgResult::Ok)
    {
      if (cb) cb(res, 0, 0);
      return;
    }

    Message featureCountReqMsg(Message::Type::Long, DeviceIndex::WirelessDevice1, featureIndex);

    m_connection->sendRequest(std::move(featureCountReqMsg),
    [featureIndex, cb=std::move(cb)](MsgResult result, Message&& msg) {
      if (cb) cb(result, featureIndex, (result != MsgResult::Ok) ? 0 : msg[4]);
    });
  }));
}

// -------------------------------------------------------------------------------------------------
void FeatureSet::getFirmwareCount(std::function<void(MsgResult, uint8_t, uint8_t)> cb)
{
  getFeatureIndex(FeatureCode::FirmwareVersion, makeSafeCallback(
  [this, cb=std::move(cb)](MsgResult res, uint8_t featureIndex)
  {
    if (res != MsgResult::Ok)
    {
      if (cb) cb(res, 0, 0);
      return;
    }

    Message fwCountReqMsg(Message::Type::Long, DeviceIndex::WirelessDevice1, featureIndex);

    m_connection->sendRequest(std::move(fwCountReqMsg),
    [featureIndex, cb=std::move(cb)](MsgResult result, Message&& msg)
    {
      logDebug(hid) << tr("getFirmwareCount() => %1, featureIndex = %2, count = %3")
                       .arg(toString(result)).arg(featureIndex).arg(msg[4]);
      if (cb) cb(result, featureIndex, (result != MsgResult::Ok) ? 0 : msg[4]);
    });
  }));
}

// -------------------------------------------------------------------------------------------------
void FeatureSet::getFirmwareInfo(uint8_t fwIndex, uint8_t entity,
                                 std::function<void(MsgResult, FirmwareInfo&&)> cb)
{
  if (m_connection == nullptr)
  {
    if (cb) cb(MsgResult::WriteError, FirmwareInfo());
    return;
  }

  Message fwVerReqMessage(Message::Type::Long, DeviceIndex::WirelessDevice1, fwIndex, 1,
                          Message::Data{entity});

  m_connection->sendRequest(std::move(fwVerReqMessage),
  [cb=std::move(cb)](MsgResult res, Message&& msg) {
    if (cb) cb(res, FirmwareInfo(std::move(msg)));
  });
}

// -------------------------------------------------------------------------------------------------
void FeatureSet::getMainFirmwareInfo(std::function<void(MsgResult, FirmwareInfo&&)> cb)
{
  getFirmwareCount(makeSafeCallback(
  [this, cb=std::move(cb)](MsgResult res, uint8_t featureIndex, uint8_t count)
  {
    if (res != MsgResult::Ok)
    {
      if (cb) cb(res, FirmwareInfo());
      return;
    }
    getMainFirmwareInfo(featureIndex, count, 0, std::move(cb));
  }));
}

// -------------------------------------------------------------------------------------------------
void FeatureSet::getMainFirmwareInfo(uint8_t fwIndex, uint8_t max, uint8_t current,
                                     std::function<void(MsgResult, FirmwareInfo&&)> cb)
{
  getFirmwareInfo(fwIndex, current, makeSafeCallback(
  [this, current, max, fwIndex, cb=std::move(cb)](MsgResult res, FirmwareInfo&& fi)
  {
    logDebug(hid) << tr("getFirmwareInfo(%1, %2, %3) => %4, fi.type = %5, fi.ver = %6, fi.pref = %7")
                     .arg(fwIndex).arg(max).arg(current).arg(toString(res))
                     .arg(to_integral(fi.firmwareType())).arg(fi.firmwareVersion()).arg(fi.firmwarePrefix());

    if (res == MsgResult::Ok && fi.firmwareType() == FirmwareInfo::FirmwareType::MainApp)
    {
      if (cb) cb(res, std::move(fi));
      return;
    }

    if (max == current + 1) {
      if (cb) cb(res, FirmwareInfo());
      return;
    }

    getMainFirmwareInfo(fwIndex, max, current + 1, std::move(cb));
  }));
}

// -------------------------------------------------------------------------------------------------
void FeatureSet::initFromDevice(std::function<void(State)> cb)
{
  postSelf([this, cb=std::move(cb)]()
  {
    if (m_connection == nullptr || m_state == State::Initialized || m_state == State::Initializing)
    {
      if (cb) cb(m_state);
      return;
    }

    setState(State::Initializing);

    getMainFirmwareInfo(makeSafeCallback([this, cb=std::move(cb)](MsgResult res, FirmwareInfo&& fi)
    {
      logDebug(hid) << tr("getMainFirmwareInfo() => %1, fi.type = %2").arg(toString(res))
      .arg(to_integral(fi.firmwareType()));

      if (fi.firmwareType() == FirmwareInfo::FirmwareType::MainApp) {
        m_mainFirmwareInfo = std::move(fi);
      }

      // Independent from firmware result try to get features
      Q_UNUSED(res);

      getFeatureCount(makeSafeCallback(
      [this, cb=std::move(cb)](MsgResult res, uint8_t featureIndex, uint8_t count)
      {
        logDebug(hid) << tr("getFeatureCount() => %1, featureIndex = %2, count = %3")
                         .arg(toString(res)).arg(featureIndex).arg(count);

        if (res != MsgResult::Ok)
        {
          setState(State::Error);
          if (cb) cb(m_state);
          return;
        }

        getFeatureIds(featureIndex, count, makeSafeCallback(
        [this, cb=std::move(cb)](MsgResult res, FeatureTable&& ft)
        {
          if (res != MsgResult::Ok) {
            setState(State::Error);
          }
          else
          {
            m_featureTable = std::move(ft);
            setState(State::Initialized);
          }

          if (cb) cb(m_state);
        })); // getFeatureIds (table)
      })); // getFeatureCount
    })); // getMainFwInfo
  }); // postSelf
}

// -------------------------------------------------------------------------------------------------
void FeatureSet::getFeatureIds(uint8_t featureSetIndex, uint8_t count,
                               std::function<void(MsgResult, FeatureTable&&)> cb)
{
  if (m_connection == nullptr)
  {
    if (cb) cb(MsgResult::WriteError, FeatureTable{}); // empty featuretable
    return;
  }

  if (count == 0)
  {
    if (cb) cb(MsgResult::Ok, FeatureTable{}); // no count, empty featuretable
    return;
  }

  auto featureTable = std::make_shared<FeatureTable>();

  HidppConnectionInterface::RequestBatch batch;
  for (uint8_t featureIndex = 1; featureIndex <= count; ++featureIndex)
  {
    // featureIdReqMsg[Offset::Payload] = featureIndex;
    batch.emplace(HidppConnectionInterface::RequestBatchItem {
      Message(Message::Type::Long, DeviceIndex::WirelessDevice1, featureSetIndex, 1,
              Message::Data{featureIndex}),
      [featureTable, featureIndex](MsgResult res, Message&& msg)
      {
        if (res != MsgResult::Ok) return;
        const uint16_t featureCode = (static_cast<uint16_t>(msg[4]) << 8)
                                     | static_cast<uint8_t>(msg[5]);
        const uint8_t featureType = msg[6];
        const bool softwareHidden = (featureType & (1<<6));
        const bool obsoleteFeature = (featureType & (1<<7));
        if (!softwareHidden && !obsoleteFeature) {
          // logDebug(hid) << tr("featureCode %1 -> index %2").arg(featureCode).arg(featureIndex);
          featureTable->emplace(featureCode, featureIndex);
        }
      }
    });
  }

  m_connection->sendRequestBatch(std::move(batch),
  [featureTable, cb=std::move(cb)](std::vector<MsgResult>&& results) {
    if (cb) cb(results.back(), std::move(*featureTable));
  });
}

// -------------------------------------------------------------------------------------------------
bool FeatureSet::featureCodeSupported(FeatureCode fc) const
{
  const auto featurePair = m_featureTable.find(to_integral(fc));
  return (featurePair != m_featureTable.end());
}

// -------------------------------------------------------------------------------------------------
uint8_t FeatureSet::featureIndex(FeatureCode fc) const
{
  const auto it = m_featureTable.find(to_integral(fc));
  if (it == m_featureTable.cend()) {
    return 0x00;
  }
  return it->second;
}

// =================================================================================================
FirmwareInfo::FirmwareInfo(Message&& msg)
  : m_rawMsg(std::move(msg))
{}

// -------------------------------------------------------------------------------------------------
FirmwareInfo::FirmwareType FirmwareInfo::firmwareType() const
{
  if (!m_rawMsg.isLong()) return FirmwareType::Invalid;

  switch(m_rawMsg[Offset::Payload] & 0xf)
  {
    case 0: return FirmwareType::MainApp;
    case 1: return FirmwareType::Bootloader;
    case 2: return FirmwareType::Hardware;
    default: return FirmwareType::Other;
  }
}

// -------------------------------------------------------------------------------------------------
QString FirmwareInfo::firmwarePrefix() const
{
  if (!m_rawMsg.isLong()) return QString();

  return QString(
    QByteArray::fromRawData(reinterpret_cast<const char*>(&m_rawMsg[Offset::FwPrefix]), 3)
  );
}

// -------------------------------------------------------------------------------------------------
uint16_t FirmwareInfo::firmwareVersion() const
{
  if (!m_rawMsg.isLong()) return 0;

  const auto& fwVersionMsb = m_rawMsg[Offset::FwVersion];
  const auto& fwVersionLsb = m_rawMsg[Offset::FwVersion+1];

  // Firmware version is BCD encoded
  return (  fwVersionLsb        & 0xF)
       + (((fwVersionLsb >> 4 ) & 0xF) * 10)
       + (( fwVersionMsb        & 0xF) * 100)
       + (((fwVersionMsb >> 4)  & 0xF) * 1000);
}

// -------------------------------------------------------------------------------------------------
uint16_t FirmwareInfo::firmwareBuild() const
{
  if (!m_rawMsg.isLong()) return 0;

  const auto& fwBuildMsb = m_rawMsg[Offset::FwBuild];
  const auto& fwBuildLsb = m_rawMsg[Offset::FwBuild+1];

  // Firmware build is BCD encoded ??
  return (  fwBuildLsb        & 0xF)
       + (((fwBuildLsb >> 4 ) & 0xF) * 10)
       + (( fwBuildMsb        & 0xF) * 100)
       + (((fwBuildMsb >> 4)  & 0xF) * 1000);
}

} // end namespace HIDPP

// -------------------------------------------------------------------------------------------------
const char* toString(HIDPP::FeatureSet::State s)
{
  using State = HIDPP::FeatureSet::State;
  switch (s)
  {
    ENUM_CASE_STRINGIFY(State::Uninitialized);
    ENUM_CASE_STRINGIFY(State::Initialized);
    ENUM_CASE_STRINGIFY(State::Initializing);
    ENUM_CASE_STRINGIFY(State::Error);
  };
  return "State::(unknown)";
}

// -------------------------------------------------------------------------------------------------
const char* toString(HIDPP::FeatureCode fc)
{
  using FeatureCode = HIDPP::FeatureCode;
  switch (fc)
  {
    ENUM_CASE_STRINGIFY(FeatureCode::Root);
    ENUM_CASE_STRINGIFY(FeatureCode::FeatureSet);
    ENUM_CASE_STRINGIFY(FeatureCode::FirmwareVersion);
    ENUM_CASE_STRINGIFY(FeatureCode::DeviceName);
    ENUM_CASE_STRINGIFY(FeatureCode::Reset);
    ENUM_CASE_STRINGIFY(FeatureCode::DFUControlSigned);
    ENUM_CASE_STRINGIFY(FeatureCode::BatteryStatus);
    ENUM_CASE_STRINGIFY(FeatureCode::PresenterControl);
    ENUM_CASE_STRINGIFY(FeatureCode::Sensor3D);
    ENUM_CASE_STRINGIFY(FeatureCode::ReprogramControlsV4);
    ENUM_CASE_STRINGIFY(FeatureCode::WirelessDeviceStatus);
    ENUM_CASE_STRINGIFY(FeatureCode::SwapCancelButton);
    ENUM_CASE_STRINGIFY(FeatureCode::PointerSpeed);
  };
  return "FeatureCode::(unknown)";
}

// -------------------------------------------------------------------------------------------------
const char* toString(HIDPP::BatteryStatus bs)
{
  using BatteryStatus = HIDPP::BatteryStatus;
  switch (bs)
  {
    ENUM_CASE_STRINGIFY(BatteryStatus::AlmostFull);
    ENUM_CASE_STRINGIFY(BatteryStatus::Charging);
    ENUM_CASE_STRINGIFY(BatteryStatus::ChargingError);
    ENUM_CASE_STRINGIFY(BatteryStatus::Discharging);
    ENUM_CASE_STRINGIFY(BatteryStatus::Full);
    ENUM_CASE_STRINGIFY(BatteryStatus::InvalidBattery);
    ENUM_CASE_STRINGIFY(BatteryStatus::SlowCharging);
    ENUM_CASE_STRINGIFY(BatteryStatus::ThermalError);
  };
  return "BatteryStatus::(unknown)";
}
