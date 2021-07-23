// This file is part of Projecteur - https://github.com/jahnf/projecteur - See LICENSE.md and README.md
#include "hidpp.h"
#include "logging.h"

#include <unistd.h>

#include <QTime>

DECLARE_LOGGING_CATEGORY(hid)

// -------------------------------------------------------------------------------------------------
QByteArray FeatureSet::getResponseFromDevice(QByteArray expectedBytes)
{
  if (m_fdHIDDevice == -1) return QByteArray();

  QByteArray readVal(20, 0);
  int timeOut = 4; // time out just in case device did not reply;
                   // 4 seconds time out is used by other prorams like Solaar.
  QTime timeOutTime = QTime::currentTime().addSecs(timeOut);
  while(true) {
    if(::read(m_fdHIDDevice, readVal.data(), readVal.length())) {
      if (readVal.mid(1, 3) == expectedBytes) return readVal;
      if (static_cast<uint8_t>(readVal.at(2)) == 0x8f) return readVal;  //Device not online
      if (QTime::currentTime() >= timeOutTime) return QByteArray();
    }
  }
}

// -------------------------------------------------------------------------------------------------
uint8_t FeatureSet::getFeatureIDFromDevice(FeatureCode fc)
{
  if (m_fdHIDDevice == -1) return 0x00;

  uint8_t fSetLSB = static_cast<uint8_t>(static_cast<uint16_t>(fc) >> 8);
  uint8_t fSetMSB = static_cast<uint8_t>(static_cast<uint16_t>(fc));
  uint8_t featureIDReq[] = {HIDPP_LONG_MSG, MSG_TO_SPOTLIGHT, 0x00, 0x0d, fSetLSB, fSetMSB, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00};
  const QByteArray featureIDReqArr(reinterpret_cast<const char*>(featureIDReq), sizeof(featureIDReq));
  ::write(m_fdHIDDevice, featureIDReqArr.data(), featureIDReqArr.length());

  auto response = getResponseFromDevice(featureIDReqArr.mid(1, 3));
  if (!response.length() || static_cast<uint8_t>(response.at(2)) == 0x8f) return 0x00;
  uint8_t featureID = static_cast<uint8_t>(response.at(4));

  return featureID;
}

// -------------------------------------------------------------------------------------------------
uint8_t FeatureSet::getFeatureCountFromDevice(uint8_t featureSetID)
{
  if (m_fdHIDDevice == -1) return 0x00;

  // Get Number of features (except Root Feature) supported
  uint8_t featureCountReq[] = {HIDPP_LONG_MSG, MSG_TO_SPOTLIGHT, featureSetID, 0x0d, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00};
  const QByteArray featureCountReqArr(reinterpret_cast<const char*>(featureCountReq), sizeof(featureCountReq));
  ::write(m_fdHIDDevice, featureCountReqArr.data(), featureCountReqArr.length());
  auto response = getResponseFromDevice(featureCountReqArr.mid(1, 3));
  if (!response.length() || static_cast<uint8_t>(response.at(2)) == 0x8f) return 0x00;
  uint8_t featureCount = static_cast<uint8_t>(response.at(4));

  return featureCount;
}

// -------------------------------------------------------------------------------------------------
QByteArray FeatureSet::getFirmwareVersionFromDevice()
{
  if (m_fdHIDDevice == -1) return 0x00;

  // To get firmware details: first get Feature ID corresponding to Firmware feature code
  uint8_t fwID = getFeatureIDFromDevice(FeatureCode::FirmwareVersion);
  if (!fwID) return QByteArray();

  // Get the number of firmwares (Main HID++ application, BootLoader, or Hardware) now
  uint8_t fwCountReq[] = {HIDPP_LONG_MSG, MSG_TO_SPOTLIGHT, fwID, 0x0d, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00};
  const QByteArray fwCountReqArr(reinterpret_cast<const char*>(fwCountReq), sizeof(fwCountReq));
  ::write(m_fdHIDDevice, fwCountReqArr.data(), fwCountReqArr.length());
  auto response = getResponseFromDevice(fwCountReqArr.mid(1, 3));
  if (!response.length() || static_cast<uint8_t>(response.at(2)) == 0x8f) return  QByteArray();
  uint8_t fwCount = static_cast<uint8_t>(response.at(4));

  // The following info is not used currently; however, these commented lines are kept for future reference.
  // uint8_t connectionMode = static_cast<uint8_t>(response.at(10));
  // bool supportBluetooth = (connectionMode & 0x01);
  // bool supportBluetoothLE = (connectionMode & 0x02);  // true for Logitech Spotlight
  // bool supportUsbReceiver = (connectionMode & 0x04);  // true for Logitech Spotlight
  // bool supportUsbWired = (connectionMode & 0x08);
  // auto unitID = response.mid(5, 4);
  // auto modelIDs = response.mid(11, 8);
  // int count = 0;
  // if (supportBluetooth) { auto btmodelID = modelIDs.mid(count, 2); count += 2;}
  // if (supportBluetoothLE) { auto btlemodelID = modelIDs.mid(count, 2); count += 2;}
  // if (supportUsbReceiver) { auto wpmodelID = modelIDs.mid(count, 2); count += 2;}
  // if (supportUsbWired) { auto usbmodelID = modelIDs.mid(count, 2); count += 2;}


  // Iteratively find out firmware version for all firmwares and get the firmware for main application
  for (uint8_t i = 0x00; i < fwCount; i++)
  {
    uint8_t fwVerReq[] = {HIDPP_LONG_MSG, MSG_TO_SPOTLIGHT, fwID, 0x1d, i, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00};
    const QByteArray fwVerReqArr(reinterpret_cast<const char*>(fwVerReq), sizeof(fwVerReq));
    ::write(m_fdHIDDevice, fwVerReqArr.data(), fwVerReqArr.length());
    auto fwResponse = getResponseFromDevice(fwVerReqArr.mid(1, 3));
    if (!fwResponse.length() || static_cast<uint8_t>(fwResponse.at(2)) == 0x8f) return QByteArray();
    auto fwType = (fwResponse.at(4) & 0x0f);  // 0 for main HID++ application, 1 for BootLoader, 2 for Hardware, 3-15 others
    auto fwVersion = fwResponse.mid(5, 7);
    // Currently we are not interested in these details; however, these commented lines are kept for future reference.
    //auto firmwareName = fwVersion.mid(0, 3).data();
    //auto majorVesion = fwResponse.at(3);
    //auto MinorVersion = fwResponse.at(4);
    //auto build = fwResponse.mid(5);
    if (fwType == 0)
    {
      logDebug(hid) << "Main application firmware Version:" << fwVersion.toHex();
      return fwVersion;
    }
  }
  return QByteArray();
}

