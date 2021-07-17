// This file is part of Projecteur - https://github.com/jahnf/projecteur - See LICENSE.md and README.md
#pragma once

#include "enum-helper.h"

#include <memory>

#include <QObject>

#include <linux/input.h>

// -------------------------------------------------------------------------------------------------
// Bus on which device is connected
enum class BusType : uint16_t { Unknown, Usb, Bluetooth };

// -------------------------------------------------------------------------------------------------
enum class BatteryStatus : uint8_t {Discharging    = 0x00,
                                    Charging       = 0x01,
                                    AlmostFull     = 0x02,
                                    Full           = 0x03,
                                    SlowCharging   = 0x04,
                                    InvalidBattery = 0x05,
                                    ThermalError   = 0x06};

struct BatteryInfo
{
  uint8_t currentLevel = 0;
  uint8_t nextReportedLevel = 0;
  BatteryStatus status = BatteryStatus::Discharging;
};

// -------------------------------------------------------------------------------------------------
struct DeviceId
{
  uint16_t vendorId = 0;
  uint16_t productId = 0;
  BusType busType = BusType::Unknown;
  QString phys; // should be sufficient to differentiate between two devices of the same type
                // - not tested, don't have two devices of any type currently.

  inline bool operator==(const DeviceId& rhs) const {
    return std::tie(vendorId, productId, busType, phys) == std::tie(rhs.vendorId, rhs.productId, rhs.busType, rhs.phys);
  }

  inline bool operator!=(const DeviceId& rhs) const {
    return std::tie(vendorId, productId, busType, phys) != std::tie(rhs.vendorId, rhs.productId, rhs.busType, rhs.phys);
  }

  inline bool operator<(const DeviceId& rhs) const {
    return std::tie(vendorId, productId, busType, phys) < std::tie(rhs.vendorId, rhs.productId, rhs.busType, rhs.phys);
  }
};

Q_DECLARE_METATYPE(DeviceId);

// -------------------------------------------------------------------------------------------------
class InputMapper;
class QSocketNotifier;
class SubDeviceConnection;
class VirtualDevice;

// -----------------------------------------------------------------------------------------------
enum class ConnectionType : uint8_t { Event, Hidraw };
enum class ConnectionMode : uint8_t { ReadOnly, WriteOnly, ReadWrite };

// -------------------------------------------------------------------------------------------------
class DeviceConnection : public QObject
{
  Q_OBJECT

public:
  DeviceConnection(const DeviceId& id, const QString& name, std::shared_ptr<VirtualDevice> vdev);
  ~DeviceConnection();

  const auto& deviceName() const { return m_deviceName; }
  const auto& deviceId() const { return m_deviceId; }
  const auto& inputMapper() const { return m_inputMapper; }

  auto subDeviceCount() const { return m_subDeviceConnections.size(); }
  bool hasSubDevice(const QString& path) const;
  void addSubDevice(std::shared_ptr<SubDeviceConnection>);
  bool removeSubDevice(const QString& path);
  const auto& subDevices() { return m_subDeviceConnections; }
  void queryBatteryStatus();
  auto getBatteryInfo(){return m_batteryInfo;};

public slots:
  void setBatteryInfo(QByteArray batteryData);

signals:
  void subDeviceConnected(const DeviceId& id, const QString& path);
  void subDeviceDisconnected(const DeviceId& id, const QString& path);

protected:
  using DevicePath = QString;
  using ConnectionMap = std::map<DevicePath, std::shared_ptr<SubDeviceConnection>>;

  DeviceId m_deviceId;
  QString m_deviceName;
  std::shared_ptr<InputMapper> m_inputMapper;
  ConnectionMap m_subDeviceConnections;
  BatteryInfo m_batteryInfo;
};

// -------------------------------------------------------------------------------------------------
enum class DeviceFlag : uint32_t {
  NoFlags = 0,
  NonBlocking    = 1 << 0,
  SynEvents      = 1 << 1,
  RepEvents      = 1 << 2,
  RelativeEvents = 1 << 3,
  KeyEvents      = 1 << 4,

  Vibrate        = 1 << 16,
};
ENUM(DeviceFlag, DeviceFlags)

