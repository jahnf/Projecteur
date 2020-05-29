// This file is part of Projecteur - https://github.com/jahnf/projecteur - See LICENSE.md and README.md
#pragma once

#include "spotlight.h"

#include <QPointer>
#include <QWidget>

#include <memory>

class DeviceConnection;
class Settings;
class QComboBox;
class InputMapper;

// -------------------------------------------------------------------------------------------------
class DevicesWidget : public QWidget
{
  Q_OBJECT

public:
  explicit DevicesWidget(Settings* settings, Spotlight* spotlight, QWidget* parent = nullptr);
  const DeviceId currentDeviceId() const;

signals:
  void currentDeviceChanged(const DeviceId&);

private:
  QWidget* createDisconnectedStateWidget();
  void createDeviceComboBox(Spotlight* spotlight);
  QWidget* createDevicesWidget(Spotlight* spotlight);
  QWidget* createInputMapperWidget(Spotlight* spotlight);
  QWidget* createDeviceInfoWidget(Spotlight* spotlight);

  QComboBox* m_devicesCombo = nullptr;
  QPointer<InputMapper> m_inputMapper;
};
