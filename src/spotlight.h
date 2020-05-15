// This file is part of Projecteur - https://github.com/jahnf/projecteur - See LICENSE.md and README.md
#pragma once

#include <QObject>
#include <QSocketNotifier>

#include <memory>
#include <map>
#include <set>

class QSocketNotifier;
class QTimer;
class VirtualDevice;

/// Class to notify the application if the Logitech Spotlight and other supported devices
/// are connected and sending mouse move events. Used to turn the applications spot on or off.
class Spotlight : public QObject
{
  Q_OBJECT

public:
  struct SupportedDevice {
    quint16 vendorId;
    quint16 productId;
    bool isBluetooth = false;
    QString name = {};
  };

  struct Options {
    bool enableUInput = true; // enable virtual uinput device
    QList<Spotlight::SupportedDevice> additionalDevices;
  };

  explicit Spotlight(QObject* parent, Options options);
  virtual ~Spotlight();

  bool spotActive() const { return m_spotActive; }
  bool anySpotlightDeviceConnected() const;

  struct SubDevice {
    QString inputDeviceFile;
    QString hidrawDeviceFile;
    QString phys;
    bool hasRelativeEvents = false;
    bool inputDeviceReadable = false;
    bool inputDeviceWritable = false;
    bool hidrawDeviceReadable = false;
    bool hidrawDeviceWritable = false;
  };

  struct DeviceId {
    uint16_t vendorId = 0;
    uint16_t productId = 0;
    QString phys; // should be sufficient to differentiate between two devices of the same type
                  // - not tested, don't have two devices of any type currently.

    inline bool operator==(const DeviceId& rhs) const {
      return std::tie(vendorId, productId, phys) == std::tie(rhs.vendorId, rhs.productId, rhs.phys);
    }

    inline bool operator!=(const DeviceId& rhs) const {
      return std::tie(vendorId, productId, phys) != std::tie(rhs.vendorId, rhs.productId, rhs.phys);
    }

    inline bool operator<(const DeviceId& rhs) const {
      return std::tie(vendorId, productId, phys) < std::tie(rhs.vendorId, rhs.productId, rhs.phys);
    }
  };

  struct Device {
    enum class BusType : uint16_t { Unknown, Usb, Bluetooth };
    QString name;
    QString userName;
    DeviceId id;
    BusType busType = BusType::Unknown;
    QList<SubDevice> subDevices;
  };

  struct ScanResult {
    QList<Device> devices;
    quint16 numDevicesReadable = 0;
    quint16 numDevicesWritable = 0;
    QStringList errorMessages;
  };

  uint32_t connectedDeviceCount() const;
  std::set<DeviceId> connectedDevices() const;

  /// scan for supported devices and check if they are accessible
  static ScanResult scanForDevices(const QList<SupportedDevice>& additionalDevices = {});

signals:
  void error(const QString& errMsg);
  void deviceConnected(const DeviceId& id, const QString& name);
  void deviceDisconnected(const DeviceId& id, const QString& name);
  void subDeviceConnected(const DeviceId& id, const QString& name, const QString& path);
  void subDeviceDisconnected(const DeviceId& id, const QString& name, const QString& path);
  void anySpotlightDeviceConnectedChanged(bool connected);
  void spotActiveChanged(bool isActive);

private:
  enum class ConnectionResult { CouldNotOpen, NotASpotlightDevice, Connected };
  ConnectionResult connectSpotlightDevice(const QString& devicePath, bool verbose = false);

  enum class ConnectionType : uint8_t { Event, Hidraw };
  enum class ConnectionMode : uint8_t { ReadOnly, WriteOnly, ReadWrite };
  struct ConnectionInfo {
    ConnectionInfo() = default;
    ConnectionInfo(const QString& path, ConnectionType type, ConnectionMode mode)
      : type(type), mode(mode), devicePath(path) {}

    uint16_t vendorId = 0;
    uint16_t productId = 0;
    ConnectionType type;
    ConnectionMode mode;
    bool grabbed = false;
    QString devicePath;
  };

  struct DeviceConnection
  {
    DeviceConnection() = default;
    DeviceConnection(const QString& path, ConnectionType type, ConnectionMode mode) : info(path, type, mode) {}
    ConnectionInfo info;
    std::unique_ptr<QSocketNotifier> notifier;
  };

  using DevicePath = QString;
  using ConnectionMap = std::map<DevicePath, DeviceConnection>;
  struct ConnectionDetails {
    DeviceId deviceId;
    QString deviceName;
    ConnectionMap map;
  };

  DeviceConnection openEventDevice(const QString& devicePath, const Device& dev);
  void addInputEventHandler(ConnectionDetails&, DeviceConnection& connection);

  bool setupDevEventInotify();
  int connectDevices();
  void removeDeviceConnection(const QString& devicePath);

  const Options m_options;
  std::map<DeviceId, ConnectionDetails> m_deviceConnections;

  QTimer* m_activeTimer = nullptr;
  QTimer* m_connectionTimer = nullptr;
  bool m_spotActive = false;
  std::shared_ptr<VirtualDevice> m_virtualDevice;
};
