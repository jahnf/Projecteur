// This file is part of Projecteur - https://github.com/jahnf/projecteur - See LICENSE.md and README.md
#pragma once

#include <memory>

#include <QObject>
#include <QFile>

// Feature Codes important for Logitech Spotlight
enum class FeatureCode : uint16_t {
  Root                 = 0x0000,
  FeatureSet           = 0x0001,
  FirmwareVersion      = 0x0003,
  DeviceName           = 0x0005,
  Reset                = 0x0020,
  DFUControlSigned     = 0x00c2,
  BatteryStatus        = 0x1000,
  BatteryVoltage       = 0x1001,
  BatteryUnified       = 0x1004,
  PresenterControl     = 0x1a00,
  Sensor3D             = 0x1a01,
  ReprogramControlsV4  = 0x1b04,
  WirelessDeviceStatus = 0x1db4,
  SwapCancelButton     = 0x2005,
  PointerSpeed         = 0x2205,
};


class FeatureSet
{
public:
  FeatureSet() {};
  virtual ~FeatureSet() {}

  void setHIDDeviceFileDescriptor(int fd) { m_fHIDDevice = fd; };
  uint8_t getFeatureID(FeatureCode fc);
  bool supportFeatureCode(FeatureCode fc);
  auto getFeatureCount() { return m_featureTable.size(); }
  void populateFeatureTable();
  bool hasFeatureTable(){ return !(m_featureTable.empty()); }

protected:
  std::map<uint16_t, uint8_t> m_featureTable;
  int m_fHIDDevice=0;
};
