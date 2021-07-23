// This file is part of Projecteur - https://github.com/jahnf/projecteur - See LICENSE.md and README.md
#pragma once

#include <QPointer>
#include <QWidget>

struct DeviceId;
class InputMapper;
class QComboBox;
class Settings;
class Spotlight;
class VibrationSettingsWidget;

class QTimer;
class QTextEdit;

// -------------------------------------------------------------------------------------------------
class DevicesWidget : public QWidget
{
  Q_OBJECT

public:
  explicit DevicesWidget(Settings* settings, Spotlight* spotlight, QWidget* parent = nullptr);
  const DeviceId currentDeviceId() const;
  void updateDeviceDetails(Spotlight* spotlight);

signals:
  void currentDeviceChanged(const DeviceId&);

private:
  QWidget* createDisconnectedStateWidget();
  void createDeviceComboBox(Spotlight* spotlight);
  QWidget* createDevicesWidget(Settings* settings, Spotlight* spotlight);
  QWidget* createInputMapperWidget(Settings* settings, Spotlight* spotlight);
  QWidget* createDeviceInfoWidget(Spotlight* spotlight);
  QWidget* createTimerTabWidget(Settings* settings, Spotlight* spotlight);

  QComboBox* m_devicesCombo = nullptr;
  QWidget* m_timerTabWidget = nullptr;
  QWidget* m_deviceDetailsTabWidget = nullptr;

  // TODO Put into separate DeviceDetailsWidget
  QTextEdit* m_deviceDetailsTextEdit = nullptr;
  QTimer* m_updateDeviceDetailsTimer = nullptr;

  VibrationSettingsWidget* m_vibrationSettingsWidget = nullptr;
  QPointer<InputMapper> m_inputMapper;
};
