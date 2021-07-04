// This file is part of Projecteur - https://github.com/jahnf/projecteur - See LICENSE.md and README.md
#include "deviceswidget.h"

#include "device-vibration.h"
#include "deviceinput.h"
#include "iconwidgets.h"
#include "inputmapconfig.h"
#include "logging.h"
#include "settings.h"
#include "spotlight.h"

#include <QComboBox>
#include <QLabel>
#include <QLayout>
#include <QShortcut>
#include <QSpinBox>
#include <QStackedLayout>
#include <QStyle>
#include <QTabWidget>
#include <QTimer>
#include <QTextEdit>

DECLARE_LOGGING_CATEGORY(preferences)

// -------------------------------------------------------------------------------------------------
namespace {
  const auto hexId = logging::hexId;

  QString descriptionString(const QString& name, const DeviceId& id) {
    return QString("%1 (%2:%3) [%4]").arg(name, hexId(id.vendorId), hexId(id.productId), id.phys);
  }

  const auto invalidDeviceId = DeviceId(); // vendorId = 0, productId = 0

  bool removeTab(QTabWidget* tabWidget, QWidget* widget)
  {
    const auto idx = tabWidget->indexOf(widget);
    if (idx >= 0) {
      tabWidget->removeTab(idx);
      return true;
    }
    return false;
  }
}

// -------------------------------------------------------------------------------------------------
DevicesWidget::DevicesWidget(Settings* settings, Spotlight* spotlight, QWidget* parent)
  : QWidget(parent)
  , m_updateDeviceDetailsTimer(new QTimer(this))
{
  createDeviceComboBox(spotlight);

  const auto stackLayout = new QStackedLayout(this);
  const auto disconnectedWidget = createDisconnectedStateWidget();
  stackLayout->addWidget(disconnectedWidget);
  const auto deviceWidget = createDevicesWidget(settings, spotlight);
  stackLayout->addWidget(deviceWidget);

  const bool anyDeviceConnected = spotlight->anySpotlightDeviceConnected();
  stackLayout->setCurrentWidget(anyDeviceConnected ? deviceWidget : disconnectedWidget);

  connect(spotlight, &Spotlight::anySpotlightDeviceConnectedChanged, this,
  [stackLayout, deviceWidget, disconnectedWidget](bool anyConnected){
    stackLayout->setCurrentWidget(anyConnected ? deviceWidget : disconnectedWidget);
  });
}

// -------------------------------------------------------------------------------------------------
const DeviceId DevicesWidget::currentDeviceId() const
{
  if (m_devicesCombo->currentIndex() < 0)
    return invalidDeviceId;

  return qvariant_cast<DeviceId>(m_devicesCombo->currentData());
}

// -------------------------------------------------------------------------------------------------
TimerTabWidget* DevicesWidget::createTimerTabWidget(Settings* settings, Spotlight* spotlight)
{
  Q_UNUSED(spotlight);
  const auto w = new TimerTabWidget(settings, this);
  w->loadSettings(currentDeviceId());

  connect(this, &DevicesWidget::currentDeviceChanged, this, [this](const DeviceId& dId) {
    if (m_timerTabWidget) { m_timerTabWidget->loadSettings(dId); }
  });

  return w;
}

// -------------------------------------------------------------------------------------------------
QWidget* DevicesWidget::createDevicesWidget(Settings* settings, Spotlight* spotlight)
{
  const auto dw = new QWidget(this);
  const auto vLayout = new QVBoxLayout(dw);
  const auto devHLayout = new QHBoxLayout();
  vLayout->addLayout(devHLayout);

  devHLayout->addWidget(new QLabel(tr("Device"), dw));
  devHLayout->addWidget(m_devicesCombo);
  devHLayout->setStretch(1, 1);

  vLayout->addSpacing(10);

  m_tabWidget = new QTabWidget(dw);
  vLayout->addWidget(m_tabWidget);

  m_tabWidget->addTab(createInputMapperWidget(settings, spotlight), tr("Input Mapping"));
  m_timerTabWidget = createTimerTabWidget(settings, spotlight);

  updateTimerTab(spotlight);

  m_deviceDetailsTabWidget = createDeviceInfoWidget(spotlight);
  m_tabWidget->addTab(m_deviceDetailsTabWidget, tr("Details"));

  // Update the timer tab when the current device has changed
  connect(this, &DevicesWidget::currentDeviceChanged, this,
  [spotlight, this]() { updateTimerTab(spotlight); });

  return dw;
}

