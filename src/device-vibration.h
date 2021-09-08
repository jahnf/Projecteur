// This file is part of Projecteur - https://github.com/jahnf/projecteur
// - See LICENSE.md and README.md
#pragma once

#include <QPointer>
#include <QWidget>
#include <memory>

class QSpinBox;
class SubDeviceConnection;
class SubHidppConnection;

// -------------------------------------------------------------------------------------------------
class TimerWidget : public QWidget
{
  Q_OBJECT

public:
  TimerWidget(QWidget* parent);
  ~TimerWidget() override;

  bool timerEnabled() const;
  void setTimerEnabled(bool enabled);

  void start();
  void stop();
  bool timerRunning() const;
  void setValueSeconds(int seconds);
  void setValueMinutes(int minutes);
  int valueSeconds() const;

signals:
  void timeout();
  void valueSecondsChanged(int);
  void enabledChanged(bool);

private:
  struct Impl;
  std::unique_ptr<Impl> m_impl;
};

// -------------------------------------------------------------------------------------------------
class MultiTimerWidget : public QWidget
{
  Q_OBJECT

public:
  explicit MultiTimerWidget(QWidget* parent = nullptr);
  virtual ~MultiTimerWidget() override;

  /// Returns the number of timers
  int timerCount() const;

  void setTimerEnabled(int timerId, bool enabled);
  bool timerEnabled(int timerId) const;

  void startTimer(int timerId);
  void stopTimer(int timerId);
  void stopAllTimers();
  bool timerRunning(int timerId) const;

  void setTimerValue(int timerId, int seconds);
  int timerValue(int timerId) const;

signals:
  /// Emitted when a timer times out.
  void timeout(int timerId);
  void timerEnabledChanged(int timerId, bool enabled);
  void timerValueChanged(int timerId, int seconds);

private:
  struct Impl;
  std::unique_ptr<Impl> m_impl;
};

// -------------------------------------------------------------------------------------------------
class VibrationSettingsWidget : public QWidget
{
  Q_OBJECT

public:
  explicit VibrationSettingsWidget(QWidget* parent = nullptr);

  uint8_t length() const;
  void setLength(uint8_t len);

  uint8_t intensity() const;
  void setIntensity(uint8_t intensity);

  void setSubDeviceConnection(SubDeviceConnection* sdc);
  void sendVibrateCommand();

signals:
  void intensityChanged(uint8_t intensity);
  void lengthChanged(uint8_t length);

private:
  QPointer<SubHidppConnection> m_subDeviceConnection;
  QSpinBox* m_sbLength = nullptr;
  QSpinBox* m_sbIntensity = nullptr;
};
