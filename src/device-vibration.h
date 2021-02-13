// This file is part of Projecteur - https://github.com/jahnf/projecteur - See LICENSE.md and README.md
#pragma once

#include <QWidget>
#include <memory>

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

  void restartTimer(int timerId);
  void stopTimer(int timerId);
  bool timerRunning(int timerId) const;
  /// Returns remaining time of a timer or -1 if timer is not running.
  int remainingTime(int timerId) const;

  void setTimerValue(int timerId, int seconds);
  int timerValue(int timerId) const;

signals:
  /// Emitted when a timer times out.
  void timeout(int timerId);
  void timerEnabledChanged(int timerId, bool enabled);

private:
  struct Impl;
  std::unique_ptr<Impl> m_impl;
};
