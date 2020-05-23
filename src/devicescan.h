// This file is part of Projecteur - https://github.com/jahnf/projecteur - See LICENSE.md and README.md
#pragma once

#include "device.h"

#include <QList>
#include <QString>
#include <QMetaType>

#include <tuple>

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

  /// Scan for supported devices and check if they are accessible
  ScanResult getDevices(const QList<SupportedDevice>& additionalDevices = {});
}