// -------------------------------------------------------------------------------------------------
void DevicesWidget::updateDeviceDetails(Spotlight* spotlight)
{
  auto updateBatteryInfo = [this, spotlight]() {
    auto curDeviceId = currentDeviceId();
    if (curDeviceId == invalidDeviceId)
      return;
    auto dc = spotlight->deviceConnection(curDeviceId);
    dc->queryBatteryStatus();
  };

  auto getDeviceDetails = [this, spotlight]() {
    QString deviceDetails;
    auto curDeviceId = currentDeviceId();
    if (curDeviceId == invalidDeviceId)
      return tr("No Device Connected");
    auto dc = spotlight->deviceConnection(curDeviceId);

    const auto busTypeToString = [](BusType type) -> QString {
      if (type == BusType::Usb) return "USB";
      if (type == BusType::Bluetooth) return "Bluetooth";
      return "Unknown";
    };

    const QStringList subDeviceList = [dc](){
      QStringList subDeviceList;
      auto accessText = [](ConnectionMode m){
        if (m == ConnectionMode::ReadOnly) return "ReadOnly";
        if (m == ConnectionMode::WriteOnly) return "WriteOnly";
        if (m == ConnectionMode::ReadWrite) return "ReadWrite";
        return "Unknown Access";
      };
      // report special flags set by program (like vibration and others)
      auto flagText = [](DeviceFlag f){
        QStringList flagList;
        if (!!(f & DeviceFlag::Hidpp)) flagList.push_back("HID++");
        if (!!(f & DeviceFlag::Vibrate)) flagList.push_back("Vibration");
        if (!!(f & DeviceFlag::ReportBattery)) flagList.push_back("Report_Battery");
        if (!!(f & DeviceFlag::NextHold)) flagList.push_back("Next_Hold");
        if (!!(f & DeviceFlag::BackHold)) flagList.push_back("Back_Hold");
        return flagList;
      };
      for (const auto& sd: dc->subDevices()) {
        if (sd.second->path().size()) {
          auto sds = sd.second;
          auto flagInfo = flagText(sds->flags());
          subDeviceList.push_back(tr("%1\t[%2, %3, %4]").arg(sds->path(),
                                                     accessText(sds->mode()),
                                                     sds->isGrabbed()?"Grabbed":"",
                                                     flagInfo.isEmpty()?"":"Supports: " + flagInfo.join("; ")
                                                     ));
        }
      }
      return subDeviceList;
    }();
    auto batteryStatusText = [](BatteryStatus d){
      if (d == BatteryStatus::Discharging) return "Discharging";
      if (d == BatteryStatus::Charging) return "Charging";
      if (d == BatteryStatus::AlmostFull) return "Almost Full";
      if (d == BatteryStatus::Full) return "Full Charge";
      if (d == BatteryStatus::SlowCharging) return "Slow Charging";
      if (d == BatteryStatus::InvalidBattery || d == BatteryStatus::ThermalError || d == BatteryStatus::ChargingError) {
        return "Charging Error";
      };
      return "";
    };

    auto sDevices = dc->subDevices();
    auto batteryInfoText = [dc, batteryStatusText, sDevices](){
      const bool isOnline = std::any_of(sDevices.cbegin(), sDevices.cend(),
                                            [](const auto& sd){
            return (sd.second->type() == ConnectionType::Hidraw &&
                    sd.second->mode() == ConnectionMode::ReadWrite &&
                    sd.second->isOnline());});
      if (isOnline) {
        auto batteryInfo= dc->getBatteryInfo();
        // Only show battery percent while discharging.
        // Other cases, device do not report battery percentage correctly.
        if (batteryInfo.status == BatteryStatus::Discharging) {
          return tr("%1\% - %2% (%3)").arg(
                      QString::number(batteryInfo.currentLevel),
                      QString::number(batteryInfo.nextReportedLevel),
                      batteryStatusText(batteryInfo.status));
        } else {
          return tr("%3").arg(batteryStatusText(batteryInfo.status));
        }
      } else {
        return tr("Device not active. Press any key on device to update.");
      }
    };
    const bool hasBattery = std::any_of(sDevices.cbegin(), sDevices.cend(), [](const auto& sd)
    {
      return (sd.second->type() == ConnectionType::Hidraw &&
              sd.second->mode() == ConnectionMode::ReadWrite &&
              sd.second->hasFlags(DeviceFlag::ReportBattery));
    });

    deviceDetails += tr("Name:\t\t%1\n").arg(dc->deviceName());
    deviceDetails += tr("VendorId:\t%1\n").arg(hexId(dc->deviceId().vendorId));
    deviceDetails += tr("ProductId:\t%1\n").arg(hexId(dc->deviceId().productId));
    deviceDetails += tr("Phys:\t\t%1\n").arg(dc->deviceId().phys);
    deviceDetails += tr("Bus Type:\t%1\n").arg(busTypeToString(dc->deviceId().busType));
    deviceDetails += tr("Sub-Devices:\t%1\n").arg(subDeviceList.join(",\n\t\t"));
    if (hasBattery) deviceDetails += tr("Battery Status:\t%1\n").arg(batteryInfoText());

    return deviceDetails;
  };

  QTimer::singleShot(200, this, [updateBatteryInfo](){updateBatteryInfo();});
  if (m_deviceDetailsTextEdit) {
    QTimer::singleShot(1000, this, [this, getDeviceDetails](){m_deviceDetailsTextEdit->setText(getDeviceDetails());});
  }
}

