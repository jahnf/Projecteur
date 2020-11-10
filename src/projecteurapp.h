// This file is part of Projecteur - https://github.com/jahnf/projecteur - See LICENSE.md and README.md
#pragma once
#include "spotlight.h"

#include <QApplication>

#include <map>
#include <memory>

class AboutDialog;
class LinuxDesktop;
class PreferencesDialog;
class QLocalServer;
class QLocalSocket;
class QMenu;
class QQmlApplicationEngine;
class QQmlComponent;
class QSystemTrayIcon;
class Settings;
class Settings;

class ProjecteurApplication : public QApplication
{
  Q_OBJECT
  Q_PROPERTY(bool overlayVisible READ overlayVisible NOTIFY overlayVisibleChanged)
  Q_PROPERTY(quint64 currentSpotScreen READ currentSpotScreen NOTIFY currentSpotScreenChanged)

public:
  struct Options {
    QString configFile;
    bool enableUInput = true; // enable virtual uinput device
    bool showPreferencesOnStart = false;
    bool dialogMinimizeOnly = false;
    bool disableOverlay = false;
    std::vector<SupportedDevice> additionalDevices;
  };

  explicit ProjecteurApplication(int &argc, char **argv, const Options& options);
  virtual ~ProjecteurApplication() override;

  bool overlayVisible() const { return m_overlayVisible; }

signals:
  void overlayVisibleChanged(bool visible);
  void currentSpotScreenChanged(quint64 screen);

public slots:
  void cursorExitedWindow();
  void cursorEntered(quint64 screen);
  void spotlightWindowClicked();

private slots:
  void readCommand(QLocalSocket* client);

private:
  void showPreferences(bool show = true);
  void setScreenForCursorPos();
  QScreen* screenAtCursorPos() const;
  QWindow* createOverlayWindow();
  void updateOverlayWindow(QWindow* window, QScreen* screen);
  void setupScreenOverlays();
  quint64 currentSpotScreen() const;
  void setCurrentSpotScreen(quint64 screen);

private:
  std::unique_ptr<QSystemTrayIcon> m_trayIcon;
  std::unique_ptr<QMenu> m_trayMenu;
  std::unique_ptr<PreferencesDialog> m_dialog;
  std::unique_ptr<AboutDialog> m_aboutDialog;
  QLocalServer* const m_localServer = nullptr;
  Spotlight* m_spotlight = nullptr;
  Settings* m_settings = nullptr;
  LinuxDesktop* m_linuxDesktop = nullptr;
  QQmlApplicationEngine* m_qmlEngine = nullptr;
  QQmlComponent* m_windowQmlComponent = nullptr;
  std::map<QLocalSocket*, quint32> m_commandConnections;
  bool m_overlayVisible = false;
  const bool m_xcbOnWayland = false;

  QList<QWindow*> m_overlayWindows;
  std::map<QScreen*, QWindow*> m_screenWindowMap;
  quint64 m_currentSpotScreen = 0;
};

class ProjecteurCommandClientApp : public QCoreApplication
{
  Q_OBJECT

public:
  explicit ProjecteurCommandClientApp(const QStringList& ipcCommands, int &argc, char **argv);
};
