// This file is part of Projecteur - https://github.com/jahnf/projecteur - See LICENSE.md and README.md
#pragma once

#include <QPointer>
#include <QWidget>

struct DeviceId;
class InputMapper;
class QComboBox;
class Settings;
class Spotlight;

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
  QWidget* createDevicesWidget(Settings* settings, Spotlight* spotlight);
  QWidget* createInputMapperWidget(Settings* settings, Spotlight* spotlight);
  QWidget* createDeviceInfoWidget(Spotlight* spotlight);

  QComboBox* m_devicesCombo = nullptr;
  QWidget* m_vibrationTimerWidget = nullptr;
  QPointer<InputMapper> m_inputMapper;
};