// -------------------------------------------------------------------------------------------------
QWidget* DevicesWidget::createDeviceInfoWidget(Spotlight* spotlight)
{
  const auto diWidget = new QWidget(this);
  const auto layout = new QHBoxLayout(diWidget);
  if (!m_deviceDetailsTextEdit) m_deviceDetailsTextEdit = new QTextEdit(this);
  m_deviceDetailsTextEdit->setReadOnly(true);
  m_deviceDetailsTextEdit->setText("");

  updateDeviceDetails(spotlight);

  connect(m_updateDeviceDetailsTimer, &QTimer::timeout, this, [this, spotlight](){updateDeviceDetails(spotlight);});
  m_updateDeviceDetailsTimer->start(900000);  // Update every 15 minutes

  connect(this, &DevicesWidget::currentDeviceChanged, this, [this, spotlight](){updateDeviceDetails(spotlight);});
  connect(spotlight, &Spotlight::deviceActivated, this,
          [this, spotlight](const DeviceId& d){if (d==currentDeviceId()){ updateDeviceDetails(spotlight);};});

  layout->addWidget(m_deviceDetailsTextEdit);
  return diWidget;
}

// -------------------------------------------------------------------------------------------------
QWidget* DevicesWidget::createInputMapperWidget(Settings* settings, Spotlight* /*spotlight*/)
{
  const auto delShortcut = new QShortcut( QKeySequence(Qt::ShiftModifier + Qt::Key_Delete), this);

  const auto imWidget = new QWidget(this);
  const auto layout = new QVBoxLayout(imWidget);
  const auto intervalLayout = new QHBoxLayout();

  const auto addBtn = new IconButton(Font::Icon::plus_5, imWidget);
  addBtn->setToolTip(tr("Add a new input mapping entry."));
  const auto delBtn = new IconButton(Font::Icon::trash_can_1, imWidget);
  delBtn->setToolTip(tr("Delete the selected input mapping entries (%1).", "%1=shortcut")
                       .arg(delShortcut->key().toString()));
  delBtn->setEnabled(false);

  const auto intervalLbl = new QLabel(tr("Input Sequence Interval"), imWidget);
  const auto intervalSb = new QSpinBox(this);
  const auto intervalUnitLbl = new QLabel(tr("ms"), imWidget);
  intervalSb->setMaximum(settings->inputSequenceIntervalRange().max);
  intervalSb->setMinimum(settings->inputSequenceIntervalRange().min);
  intervalSb->setValue(m_inputMapper ? m_inputMapper->keyEventInterval()
                                     : settings->deviceInputSeqInterval(currentDeviceId()));
  intervalSb->setSingleStep(50);

  intervalLayout->addWidget(addBtn);
  intervalLayout->addWidget(delBtn);
  intervalLayout->addStretch(1);
  intervalLayout->addWidget(intervalLbl);
  intervalLayout->addWidget(intervalSb);
  intervalLayout->addWidget(intervalUnitLbl);

  const auto tblView = new InputMapConfigView(imWidget);
  const auto imModel = new InputMapConfigModel(m_inputMapper, imWidget);
  if (m_inputMapper) imModel->setConfiguration(m_inputMapper->configuration());

  tblView->setModel(imModel);
  const auto selectionModel = tblView->selectionModel();

  auto updateImWidget = [this, imWidget]() {
    imWidget->setDisabled(!m_inputMapper || !m_inputMapper->hasVirtualDevice());
  };
  updateImWidget();

  connect(this, &DevicesWidget::currentDeviceChanged, this,
  [this, imModel, intervalSb, updateImWidget=std::move(updateImWidget)](){
    imModel->setInputMapper(m_inputMapper);
    if (m_inputMapper) {
      intervalSb->setValue(m_inputMapper->keyEventInterval());
      imModel->setConfiguration(m_inputMapper->configuration());
    }
    updateImWidget();
  });

  connect(intervalSb, static_cast<void (QSpinBox::*)(int)>(&QSpinBox::valueChanged),
  this, [this, settings](int valueMs) {
    if (m_inputMapper) {
      m_inputMapper->setKeyEventInterval(valueMs);
      settings->setDeviceInputSeqInterval(currentDeviceId(), valueMs);
    }
  });

  connect(selectionModel, &QItemSelectionModel::selectionChanged, this,
  [delBtn, selectionModel](){
    delBtn->setEnabled(selectionModel->hasSelection());
  });

  auto removeCurrentSelection = [imModel, selectionModel](){
    const auto selectedRows = selectionModel->selectedRows();
    std::vector<int> rows;
    rows.reserve(selectedRows.size());
    for (const auto& selectedRow : selectedRows) {
      rows.emplace_back(selectedRow.row());
    }
    imModel->removeConfigItemRows(std::move(rows));
  };

  connect(delBtn, &QToolButton::clicked, this, removeCurrentSelection);
  // --- Delete selected items on Shift + Delete
  connect(delShortcut, &QShortcut::activated, this, std::move(removeCurrentSelection));

  connect(addBtn, &QToolButton::clicked, this, [imModel, tblView](){
    tblView->selectRow(imModel->addNewItem(std::make_shared<KeySequenceAction>()));
  });

  layout->addLayout(intervalLayout);
  layout->addWidget(tblView);
  return imWidget;
}

