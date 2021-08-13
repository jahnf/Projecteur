// This file is part of Projecteur - https://github.com/jahnf/projecteur
// - See LICENSE.md and README.md

#pragma once

#include "device-defs.h"
#include "asynchronous.h"

#include <map>
#include <queue>
#include <vector>

#include <QString>

// Hidpp specific functionality
// - code is heavily inspired by this library: https://github.com/cvuchener/hidpp
// - also see https://6xq.net/git/lars/lshidpp.git
// - also see https://github.com/cvuchener/g500/blob/master/doc/hidpp10.md

namespace HIDPP {
  namespace DeviceIndex {
    constexpr uint8_t DefaultDevice = 0xff;
    constexpr uint8_t CordedDevice = 0x00;
    constexpr uint8_t WirelessDevice1 = 1;
    constexpr uint8_t WirelessDevice2 = 2;
    constexpr uint8_t WirelessDevice3 = 3;
    constexpr uint8_t WirelessDevice4 = 4;
    constexpr uint8_t WirelessDevice5 = 5;
    constexpr uint8_t WirelessDevice6 = 6;
  } // end namespace DeviceIndex

  // see also: https://github.com/cvuchener/hidpp/blob/master/src/tools/hidpp-list-features.cpp
  // Feature Codes important for Logitech Spotlight
  enum class FeatureCode : uint16_t {
    Root                 = 0x0000,
    FeatureSet           = 0x0001,
    FirmwareVersion      = 0x0003,
    DeviceName           = 0x0005,
    Reset                = 0x0020,
    DFUControlSigned     = 0x00c2,
    BatteryStatus        = 0x1000,
    PresenterControl     = 0x1a00,
    Sensor3D             = 0x1a01,
    ReprogramControlsV4  = 0x1b04,
    WirelessDeviceStatus = 0x1db4,
    SwapCancelButton     = 0x2005,
    PointerSpeed         = 0x2205,
  };

  /// Hid++ 2.0 error codes
  enum class Error : uint8_t {
    NoError             = 0,
    Unknown             = 1,
    InvalidArgument     = 2,
    OutOfRange          = 3,
    HWError             = 4,
    LogitechInternal    = 5,
    InvalidFeatureIndex = 6,
    InvalidFunctionId   = 7,
    Busy                = 8, // Device (or receiver) busy
    Unsupported         = 9,
  };

  const char* toString(Error e);

  namespace Commands {
    constexpr uint8_t SetRegister     = 0x80;
    constexpr uint8_t GetRegister     = 0x81;
    constexpr uint8_t SetLongRegister = 0x82;
    constexpr uint8_t GetLongRegister = 0x83;
  }

  /// Hidpp message class, heavily inspired by this library: https://github.com/cvuchener/hidpp
  class Message final
  {
  public:
    using Data = std::vector<uint8_t>;

    /// HID++ message type.
    enum class Type : uint8_t {
      Invalid = 0x0,
      Short = 0x10,
      Long = 0x11,
    };

    /// Creates an invalid HID++ message object.
    Message();
    /// Creates an empty default HID++ message of the given type.
    /// An internal default is used as software id for the message.
    Message(Type type);
    /// Create a message with the given properties and payload.
    Message(Type type, uint8_t deviceIndex, uint8_t featureIndex, uint8_t function, uint8_t swId,
            Data payload);
    /// Create a message with the given properties and payload.
    /// An internal default is used as software id for the message.
    Message(Type type, uint8_t deviceIndex, uint8_t featureIndex, uint8_t function, Data payload);

    /// Create a message from raw data.
    /// If the data is not a valid Hidpp message, this will result in an invalid HID++ message.
    Message(std::vector<uint8_t>&& data);

    Message(Message&& msg) = default;
    Message(const Message& msg) = default;

    inline bool operator==(const Message& other) const { return m_data == other.m_data; }

    bool isValid() const;
    bool isLong() const;
    bool isShort() const;
    size_t size() const;

    bool isError() const;
    // --- For short error messages (isShort() && isError())
    uint8_t errorSubId() const;
    uint8_t errorAddress() const;
    // -- For long error messages (isLong() && isError())
    uint8_t errorFeatureIndex() const;
    uint8_t errorFunction() const;
    uint8_t errorSoftwareId() const;
    // --- for both long & short error messages
    Error errorCode() const;

    /// Converts the message to a long message, if it is a valid short message
    Message& convertToLong();
    /// Converts the message to a long message and returns it as a new object,
    /// if it is a valid short message.
    Message toLong() const;

    Type type() const;
    uint8_t deviceIndex() const;
    void setDeviceIndex(uint8_t);

