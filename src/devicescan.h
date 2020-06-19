// This file is part of Projecteur - https://github.com/jahnf/projecteur - See LICENSE.md and README.md
#pragma once

#include "device.h"

#include <QString>
#include <QMetaType>

#include <tuple>
#include <vector>

// -------------------------------------------------------------------------------------------------
struct SupportedDevice
{
  quint16 vendorId;
  quint16 productId;
  bool isBluetooth = false;
  QString name = {};
};

// -------------------------------------------------------------------------------------------------
namespace DeviceScan
{
  struct SubDevice { // Structure for device scan results
    enum class Type : uint8_t { Unknown, Event, Hidraw };
    QString deviceFile;
    QString phys;
    Type type = Type::Unknown;
    bool hasRelativeEvents = false;
    bool deviceReadable = false;
    bool deviceWritable = false;
  };

  struct Device { // Structure for device scan results
    enum class BusType : uint16_t { Unknown, Usb, Bluetooth };
    const QString& getName() const { return userName.size() ? userName : name; }
    QString name;
    QString userName;
    DeviceId id;
    BusType busType = BusType::Unknown;
    std::vector<SubDevice> subDevices;
  };

  struct ScanResult {
    std::vector<Device> devices;
    quint16 numDevicesReadable = 0;
    quint16 numDevicesWritable = 0;
    QStringList errorMessages;
  };

  /// Scan for supported devices and check if they are accessible
  ScanResult getDevices(const std::vector<SupportedDevice>& additionalDevices = {});
}
