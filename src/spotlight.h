// This file is part of Projecteur - https://github.com/jahnf/projecteur - See LICENSE.md and README.md
#pragma once

#include <QObject>

#include <memory>
#include <map>

class QSocketNotifier;
class QTimer;
class VirtualDevice;

/// Simple class to notify the application if the Logitech Spotlight and other supported devices
/// are sending mouse move events. Used to turn the applications spot on or off.
class Spotlight : public QObject
{
  Q_OBJECT

public:
  struct SupportedDevice {
    quint16 vendorId;
    quint16 productId;
    bool isBluetooth = false;
  };

  struct Options {
    bool enableUInput = true; // enable virtual uinput device
    QList<Spotlight::SupportedDevice> additionalDevices;
  };

  explicit Spotlight(QObject* parent, Options options);
  virtual ~Spotlight();

  bool spotActive() const { return m_spotActive; }
  bool anySpotlightDeviceConnected() const;
  const VirtualDevice* virtualDevice() const;
  QStringList connectedDevices() const;

  struct Device {
    enum class BusType { Unknown, Usb, Bluetooth };
    QString name;
    quint16 vendorId = 0;
    quint16 productId = 0;
    BusType busType = BusType::Unknown;
    QString phys;
    QString inputDeviceFile;
    bool inputDeviceReadable = false;
    bool inputDeviceWritable = false;
  };

  struct ScanResult {
    QList<Device> devices;
    quint16 numDevicesReadable = 0;
    quint16 numDevicesWritable = 0;
    QStringList errorMessages;
  };

  /// scan for supported devices and check if they are accessible
  static ScanResult scanForDevices(const QList<SupportedDevice>& additionalDevices = {});

signals:
  void error(const QString& errMsg);
  void connected(const QString& devicePath); //!< signal for every device connected
  void disconnected(const QString& devicePath); //!< signal for every device disconnected
  void anySpotlightDeviceConnectedChanged(bool connected);
  void spotActiveChanged(bool isActive);

private:
  enum class ConnectionResult { CouldNotOpen, NotASpotlightDevice, Connected };
  ConnectionResult connectSpotlightDevice(const QString& devicePath);
  bool setupDevEventInotify();
  int connectDevices();
  void tryConnect(const QString& devicePath, int msec, int retries);

private:
  const Options m_options;
  std::map<QString, QScopedPointer<QSocketNotifier>> m_eventNotifiers;
  QTimer* m_activeTimer = nullptr;
  bool m_spotActive = false;
  std::unique_ptr<VirtualDevice> m_virtualDevice;
};
