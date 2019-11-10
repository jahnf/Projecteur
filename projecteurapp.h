// This file is part of Projecteur - https://github.com/jahnf/projecteur - See LICENSE.md and README.md
#pragma once

#include <QApplication>

#include <map>

class AboutDialog;
class PreferencesDialog;
class QLocalServer;
class QLocalSocket;
class QMenu;
class QSystemTrayIcon;
class Settings;
class Spotlight;
class Settings;

class ProjecteurApplication : public QApplication
{
  Q_OBJECT

public:
  struct Options {
    QString configFile;
  };

  explicit ProjecteurApplication(int &argc, char **argv, const Options& options);
  virtual ~ProjecteurApplication() override;

public slots:
  void cursorExitedWindow();

private slots:
  void readCommand(QLocalSocket* client);

private:
  void showPreferences(bool show = true);

private:
  QScopedPointer<QSystemTrayIcon> m_trayIcon;
  QScopedPointer<QMenu> m_trayMenu;
  QScopedPointer<PreferencesDialog> m_dialog;
  QScopedPointer<AboutDialog> m_aboutDialog;
  QLocalServer* const m_localServer = nullptr;
  Spotlight* m_spotlight = nullptr;
  Settings* m_settings = nullptr;
  std::map<QLocalSocket*, quint32> m_commandConnections;
};

class ProjecteurCommandClientApp : public QCoreApplication
{
  Q_OBJECT

public:
  explicit ProjecteurCommandClientApp(const QString& ipcCommand, int &argc, char **argv);
};
