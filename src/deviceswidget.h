// This file is part of Projecteur - https://github.com/jahnf/projecteur
// - See LICENSE.md and README.md
#pragma once

#include "device.h"

#include <QPointer>
#include <QWidget>

class InputMapper;
class MultiTimerWidget;
class QComboBox;
class Settings;
class Spotlight;
class VibrationSettingsWidget;
class TimerTabWidget;

class QTabWidget;
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
  TimerTabWidget* createTimerTabWidget(Settings* settings, Spotlight* spotlight);
  void updateTimerTab(Spotlight* spotlight);

  QComboBox* m_devicesCombo = nullptr;
  QTabWidget* m_tabWidget = nullptr;
  TimerTabWidget* m_timerTabWidget = nullptr;
  QPointer<QObject> m_timerTabContext;
  QWidget* m_deviceDetailsTabWidget = nullptr;

  // TODO Put into separate DeviceDetailsWidget
  QTextEdit* m_deviceDetailsTextEdit = nullptr;
  QTimer* m_updateDeviceDetailsTimer = nullptr;

  QPointer<InputMapper> m_inputMapper;
};

// -------------------------------------------------------------------------------------------------
class TimerTabWidget : public QWidget
{
  Q_OBJECT

public:
  TimerTabWidget(Settings* settings, QWidget* parent = nullptr);
  VibrationSettingsWidget* vibrationSettingsWidget();

  void loadSettings(const DeviceId& deviceId);
  void setSubDeviceConnection(SubDeviceConnection* sdc);

private:
  DeviceId m_deviceId;
  Settings* const m_settings = nullptr;
  MultiTimerWidget* m_multiTimerWidget = nullptr;
  VibrationSettingsWidget* m_vibrationSettingsWidget = nullptr;
};