// -------------------------------------------------------------------------------------------------
void DevicesWidget::createDeviceComboBox(Spotlight* spotlight)
{
  m_devicesCombo = new QComboBox(this);
  m_devicesCombo->setToolTip(tr("List of connected devices."));

  for (const auto& dev : spotlight->connectedDevices()) {
    const auto data = QVariant::fromValue(dev.id);
    if (m_devicesCombo->findData(data) < 0) {
      m_devicesCombo->addItem(descriptionString(dev.name, dev.id), data);
    }
  }

  connect(spotlight, &Spotlight::deviceDisconnected, this,
  [this](const DeviceId& id, const QString& /*name*/)
  {
    const auto idx = m_devicesCombo->findData(QVariant::fromValue(id));
    if (idx >= 0) {
      m_devicesCombo->removeItem(idx);
    }
  });

  connect(spotlight, &Spotlight::deviceConnected, this,
  [this](const DeviceId& id, const QString& name)
  {
    const auto data = QVariant::fromValue(id);
    if (m_devicesCombo->findData(data) < 0) {
      m_devicesCombo->addItem(descriptionString(name, id), data);
    }
  });

  connect(m_devicesCombo, static_cast<void (QComboBox::*)(int)>(&QComboBox::currentIndexChanged), this,
  [this, spotlight](int index)
  {
    if (index < 0)
    {
      m_inputMapper = nullptr;
      emit currentDeviceChanged(invalidDeviceId);
      return;
    }

    const auto devId = qvariant_cast<DeviceId>(m_devicesCombo->itemData(index));
    const auto currentConn = spotlight->deviceConnection(devId);
    m_inputMapper = currentConn ? currentConn->inputMapper().get() : nullptr;

    emit currentDeviceChanged(devId);
  });

  const auto currentConn = spotlight->deviceConnection(currentDeviceId());
  m_inputMapper = currentConn ? currentConn->inputMapper().get() : nullptr;
}

// -------------------------------------------------------------------------------------------------
QWidget* DevicesWidget::createDisconnectedStateWidget()
{
  const auto stateWidget = new QWidget(this);
  const auto hbox = new QHBoxLayout(stateWidget);
  const auto label = new QLabel(tr("No devices connected."), stateWidget);
  label->setToolTip(label->text());
  auto icon = style()->standardIcon(QStyle::SP_MessageBoxWarning);
  const auto iconLabel = new QLabel(stateWidget);
  iconLabel->setPixmap(icon.pixmap(16,16));
  hbox->addStretch();
  hbox->addWidget(iconLabel);
  hbox->addWidget(label);
  hbox->addStretch();
  return stateWidget;
}

