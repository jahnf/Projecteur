// This file is part of Projecteur - https://github.com/jahnf/projecteur - See LICENSE.md and README.md
#pragma once

#include <QObject>
#include <QSocketNotifier>

#include <memory>
#include <map>
#include <set>

#include "enum-helper.h"

class InputMapper;
class QSocketNotifier;
class QTimer;
class VirtualDevice;

enum class DeviceFlag : uint32_t {
  NoFlags = 0,
  NonBlocking    = 1 << 0,
  SynEvents      = 1 << 1,
  RepEvents      = 1 << 2,
  RelativeEvents = 1 << 3,
};
ENUM(DeviceFlag, DeviceFlags)

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

  struct ConnectedDeviceInfo {
    DeviceId id;
    QString name;
  };

  uint32_t connectedDeviceCount() const;
  QList<ConnectedDeviceInfo> connectedDevices() const;

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

    ConnectionType type;
    ConnectionMode mode;
    bool grabbed = false;
    DeviceFlags deviceFlags = DeviceFlags::NoFlags;
    QString devicePath;
  };

  struct DeviceConnection;
  struct ConnectionDetails;
  using DevicePath = QString;
  using ConnectionMap = std::map<DevicePath, std::shared_ptr<DeviceConnection>>;

  std::shared_ptr<DeviceConnection> openEventDevice(const QString& devicePath, const Device& dev);
  bool addInputEventHandler(std::shared_ptr<DeviceConnection> connection);

  bool setupDevEventInotify();
  int connectDevices();
  void removeDeviceConnection(const QString& devicePath);
  void onDeviceDataAvailable(int fd, DeviceConnection& connection);

  const Options m_options;
  std::map<DeviceId, std::unique_ptr<ConnectionDetails>> m_deviceConnections;

  QTimer* m_activeTimer = nullptr;
  QTimer* m_connectionTimer = nullptr;
  bool m_spotActive = false;
  std::shared_ptr<VirtualDevice> m_virtualDevice;
};

Q_DECLARE_METATYPE(Spotlight::DeviceId);
