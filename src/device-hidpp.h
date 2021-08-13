// This file is part of Projecteur - https://github.com/jahnf/projecteur
// - See LICENSE.md and README.md
#pragma once

#include "device.h"
#include "hidpp.h"

#include <chrono>
#include <list>

class QTimer;

// -------------------------------------------------------------------------------------------------
/// @brief TODO
class SubHidppConnection : public SubHidrawConnection, public HidppConnectionInterface
{
  Q_OBJECT

public:
  enum class State : uint8_t {Uninitialized, Initializing, Initialized, Error};

  static std::shared_ptr<SubHidppConnection> create(const DeviceScan::SubDevice& sd,
                                                    const DeviceConnection& dc);

  SubHidppConnection(SubHidrawConnection::Token, const DeviceScan::SubDevice& sd,
                     const DeviceId& id);
  ~SubHidppConnection();

  using SubHidrawConnection::sendData;

  // --- HidppConnectionInterface implementation:

  BusType busType() const override { return m_busType; }
  ssize_t sendData(std::vector<uint8_t> msg) override;
  ssize_t sendData(HIDPP::Message msg) override;
  void sendData(std::vector<uint8_t> msg, SendResultCallback resultCb) override;
  void sendData(HIDPP::Message msg, SendResultCallback resultCb) override;
  void sendDataBatch(DataBatch dataBatch, DataBatchResultCallback cb,
                     bool continueOnError = false) override;
  void sendRequest(std::vector<uint8_t> msg, RequestResultCallback responseCb) override;
  void sendRequest(HIDPP::Message msg, RequestResultCallback responseCb) override;
  void sendRequestBatch(RequestBatch requestBatch, RequestBatchResultCallback cb,
                        bool continueOnError = false) override;

  // -------

  void queryBatteryStatus() override;
  void sendVibrateCommand(uint8_t intensity, uint8_t length) override;
  void pingSubDevice();
  void setPointerSpeed(uint8_t level);
  // void setHIDppProtocol(float version);
  // float getHIDppProtocol() const override { return HIDppProtocolVer; };
  // bool isOnline() const override { return (HIDppProtocolVer > 0); };

  void initialize();

signals:
  void receivedBatteryInfo(QByteArray batteryData);
  void activated();
  void deactivated();

private:
  void onHidppDataAvailable(int fd);
  void clearTimedOutRequests();
  void initUsbReceiver(std::function<void(MsgResult)>);

  void sendDataBatch(DataBatch requestBatch, DataBatchResultCallback cb, bool continueOnError,
                     std::vector<MsgResult> results);
  void sendRequestBatch(RequestBatch requestBatch, RequestBatchResultCallback cb,
                        bool continueOnError, std::vector<MsgResult> results);

  HIDPP::FeatureSet m_featureSet;
  const BusType m_busType = BusType::Unknown;

  struct RequestEntry {
    HIDPP::Message request; // bytes 0(or 1) to 5 should be enough to check against reply
    std::chrono::time_point<std::chrono::steady_clock> validUntil;
    RequestResultCallback callBack;
  };

  std::list<RequestEntry> m_requests;
  QTimer* m_requestCleanupTimer = nullptr;
};
