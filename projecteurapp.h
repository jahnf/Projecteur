#pragma once

#include <QApplication>

class PreferencesDialog;
class QMenu;
class QSystemTrayIcon;

class ProjecteurApplication : public QApplication
{
  Q_OBJECT

public:
  explicit ProjecteurApplication(int &argc, char **argv);
  virtual ~ProjecteurApplication() override;

private:
  QSystemTrayIcon* m_trayIcon = nullptr;
  QScopedPointer<QMenu> m_trayMenu;
  QScopedPointer<PreferencesDialog> m_dialog;
};
