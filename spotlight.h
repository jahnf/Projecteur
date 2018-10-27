// This file is part of Projecteur - https://github.com/jahnf/projecteur - See LICENSE.md and README.md
#pragma once

#include <QObject>
#include <map>

class QSocketNotifier;
class QTimer;

/// Simple class to notify the application if the Logitech Spotlight sending mouse move events.
/// Used to turn the applications spot on or off.
class Spotlight : public QObject
{
  Q_OBJECT

public:
  explicit Spotlight(QObject* parent);
  virtual ~Spotlight();

  bool spotActive() const { return m_spotActive; }
  bool deviceConnected() const; //!< Returns true if a Logitech Spotlight device could be opened.

signals:
  void error(const QString& errMsg);
  void connected(const QString& devicePath);
  void disconnected(const QString& devicePath);
  void spotActiveChanged(bool isActive);

private:
  enum class ConnectionResult { CouldNotOpen, NotASpotlightDevice, Connected };
  ConnectionResult connectSpotlightDevice(const QString& devicePath);
  bool setupDevEventInotify();
  int connectDevices();

private:
  std::map<QString, QScopedPointer<QSocketNotifier>> m_eventNotifiers;
  QTimer* m_activeTimer;
  bool m_spotActive = false;
};