    // --- HIDPP 1.0
    uint8_t subId() const;
    void setSubId(uint8_t subId);
    uint8_t address() const;
    void setAddress(uint8_t address);

    // --- HIDPP 2.0
    uint8_t featureIndex () const;
    void setFeatureIndex(uint8_t featureIndex);
    uint8_t function() const;
    void setFunction(uint8_t function);
    uint8_t softwareId() const;
    void setSoftwareId(uint8_t softwareId);

    /// Returns true if the message is a possible response to a given Hidpp message.
    bool isResponseTo(const Message& other) const;
    /// Returns true if the message is a possible error response to a given Hidpp message.
    bool isErrorResponseTo(const Message& other) const;

    auto data() { return m_data.data(); }
    auto dataSize() { return m_data.size(); }
    auto operator[](size_t i) { return m_data.operator[](i); }
    QString hex() const;

  private:
    Data m_data;
  };
} //end of HIDPP namespace

// -------------------------------------------------------------------------------------------------
/// Hidpp interface to be implemented by classes that allow communicating with a HID++ device.
class HidppConnectionInterface
{
public:
  enum class MsgResult : uint8_t {
    Ok = 0,
    InvalidFormat,
    WriteError,
    Timeout,
    HidppError,
  };

  static const char* toString(MsgResult);

  using SendResultCallback = std::function<void(MsgResult)>;
  using RequestResultCallback = std::function<void(MsgResult, HIDPP::Message)>;

  virtual BusType busType() const = 0;

  // --- synchronous versions
  virtual ssize_t sendData(std::vector<uint8_t> msg) = 0;
  virtual ssize_t sendData(HIDPP::Message msg) = 0;

  // --- asynchronous versions, implementations must return immediately
  virtual void sendData(std::vector<uint8_t> msg, SendResultCallback resultCb) = 0;
  virtual void sendData(HIDPP::Message msg, SendResultCallback resultCb) = 0;
  virtual void sendRequest(std::vector<uint8_t> msg, RequestResultCallback responseCb) = 0;
  virtual void sendRequest(HIDPP::Message msg, RequestResultCallback responseCb) = 0;

  struct RequestBatchItem {
    HIDPP::Message message;
    RequestResultCallback callback;
  };

  using RequestBatch = std::queue<RequestBatchItem>;
  using RequestBatchResultCallback = std::function<void(std::vector<MsgResult>)>;
  virtual void sendRequestBatch(RequestBatch requestBatch, RequestBatchResultCallback cb,
                                bool continueOnError = false) = 0;

  struct DataBatchItem {
    HIDPP::Message message;
    SendResultCallback callback;
  };

  using DataBatch = std::queue<DataBatchItem>;
  using DataBatchResultCallback = std::function<void(std::vector<MsgResult>)>;
  virtual void sendDataBatch(DataBatch dataBatch, DataBatchResultCallback cb,
                             bool continueOnError = false) = 0;
};

namespace HIDPP {
  // -----------------------------------------------------------------------------------------------
  namespace Bytes {
    constexpr uint8_t SHORT_WIRELESS_NOTIFICATION_CODE = 0x41; // TODO MOVE RENAME ///
  }

  /// Class to get and store set of supported features and additional information
  /// for a HID++ 2.0 device (although very much specialized for the Logitech Spotlight).
  class FeatureSet : public QObject, public async::Async<FeatureSet>
  {
    Q_OBJECT

  public:
    enum class State : uint8_t { Uninitialized, Initializing, Initialized, Error };

    FeatureSet(HidppConnectionInterface* connection, QObject* parent = nullptr);

    void initFromDevice();
    State state() const;

    uint8_t getFeatureIndex(FeatureCode fc) const;
    bool supportFeatureCode(FeatureCode fc) const;
    auto getFeatureCount() const { return m_featureTable.size(); }
    void populateFeatureTable();

  signals:
    void stateChanged(State s);

  private:
    struct ProtocolVersion {
      uint8_t major = 0;
      uint8_t minor = 0;
    };
    using MsgResult = HidppConnectionInterface::MsgResult;
    void getFeatureIndex(FeatureCode fc, std::function<void(MsgResult, uint8_t)> cb);

    void getProtocolVersion(std::function<void(MsgResult, HIDPP::Error, ProtocolVersion)> cb);

    uint8_t getFeatureIndexFromDevice(FeatureCode fc);
    uint8_t getFeatureCountFromDevice(uint8_t featureSetID);
    QByteArray getFirmwareVersionFromDevice();

    HidppConnectionInterface* m_connection = nullptr;
    std::map<uint16_t, uint8_t> m_featureTable;

    ProtocolVersion m_protocolVersion;

    State m_state = State::Uninitialized;
  };
} //end namespace HIDPP
