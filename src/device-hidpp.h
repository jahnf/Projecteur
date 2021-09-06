// This file is part of Projecteur - https://github.com/jahnf/projecteur
// - See LICENSE.md and README.md
#pragma once

#include "device.h"
#include "hidpp.h"

#include <chrono>
#include <list>

class QTimer;

// -------------------------------------------------------------------------------------------------
/// Hid++ connection class
class SubHidppConnection : public SubHidrawConnection, public HidppConnectionInterface
{
  Q_OBJECT

public:
  /// Initialization state of the Usb dongle - for bluetooth this will be always initialized.
  enum class ReceiverState : uint8_t { Uninitialized, Initializing, Initialized, Error };
  /// Initialization state of the wireless presenter.
  /// * Uninitialized - no information had been collected and no defaults had been set up
  /// * Uninitialized_Offline - same as above, but online check detected offline device
  /// * Initializing  - currently fetching feature sets and setting defaults and other information
  /// * Initialized_Online  - device initialized and online
  /// * Initialized_Offline - device initialized but offline (only relevant when using usb dongle)
  /// * Error - An error occured during initialization.
  enum class PresenterState : uint8_t { Uninitialized, Uninitialized_Offline, Initializing,
                                        Initialized_Online, Initialized_Offline, Error };

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

  // ---

  PresenterState presenterState() const;
  ReceiverState receiverState() const;

  void sendPing(RequestResultCallback cb);
  HIDPP::ProtocolVersion protocolVersion() const;

  void queryBatteryStatus() override;
  void sendVibrateCommand(uint8_t intensity, uint8_t length) override;
  void setPointerSpeed(uint8_t level);

signals:
  void receiverStateChanged(ReceiverState);
  void presenterStateChanged(PresenterState);

  void receivedBatteryInfo(QByteArray batteryData);

private:
  void subDeviceInit();
  void initReceiver(std::function<void(ReceiverState)>);
  void initPresenter(std::function<void(PresenterState)>);

  void setReceiverState(ReceiverState rs);
  void setPresenterState(PresenterState ps);

  void onHidppDataAvailable(int fd);

  void getProtocolVersion(std::function<void(MsgResult, HIDPP::Error, HIDPP::ProtocolVersion)> cb);
  void checkPresenterOnline(std::function<void(bool, HIDPP::ProtocolVersion)> cb);
  void checkAndUpdatePresenterState(std::function<void(PresenterState)> cb);

  void clearTimedOutRequests();

  void sendDataBatch(DataBatch requestBatch, DataBatchResultCallback cb, bool continueOnError,
                     std::vector<MsgResult> results);
  void sendRequestBatch(RequestBatch requestBatch, RequestBatchResultCallback cb,
                        bool continueOnError, std::vector<MsgResult> results);

  HIDPP::FeatureSet m_featureSet;
  const BusType m_busType = BusType::Unknown;
  HIDPP::ProtocolVersion m_protocolVersion;

  ReceiverState m_receiverState = ReceiverState::Uninitialized;
  PresenterState m_presenterState = PresenterState::Uninitialized;

  /// A request entry for request messages sent to the device.
  struct RequestEntry {
    HIDPP::Message request; // bytes 0(or 1) to 5 should be enough to check against reply
    std::chrono::time_point<std::chrono::steady_clock> validUntil;
    RequestResultCallback callBack;
  };

  std::list<RequestEntry> m_requests;
  QTimer* m_requestCleanupTimer = nullptr;
};

const char* toString(SubHidppConnection::ReceiverState rs);
const char* toString(SubHidppConnection::PresenterState ps);
