// This file is part of Projecteur - https://github.com/jahnf/projecteur - See LICENSE.md and README.md
#pragma once

#include "asynchronous.h"
#include "enum-helper.h"

#include <memory>
#include <vector>

#include <QObject>

#include <linux/input.h>

// -------------------------------------------------------------------------------------------------
// Bus on which device is connected
enum class BusType : uint8_t { Unknown, Usb, Bluetooth };

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

namespace HIDPP {
  class FeatureSet;
}

namespace DeviceScan {
  struct SubDevice;
}

// -------------------------------------------------------------------------------------------------
// Battery Status as returned on HID++ BatteryStatus feature code (0x1000)
enum class BatteryStatus : uint8_t {Discharging    = 0x00,
                                    Charging       = 0x01,
                                    AlmostFull     = 0x02,
                                    Full           = 0x03,
                                    SlowCharging   = 0x04,
                                    InvalidBattery = 0x05,
                                    ThermalError   = 0x06,
                                    ChargingError  = 0x07
                                   };

struct BatteryInfo
{
  uint8_t currentLevel = 0;
  uint8_t nextReportedLevel = 0;
  BatteryStatus status = BatteryStatus::Discharging;
};

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
  bool hasHidppSupport() const;

  auto subDeviceCount() const { return m_subDeviceConnections.size(); }
  bool hasSubDevice(const QString& path) const;
  void addSubDevice(std::shared_ptr<SubDeviceConnection>);
  bool removeSubDevice(const QString& path);
  const auto& subDevices() { return m_subDeviceConnections; }
  void queryBatteryStatus();
  auto getBatteryInfo(){ return m_batteryInfo; }

public slots:
  void setBatteryInfo(const QByteArray& batteryData);

signals:
  void subDeviceConnected(const DeviceId& id, const QString& path);
  void subDeviceDisconnected(const DeviceId& id, const QString& path);
  void subDeviceFlagsChanged(const DeviceId& id, const QString& path);

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

  Hidpp          = 1 << 15, ///< Device supports hidpp requests
  Vibrate        = 1 << 16, ///< Device supports vibrate commands
  ReportBattery  = 1 << 17, ///< Device can report battery status
  NextHold       = 1 << 18, ///< Device can be configured to send 'Next Hold' event.
  BackHold       = 1 << 19, ///< Device can be configured to send 'Back Hold' event.
  PointerSpeed   = 1 << 20, ///< Device allows changing pointer speed.
};
ENUM(DeviceFlag, DeviceFlags)

// -------------------------------------------------------------------------------------------------
struct SubDeviceConnectionDetails {
  SubDeviceConnectionDetails(const DeviceScan::SubDevice& sd, const DeviceId& id,
                             ConnectionType type, ConnectionMode mode);

  ConnectionType type;
  ConnectionMode mode;
  DeviceId parentDeviceID;
  bool grabbed = false;
  DeviceFlags deviceFlags = DeviceFlags::NoFlags;
  QString devicePath;
  float HIDppProtocolVer = -1;   // set after ping to HID sub-device; If positive then Hidraw device is online.
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
class SubDeviceConnection : public QObject, public async::Async<SubDeviceConnection>
{
  Q_OBJECT
public:
  virtual ~SubDeviceConnection() = 0;

  bool isConnected() const;
  void disconnect(); // destroys socket notifier and close file handle
  void setNotifiersEnabled(bool enabled); // enable/disable read and write socket notifiers
  void setReadNotifierEnabled(bool enabled); // disable/enable read socket notifier
  void setWriteNotifierEnabled(bool enabled); // disable/enable write socket notifier

  auto type() const { return m_details.type; };
  auto mode() const { return m_details.mode; };
  auto isGrabbed() const { return m_details.grabbed; };
  auto flags() const { return m_details.deviceFlags; };
  const auto& path() const { return m_details.devicePath; };

  inline bool hasFlags(DeviceFlags f) const { return ((flags() & f) == f); }

