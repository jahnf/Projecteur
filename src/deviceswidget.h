// This file is part of Projecteur - https://github.com/jahnf/projecteur - See LICENSE.md and README.md
#pragma once

#include "spotlight.h"

#include <QWidget>

class Settings;
class QComboBox;

class DevicesWidget : public QWidget
{
  Q_OBJECT
public:
  explicit DevicesWidget(Settings* settings, Spotlight* spotlight, QWidget* parent = nullptr);
  const DeviceId& currentDevice() const;

signals:
  void currentDeviceChanged(const DeviceId&);

private:
  QWidget* createDisconnectedStateWidget();
  QComboBox* createDeviceComboBox(Spotlight* spotlight);
  QWidget* createDevicesWidget(Spotlight* spotlight);
  QWidget* createInputMapperWidget();
  QWidget* createDeviceInfoWidget();

  QComboBox* m_devicesCombo = nullptr;
};