// -------------------------------------------------------------------------------------------------
void FeatureSet::populateFeatureTable()
{
  if (m_fdHIDDevice == -1) return;

  // Get the firmware version
  auto firmwareVersion = getFirmwareVersionFromDevice();
  if (!firmwareVersion.length()) return;

  // TODO:: Read and write cache file (settings most probably)
  // if the firmware details match with cached file; then load the FeatureTable from file
  // else read the entire feature table from the device
  QByteArray cacheFirmwareVersion;  // currently a dummy variable for Firmware Version from cache file.

  if (firmwareVersion == cacheFirmwareVersion)
  {
    // TODO: load the featureSet from the cache file

  } else {
    // For reading feature table from device
    // first get featureID for FeatureCode::FeatureSet
    // then we can get the number of features supported by the device (except Root Feature)
    uint8_t featureSetID = getFeatureIDFromDevice(FeatureCode::FeatureSet);
    if (!featureSetID) return;
    uint8_t featureCount = getFeatureCountFromDevice(featureSetID);
    if (!featureCount) return;

    // Root feature is supported by all HID++ 2.0 device and has a featureID of 0 always.
    m_featureTable.insert({static_cast<uint16_t>(FeatureCode::Root), 0x00});

    // Read Feature Code for other featureIds from device.
    for (uint8_t featureId = 0x01; featureId <= featureCount; featureId++) {
      const uint8_t data[] = {HIDPP_LONG_MSG, MSG_TO_SPOTLIGHT, featureSetID, 0x1d, featureId, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00};
      const QByteArray dataArr(reinterpret_cast<const char*>(data), sizeof(data));
      ::write(m_fdHIDDevice, dataArr.data(), dataArr.length());
      auto response = getResponseFromDevice(dataArr.mid(1, 3));
      if (!response.length() || static_cast<uint8_t>(response.at(2)) == 0x8f) {
        m_featureTable.clear();
        return;
      }
      uint16_t featureCode = (static_cast<uint16_t>(response.at(4)) << 8) | static_cast<uint8_t>(response.at(5));
      uint8_t featureType = static_cast<uint8_t>(response.at(6));
      auto softwareHidden = (featureType & (1<<6));
      auto obsoleteFeature = (featureType & (1<<7));
      if (!(softwareHidden) && !(obsoleteFeature)) m_featureTable.insert({featureCode, featureId});
    }
  }
}

// -------------------------------------------------------------------------------------------------
bool FeatureSet::supportFeatureCode(FeatureCode fc)
{
  auto featurePair = m_featureTable.find(static_cast<uint16_t>(fc));
  return (featurePair != m_featureTable.end());
}

// -------------------------------------------------------------------------------------------------
uint8_t FeatureSet::getFeatureID(FeatureCode fc)
{
  if (!supportFeatureCode(fc)) return 0x00;

  auto featurePair = m_featureTable.find(static_cast<uint16_t>(fc));
  return featurePair->second;
}

// -------------------------------------------------------------------------------------------------
QByteArray HIDPP::shortToLongMsg(QByteArray shortMsg)
{
  bool isValidShortMsg = (shortMsg.at(0) == HIDPP_SHORT_MSG && shortMsg.length() == 7);

  if (isValidShortMsg) {
    QByteArray longMsg;
    longMsg.append(HIDPP_LONG_MSG);
    longMsg.append(shortMsg.mid(1));
    QByteArray padding(20 - longMsg.length(), 0);
    longMsg.append(padding);
    return longMsg;
  } else return shortMsg;
}