  const std::shared_ptr<InputMapper>& inputMapper() const;
  QSocketNotifier* socketReadNotifier();   // Read notifier for Hidraw and Event connections for receiving data from device
  QSocketNotifier* socketWriteNotifier();  // Write notifier for Hidraw connection for sending data to device

  // Hidraw specific command
  // Base implementation of generic write methods to the device does nothing.
  virtual ssize_t sendData(const QByteArray& msg);
  virtual ssize_t sendData(const void* msg, size_t msgLen);

  // HID++ specific functions: These commands write on device and expect some return message
  virtual bool isOnline() const { return false; };
  virtual void sendVibrateCommand(uint8_t intensity, uint8_t length);
  virtual void queryBatteryStatus();
  virtual float getHIDppProtocol() const { return -1; };
  virtual QString getFirmwareVersion() const { return ""; }

signals:
  void flagsChanged(DeviceFlags f);

protected:
  SubDeviceConnection(const DeviceScan::SubDevice& sd, const DeviceId& id,
                             ConnectionType type, ConnectionMode mode);
  DeviceFlags setFlags(DeviceFlags f, bool set = true);

  SubDeviceConnectionDetails m_details;
  std::shared_ptr<InputMapper> m_inputMapper; // shared input mapper from parent device.
  std::unique_ptr<QSocketNotifier> m_readNotifier;
  std::unique_ptr<QSocketNotifier> m_writeNotifier;   // only useful for Hidraw connections
};

// -------------------------------------------------------------------------------------------------
class SubEventConnection : public SubDeviceConnection
{
  Q_OBJECT
  class Token{};

public:
  static std::shared_ptr<SubEventConnection> create(const DeviceScan::SubDevice& sd,
                                                    const DeviceConnection& dc);

  SubEventConnection(Token, const DeviceScan::SubDevice& sd, const DeviceId& id);
  auto& inputBuffer() { return m_inputEventBuffer; }

protected:
  InputBuffer<12> m_inputEventBuffer;
};

// -------------------------------------------------------------------------------------------------
class SubHidrawConnection : public SubDeviceConnection
{
  Q_OBJECT

protected:
  class Token{};

public:
  static std::shared_ptr<SubHidrawConnection> create(const DeviceScan::SubDevice& sd,
                                                     const DeviceConnection& dc);

  SubHidrawConnection(Token, const DeviceScan::SubDevice& sd, const DeviceId& id);

  // Generic plain sendData implementation for hidraw devices.
  ssize_t sendData(const QByteArray& msg) override;
  ssize_t sendData(const void* msg, size_t msgLen) override;

protected:
  void createSocketNotifiers(int fd);
};

// -------------------------------------------------------------------------------------------------
class SubHidppConnection : public SubHidrawConnection
{
  Q_OBJECT

public:
  static std::shared_ptr<SubHidppConnection> create(const DeviceScan::SubDevice &sd,
                                                    const DeviceConnection &dc);

  SubHidppConnection(SubHidrawConnection::Token, const DeviceScan::SubDevice& sd, const DeviceId& id);
  ~SubHidppConnection();

  using SubHidrawConnection::sendData;

  /// sendData implementation for HIDPP devices
  ssize_t sendData(const QByteArray& msg) override;

  void queryBatteryStatus() override;
  void sendVibrateCommand(uint8_t intensity, uint8_t length) override;
  void pingSubDevice();
  void setPointerSpeed(uint8_t level);
  void setHIDppProtocol(float version);
  float getHIDppProtocol() const override { return m_details.HIDppProtocolVer; };
  bool isOnline() const override { return (m_details.HIDppProtocolVer > 0); };
  QString getFirmwareVersion() const override;


  void initialize();

  const HIDPP::FeatureSet* getFeatureSet();

signals:
  void receivedBatteryInfo(QByteArray batteryData);
  void activated();
  void deactivated();

private:
  std::unique_ptr<HIDPP::FeatureSet> m_featureSet;
};
