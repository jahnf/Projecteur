// This file is part of Projecteur - https://github.com/jahnf/projecteur - See LICENSE.md and README.md
#pragma once

#include <QObject>

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

//  bool deviceFound() const; //!< Returns true if a Logitech Spotlight device was found
  bool deviceConnected() const; //!< Returns true if the Logitech Spotlight device could be opened.

signals:
  void error(const QString& errMsg);
  void connected(const QString& devicePath);
  void disconnected(const QString& devicePath);
  void spotActiveChanged(bool isActive);

private:
  bool connectToDevice(const QString& devicePath);
  bool setupUdevNotifier();

private:
  QScopedPointer<QSocketNotifier> m_deviceSocketNotifier;
  QScopedPointer<QSocketNotifier> m_linuxUdevNotifier;
  QTimer* m_activeTimer;
  bool m_spotActive = false;
};