// -------------------------------------------------------------------------------------------------
TimerTabWidget::TimerTabWidget(Settings* settings, QWidget* parent)
  : QWidget(parent)
  , m_settings(settings)
  , m_multiTimerWidget(new MultiTimerWidget(this))
  , m_vibrationSettingsWidget(new VibrationSettingsWidget(this))
{
  const auto layout = new QVBoxLayout(this);

  layout->addWidget(m_multiTimerWidget);
  layout->addWidget(m_vibrationSettingsWidget);

  connect(m_multiTimerWidget, &MultiTimerWidget::timerValueChanged, this,
  [this](int id, int secs) {
    m_settings->setTimerSettings(m_deviceId, id, m_multiTimerWidget->timerEnabled(id), secs);
  });

  connect(m_multiTimerWidget, &MultiTimerWidget::timerEnabledChanged, this,
  [this](int id, bool enabled) {
    m_settings->setTimerSettings(m_deviceId, id, enabled, m_multiTimerWidget->timerValue(id));
  });

  connect(m_vibrationSettingsWidget, &VibrationSettingsWidget::intensityChanged, this,
  [settings, this](uint8_t intensity) {
    m_settings->setVibrationSettings(m_deviceId, m_vibrationSettingsWidget->length(), intensity);
  });

  connect(m_vibrationSettingsWidget, &VibrationSettingsWidget::lengthChanged, this,
  [settings, this](uint8_t len) {
    m_settings->setVibrationSettings(m_deviceId, len, m_vibrationSettingsWidget->intensity());
  });

  connect(m_multiTimerWidget, &MultiTimerWidget::timeout,
          m_vibrationSettingsWidget, &VibrationSettingsWidget::sendVibrateCommand);
}

// -------------------------------------------------------------------------------------------------
void DevicesWidget::updateTimerTab(Spotlight* spotlight)
{
    // Helper method to return the first subconnection that supports vibrate.
  auto getVibrateConnection = [](const std::shared_ptr<DeviceConnection>& conn) {
    if (conn) {
      for (const auto& item : conn->subDevices()) {
        if (item.second->hasFlags(DeviceFlag::Vibrate)) return item.second;
      }
    }
    return std::shared_ptr<SubDeviceConnection>{};
  };

  const auto currentConn = spotlight->deviceConnection(currentDeviceId());
  const auto vibrateConn = getVibrateConnection(currentConn);

  if (m_timerTabContext) m_timerTabContext->deleteLater();

  if (vibrateConn)
  {
    if (m_tabWidget->indexOf(m_timerTabWidget) < 0) {
      m_tabWidget->insertTab(1, m_timerTabWidget, tr("Vibration Timer"));
    }
    m_timerTabWidget->setSubDeviceConnection(vibrateConn.get());
  }
  else if (m_timerTabWidget) {
    removeTab(m_tabWidget, m_timerTabWidget);
    m_timerTabWidget->setSubDeviceConnection(nullptr);
  }
  m_tabWidget->setCurrentIndex(0);

  if (currentConn) {
    m_timerTabContext = QPointer<QObject>(new QObject(this));
    connect(&*currentConn, &DeviceConnection::subDeviceFlagsChanged, m_timerTabContext,
    [currId=currentDeviceId(), spotlight, this](const DeviceId& id, const QString&) {
      if (currId != id) return;
      updateTimerTab(spotlight);
    });
  }

}

// -------------------------------------------------------------------------------------------------
void TimerTabWidget::loadSettings(const DeviceId& deviceId)
{
  m_multiTimerWidget->stopAllTimers();
  m_multiTimerWidget->blockSignals(true);
  m_vibrationSettingsWidget->blockSignals(true);

  m_deviceId = deviceId;

  for (int i = 0; i < m_multiTimerWidget->timerCount(); ++i) {
    const auto ts = m_settings->timerSettings(deviceId, i);
    m_multiTimerWidget->setTimerEnabled(i, ts.first);
    m_multiTimerWidget->setTimerValue(i, ts.second);
  }

  const auto vs = m_settings->vibrationSettings(deviceId);
  m_vibrationSettingsWidget->setLength(vs.first);
  m_vibrationSettingsWidget->setIntensity(vs.second);

  m_vibrationSettingsWidget->blockSignals(false);
  m_multiTimerWidget->blockSignals(false);
}

// -------------------------------------------------------------------------------------------------
void TimerTabWidget::setSubDeviceConnection(SubDeviceConnection* sdc) {
  m_vibrationSettingsWidget->setSubDeviceConnection(sdc);
}
