// This file is part of Projecteur - https://github.com/jahnf/projecteur - See LICENSE.md and README.md
#include "deviceswidget.h"

#include "spotlight.h"

#include <QLabel>
#include <QLayout>
#include <QStackedLayout>
#include <QStyle>
#include <QTimer>

// ------------------------------------------------------------------------------------------------
namespace {
}

// ------------------------------------------------------------------------------------------------
DevicesWidget::DevicesWidget(Settings* /*settings*/, Spotlight* spotlight, QWidget* parent)
  : QWidget(parent)
{
  const auto stackLayout = new QStackedLayout(this);
  const auto disconnectedWidget = createDisconnectedStateWidget(this);
  stackLayout->addWidget(disconnectedWidget);
  const auto deviceWidget = createDevicesWidget(this);
  stackLayout->addWidget(deviceWidget);

  const bool anyDeviceConnected = spotlight->anySpotlightDeviceConnected();
  stackLayout->setCurrentWidget(anyDeviceConnected ? deviceWidget : disconnectedWidget);

  connect(spotlight, &Spotlight::anySpotlightDeviceConnectedChanged, this,
  [stackLayout, deviceWidget, disconnectedWidget](bool anyConnected){
    stackLayout->setCurrentWidget(anyConnected ? deviceWidget : disconnectedWidget);
  });
}

// ------------------------------------------------------------------------------------------------
QWidget* DevicesWidget::createDevicesWidget(QWidget* parent)
{
  const auto w = new QWidget(parent);
  const auto l = new QVBoxLayout(w);
  l->addWidget(new QLabel("<Placeholder for devices button mapping>", w));
  return w;
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
