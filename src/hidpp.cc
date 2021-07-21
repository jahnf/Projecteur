// This file is part of Projecteur - https://github.com/jahnf/projecteur - See LICENSE.md and README.md
#include "hidpp.h"
#include "logging.h"

#include <unistd.h>

DECLARE_LOGGING_CATEGORY(hid)

// -------------------------------------------------------------------------------------------------
void FeatureSet::populateFeatureTable(){
  if (m_fHIDDevice) {
    auto getResponse = [this](QByteArray expectedBytes){
      QByteArray readVal(20, 0);
      while(true) {
        if(::read(m_fHIDDevice, readVal.data(), readVal.length())) {
          //logInfo(hid) << "Received" << readVal.toHex() << "Expected" << expectedBytes.toHex();
          if (readVal.mid(1, 3) == expectedBytes) return readVal;
          if (static_cast<uint8_t>(readVal.at(2)) == 0x8f) return readVal;  //Device not online
          if (errno != EAGAIN) return QByteArray(20, 0x8f);
        }
      }
    };
    // To get firmware details: first get Feature ID corresponding to Firmware feature code
    // and then make final request to get firmware version using the obtained feature ID
    uint8_t fwLSB = static_cast<uint8_t>(static_cast<uint16_t>(FeatureCode::FirmwareVersion) >> 8);   //0x00
    uint8_t fwMSB = static_cast<uint8_t>(static_cast<uint16_t>(FeatureCode::FirmwareVersion));        //0x03
    uint8_t fwIDReq[] = {0x11, 0x01, 0x00, 0x0d, fwLSB, fwMSB, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00};
    const QByteArray fwReqArr(reinterpret_cast<const char*>(fwIDReq), sizeof(fwIDReq));
    ::write(m_fHIDDevice, fwReqArr.data(), fwReqArr.length());
    auto response = getResponse(fwReqArr.mid(1, 3));
    if (static_cast<uint8_t>(response.at(2)) == 0x8f) return;
    uint8_t fwID = static_cast<uint8_t>(response.at(4));

    // Get the firmware version now
    uint8_t fwVerReq[] = {0x11, 0x01, fwID, 0x0d, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00};
    const QByteArray fwVerReqArr(reinterpret_cast<const char*>(fwVerReq), sizeof(fwVerReq));
    ::write(m_fHIDDevice, fwVerReqArr.data(), fwVerReqArr.length());
    auto fwResponse1 = getResponse(fwVerReqArr.mid(1, 3));
    if (static_cast<uint8_t>(fwResponse1.at(2)) == 0x8f) return;

    uint8_t fwVer2Req[] = {0x11, 0x01, fwID, 0x1d, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00};
    const QByteArray fwVer2ReqArr(reinterpret_cast<const char*>(fwVer2Req), sizeof(fwVer2Req));
    ::write(m_fHIDDevice, fwVer2ReqArr.data(), fwVer2ReqArr.length());
    auto fwResponse2 = getResponse(fwVer2ReqArr.mid(1, 3));
    if (static_cast<uint8_t>(fwResponse2.at(2)) == 0x8f) return;
    //TODO:: make sense of fwResponse1 and fwResponse2

    // TODO:: Read and write cache file
    // if the firmware details match with cached file; then load the FeatureTable from file
    // else read the entire feature table from the device


    // For reading feature table from device
    // first get the Feature Index for Feature Set
    uint8_t fSetLSB = static_cast<uint8_t>(static_cast<uint16_t>(FeatureCode::FeatureSet) >> 8);   //0x00
    uint8_t fSetMSB = static_cast<uint8_t>(static_cast<uint16_t>(FeatureCode::FeatureSet));        //0x01
    uint8_t featureSetIDReq[] = {0x11, 0x01, 0x00, 0x0d, fSetLSB, fSetMSB, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00};
    const QByteArray featureSetIDReqArr(reinterpret_cast<const char*>(featureSetIDReq), sizeof(featureSetIDReq));
    ::write(m_fHIDDevice, featureSetIDReqArr.data(), featureSetIDReqArr.length());
    response = getResponse(featureSetIDReqArr.mid(1, 3));
    if (static_cast<uint8_t>(response.at(2)) == 0x8f) return;
    uint8_t featureSetID = static_cast<uint8_t>(response.at(4));

    // Get Number of features (except Root Feature) supported
    uint8_t featureCountReq[] = {0x11, 0x01, featureSetID, 0x0d, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00};
    const QByteArray featureCountReqArr(reinterpret_cast<const char*>(featureCountReq), sizeof(featureCountReq));
    ::write(m_fHIDDevice, featureCountReqArr.data(), featureCountReqArr.length());
    response = getResponse(featureCountReqArr.mid(1, 3));
    if (static_cast<uint8_t>(response.at(2)) == 0x8f) return;
    uint8_t featureCount = static_cast<uint8_t>(response.at(4));

    // Root feature is supported by all HID++ 2.0 device and has a featureID of 0
    m_featureTable.insert({static_cast<uint16_t>(FeatureCode::Root), 0x00});

    // Read Feature Code for other featureIds from device
    for (uint8_t featureId = 0x01; featureId <= featureCount; featureId++) {
      const uint8_t data[] = {0x11, 0x01, featureSetID, 0x1d, featureId, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00};
      const QByteArray dataArr(reinterpret_cast<const char*>(data), sizeof(data));
      ::write(m_fHIDDevice, dataArr.data(), dataArr.length());
      response = getResponse(dataArr.mid(1, 3));
      if (static_cast<uint8_t>(response.at(2)) == 0x8f) {
        m_featureTable.clear();
        return;
      }
      uint16_t featureCode = (static_cast<uint16_t>(response.at(4)) << 8) | static_cast<uint8_t>(response.at(5));
      uint8_t featureType = static_cast<uint8_t>(response.at(6));
      auto softwareHidden = (featureType & (1<<6));
      auto obsoleteFeature = (featureType & (1<<7));
      if (!(softwareHidden) && !(obsoleteFeature)) {
        m_featureTable.insert({featureCode, featureId});
      }
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