// -----------------------------------------------------------------------------------------------
struct SubDeviceConnectionDetails {
  SubDeviceConnectionDetails(const QString& path, ConnectionType type, ConnectionMode mode, BusType busType)
    : type(type), mode(mode), busType(busType), devicePath(path) {}

  ConnectionType type;
  ConnectionMode mode;
  BusType busType;
  bool grabbed = false;
  DeviceFlags deviceFlags = DeviceFlags::NoFlags;
  QString phys;
  QString devicePath;
  float hidProtocolVer = -1;   // set after ping to HID sub-device; If positive then Hidraw device is online.
};

// -------------------------------------------------------------------------------------------------
template<int Size, typename T = struct input_event>
struct InputBuffer {
  auto pos() const { return pos_; }
  void reset() { pos_ = 0; }
  auto data() { return data_.data(); }
  auto size() const { return data_.size(); }
  T& current() { return data_.at(pos_); }
  InputBuffer& operator++() { ++pos_; return *this; }
  T& operator[](size_t pos) { return data_[pos]; }
  T& first() { return data_[0]; }
private:
  std::array<T, Size> data_;
  size_t pos_ = 0;
};

// -------------------------------------------------------------------------------------------------
class SubDeviceConnection : public QObject
{
  Q_OBJECT
public:
  virtual ~SubDeviceConnection() = 0;

  bool isConnected() const;
  void disconnect(); // destroys socket notifier and close file handle
  void disable(); // disable receiving/sending data
  void disableWrite(); // disable sending data
  void enableWrite(); // enable sending data

  // HID++ specific functions
  void initSubDevice();
  void resetSubDevice(struct timespec delay);
  void pingSubDevice();
  bool isOnline() {return (m_details.hidProtocolVer > 0);};
  void setHIDProtocol(float p) {m_details.hidProtocolVer = p;};
  float getHIDProtocol() {return m_details.hidProtocolVer;};
  void queryBatteryStatus();
  ssize_t sendData(const QByteArray& hidppMsg, bool checkDeviceOnline = true);               // Send HID++ Message to HIDraw connection
  ssize_t sendData(const void* hidppMsg, size_t hidppMsgLen, bool checkDeviceOnline = true); // Send HID++ Message to HIDraw connection

  auto type() const { return m_details.type; };
  auto mode() const { return m_details.mode; };
  auto isGrabbed() const { return m_details.grabbed; };
  auto flags() const { return m_details.deviceFlags; };
  const auto& phys() const { return m_details.phys; };
  const auto& path() const { return m_details.devicePath; };

  const std::shared_ptr<InputMapper>& inputMapper() const;
  QSocketNotifier* socketReadNotifier();   // Read notifier for Hidraw and Event connections for receiving data from device
  QSocketNotifier* socketWriteNotifier();  // Write notifier for Hidraw connection for sending data to device

protected:
  SubDeviceConnection(const QString& path, ConnectionType type, ConnectionMode mode, BusType busType);

  SubDeviceConnectionDetails m_details;
  std::shared_ptr<InputMapper> m_inputMapper; // shared input mapper from parent device.
  std::unique_ptr<QSocketNotifier> m_readNotifier;
  std::unique_ptr<QSocketNotifier> m_writeNotifier;   // only useful for Hidraw connections
};

// -------------------------------------------------------------------------------------------------
namespace DeviceScan {
  struct SubDevice;
}

// -------------------------------------------------------------------------------------------------
class SubEventConnection : public SubDeviceConnection
{
  Q_OBJECT
  class Token{};

public:
  static std::shared_ptr<SubEventConnection> create(const DeviceScan::SubDevice& sd,
                                                    const DeviceConnection& dc);

  SubEventConnection(Token, const QString& path);
  auto& inputBuffer() { return m_inputEventBuffer; }

protected:
  InputBuffer<12> m_inputEventBuffer;
};

// -------------------------------------------------------------------------------------------------
class SubHidrawConnection : public SubDeviceConnection
{
  Q_OBJECT
  class Token{};

public:
  static std::shared_ptr<SubHidrawConnection> create(const DeviceScan::SubDevice& sd,
                                                     const DeviceConnection& dc);

  SubHidrawConnection(Token, const QString& path);

signals:
  void receivedBatteryInfo(QByteArray batteryData);
  void receivedPingResponse();
};
