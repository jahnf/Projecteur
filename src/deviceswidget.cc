// This file is part of Projecteur - https://github.com/jahnf/projecteur - See LICENSE.md and README.md
#include "deviceswidget.h"

#include "logging.h"
#include "spotlight.h"

#include <QComboBox>
#include <QLabel>
#include <QLayout>
#include <QStackedLayout>
#include <QStyle>
#include <QTimer>

DECLARE_LOGGING_CATEGORY(preferences)

// ------------------------------------------------------------------------------------------------
namespace {
  QString descriptionString(const QString& name, const Spotlight::DeviceId& id) {
    return QString("%1 (%2:%3) [%4]").arg(name).arg(id.vendorId, 4, 16, QChar('0'))
                                     .arg(id.productId, 4, 16, QChar('0')).arg(id.phys);
  }
}

// ------------------------------------------------------------------------------------------------
DevicesWidget::DevicesWidget(Settings* /*settings*/, Spotlight* spotlight, QWidget* parent)
  : QWidget(parent)
{
  const auto stackLayout = new QStackedLayout(this);
  const auto disconnectedWidget = createDisconnectedStateWidget(this);
  stackLayout->addWidget(disconnectedWidget);
  const auto deviceWidget = createDevicesWidget(spotlight, this);
  stackLayout->addWidget(deviceWidget);

  const bool anyDeviceConnected = spotlight->anySpotlightDeviceConnected();
  stackLayout->setCurrentWidget(anyDeviceConnected ? deviceWidget : disconnectedWidget);

  connect(spotlight, &Spotlight::anySpotlightDeviceConnectedChanged, this,
  [stackLayout, deviceWidget, disconnectedWidget](bool anyConnected){
    stackLayout->setCurrentWidget(anyConnected ? deviceWidget : disconnectedWidget);
  });
}

// ------------------------------------------------------------------------------------------------
QWidget* DevicesWidget::createDevicesWidget(Spotlight* spotlight, QWidget* parent)
{
  const auto dw = new QWidget(parent);
  const auto vLayout = new QVBoxLayout(dw);
  const auto devHLayout = new QHBoxLayout();
  vLayout->addLayout(devHLayout);

  const auto deviceCombo = new QComboBox(dw);

  for (const auto& dev : spotlight->connectedDevices()) {
    deviceCombo->addItem(descriptionString(dev.name, dev.id), QVariant::fromValue(dev.id));
  }

  devHLayout->addWidget(new QLabel(tr("Devices"), dw));
  devHLayout->addWidget(deviceCombo);
  devHLayout->setStretch(1, 1);

  connect(spotlight, &Spotlight::deviceDisconnected, this,
  [deviceCombo](const Spotlight::DeviceId& id, const QString& /*name*/)
  {
    const auto idx = deviceCombo->findData(QVariant::fromValue(id));
    if (idx >= 0) {
      deviceCombo->removeItem(idx);
    }
  });

  connect(spotlight, &Spotlight::deviceConnected, this,
  [deviceCombo](const Spotlight::DeviceId& id, const QString& name)
  {
    const auto data = QVariant::fromValue(id);
    if (deviceCombo->findData(data) < 0) {
      deviceCombo->addItem(descriptionString(name, id), data);
    }
  });

  vLayout->addWidget(new QLabel("<Placeholder for Button Mapping>", dw));
  return dw;
}

// ------------------------------------------------------------------------------------------------
QWidget* DevicesWidget::createDisconnectedStateWidget(QWidget* parent)
{
  const auto stateWidget = new QWidget(parent);
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
