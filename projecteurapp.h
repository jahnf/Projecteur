#pragma once

#include <QApplication>

class QSystemTrayIcon;
class QGlobalShortcutX11;

class ProjecteurApplication : public QApplication
{
  Q_OBJECT

public:
  explicit ProjecteurApplication(int &argc, char **argv);
  virtual ~ProjecteurApplication();

private:
  QSystemTrayIcon* m_trayIcon = nullptr;
  QGlobalShortcutX11* m_globalShortcut = nullptr;
};
