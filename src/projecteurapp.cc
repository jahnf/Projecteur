// This file is part of Projecteur - https://github.com/jahnf/projecteur - See LICENSE.md and README.md
#include "projecteurapp.h"

#include "aboutdlg.h"
#include "imageitem.h"
#include "linuxdesktop.h"
#include "logging.h"
#include "preferencesdlg.h"
#include "settings.h"
#include "spotlight.h"
#include "virtualdevice.h"

#include <QDesktopWidget>
#include <QDialog>
#include <QFontDatabase>
#include <QLocalServer>
#include <QLocalSocket>
#include <QMenu>
#include <QMessageBox>
#include <QPointer>
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include <QScreen>
#include <QSystemTrayIcon>
#include <QTimer>
#include <QWindow>

LOGGING_CATEGORY(mainapp, "mainapp")
LOGGING_CATEGORY(cmdclient, "cmdclient")
LOGGING_CATEGORY(cmdserver, "cmdserver")

namespace {
  QString localServerName() {
    return QCoreApplication::applicationName() + "_local_socket";
  }
}

ProjecteurApplication::ProjecteurApplication(int &argc, char **argv, const Options& options)
  : QApplication(argc, argv)
  , m_trayIcon(new QSystemTrayIcon())
  , m_trayMenu(new QMenu())
  , m_localServer(new QLocalServer(this))
  , m_linuxDesktop(new LinuxDesktop(this))
{
  if (screens().size() < 1)
  {
    QMessageBox::critical(nullptr, tr("No Screens detected"), tr("screens().size() returned a size < 1.\nExiting."));
    QTimer::singleShot(0, [this](){ this->exit(2); });
    return;
  }

  setQuitOnLastWindowClosed(false);
  QFontDatabase::addApplicationFont(":/icons/projecteur-icons.ttf");

  m_spotlight = new Spotlight(this, Spotlight::Options{options.enableUInput, options.additionalDevices});
  m_settings = options.configFile.isEmpty() ? new Settings(this)
                                            : new Settings(options.configFile, this);
  m_settings->setOverlayDisabled(options.disableOverlay);
  m_dialog.reset(new PreferencesDialog(m_settings, m_spotlight,
                                       options.dialogMinimizeOnly
                                       ? PreferencesDialog::Mode::MinimizeOnlyDialog
                                       : PreferencesDialog::Mode::ClosableDialog));

  connect(&*m_dialog, &PreferencesDialog::testButtonClicked, [this](){
    emit m_spotlight->spotActiveChanged(true);
  });

  const QString desktopEnv = m_linuxDesktop->type() == LinuxDesktop::Type::KDE ? "KDE" :
                              m_linuxDesktop->type() == LinuxDesktop::Type::Gnome ? "Gnome"
                                                                                  : tr("Unknown");

  logDebug(mainapp) << tr("Qt platform plugin: %1;").arg(QGuiApplication::platformName())
                    << tr("Desktop Environment: %1;").arg(desktopEnv)
                    << tr("Wayland: %1").arg(m_linuxDesktop->isWayland() ? "true" : "false");

  if (options.showPreferencesOnStart) {
    QTimer::singleShot(0, this, [this](){ showPreferences(true); });
  }
  else if (options.dialogMinimizeOnly) {
    QTimer::singleShot(0, this, [this](){ m_dialog->show(); m_dialog->showMinimized(); });
  }

  const auto desktopImageProvider = new PixmapProvider(this);
  const auto engine = new QQmlApplicationEngine(this);
  engine->rootContext()->setContextProperty("Settings", m_settings);
  engine->rootContext()->setContextProperty("PreferencesDialog", &*m_dialog);
  engine->rootContext()->setContextProperty("DesktopImage", desktopImageProvider);
  engine->rootContext()->setContextProperty("ProjecteurApp", this);
  engine->load(QUrl(QStringLiteral("qrc:/main.qml")));
  const auto window = topLevelWindows().first();

  const auto actionPref = m_trayMenu->addAction(tr("&Preferences..."));
  connect(actionPref, &QAction::triggered, [this](){
    this->showPreferences(true);
  });

  const auto actionAbout = m_trayMenu->addAction(tr("&About"));
  connect(actionAbout, &QAction::triggered, [this]()
  {
    if (!m_aboutDialog) {
      m_aboutDialog = std::make_unique<AboutDialog>();
      connect(m_aboutDialog.get(), &QDialog::finished, [this](int){
        m_aboutDialog.reset(); // No need to keep about dialog in memory, not that important
      });
    }

    if (m_aboutDialog->isVisible()) {
      m_aboutDialog->show();
      m_aboutDialog->raise();
      m_aboutDialog->activateWindow();
    } else {
      m_aboutDialog->exec();
    }
  });

  m_trayMenu->addSeparator();
  const auto actionQuit = m_trayMenu->addAction(tr("&Quit"));
  connect(actionQuit, &QAction::triggered, [this](){ this->quit(); });
  m_trayIcon->setContextMenu(&*m_trayMenu);

  m_trayIcon->setIcon(QIcon(":/icons/projecteur-tray-64.png"));
  m_trayIcon->show();

  connect(&*m_trayIcon, &QSystemTrayIcon::activated, [this](QSystemTrayIcon::ActivationReason reason) {
    if (reason == QSystemTrayIcon::Trigger)
    {
      //static const bool isKDE = (qgetenv("XDG_CURRENT_DESKTOP") == QByteArray("KDE"));
      const auto trayGeometry = m_trayIcon->geometry();
      // This usually won't give us a valid geometry, since Qt isn't drawing the tray icon itself
      if (trayGeometry.isValid()) {
        m_trayIcon->contextMenu()->popup(m_trayIcon->geometry().center());
      } else {
        // It's tricky to get the same behavior on all desktop environments. While on GNOME3
        // it behaves as one (or most) would expect, it behaves differently on other Desktop
        // environments.
        // QSystemTrayIcon is a wrapper around the StatusNotfierItem on modern (Linux) Desktops
        // see: https://www.freedesktop.org/wiki/Specifications/StatusNotifierItem/
        // Via the Qt API there is not much control over how e.g. KDE or GNOME show the icon
        // and how it behaves.. e.g. setting something like
        // org.freedesktop.StatusNotifierItem.ItemIsMenu to True would be good for KDE Plasma
        // see: https://www.freedesktop.org/wiki/Specifications/StatusNotifierItem/StatusNotifierItem/
        this->showPreferences(true);
      }
    }
  });

  connect(&*m_dialog, &PreferencesDialog::exitApplicationRequested, [actionQuit]() {
    logDebug(mainapp) << tr("Exit request from preferences dialog.");
    actionQuit->trigger();
  });

  window->setFlags(window->flags() | Qt::WindowTransparentForInput | Qt::Tool);
  connect(this, &ProjecteurApplication::aboutToQuit, [window](){ if (window) window->close(); });

  const bool xcbOnWayland = QGuiApplication::platformName() == "xcb" && m_linuxDesktop->isWayland();
  if (xcbOnWayland) {
    logWarning(mainapp) << tr("Qt 'xcb' platform and Wayland session detected.");
  }

  // Handling of spotlight window when mouse move events from spotlight device are detected
  connect(m_spotlight, &Spotlight::spotActiveChanged,
  [window, desktopImageProvider, xcbOnWayland, this](bool active)
  {
    if (active && !m_settings->overlayDisabled())
    {
      setScreenForCursorPos();

      window->setFlags(window->flags() | Qt::WindowStaysOnTopHint);
      window->setFlags(window->flags() & ~Qt::SplashScreen);
      window->setFlags(window->flags() | Qt::ToolTip);
      window->setFlags(window->flags() & ~Qt::WindowTransparentForInput);

      if (window->screen())
      {
        if (m_settings->zoomEnabled()) {
          desktopImageProvider->setPixmap(m_linuxDesktop->grabScreen(window->screen()));
        }

        const auto screenGeometry = window->screen()->geometry();
        if (window->geometry() != screenGeometry) {
          window->setGeometry(screenGeometry);
        }
        window->setPosition(screenGeometry.topLeft());
      }
      window->show();
      m_overlayVisible = true;
      emit overlayVisibleChanged(true);
      window->raise();
    }
    else
    {
      m_overlayVisible = false;
      emit overlayVisibleChanged(false);
      window->setFlags(window->flags() | Qt::WindowTransparentForInput);
      window->setFlags(window->flags() & ~Qt::WindowStaysOnTopHint);
      // Workaround for 'xcb' on Wayland session (default on Ubuntu)
      // .. the window in that case is not transparent for inputs and cannot be clicked through.
      // --> hide the window, although animations will not be visible
      if (xcbOnWayland) window->hide();
    }
  });

  connect(window, &QWindow::visibleChanged, [this](bool v){
    logDebug(mainapp) << tr("Spotlight window visible = ") << v;
    if (!v && m_dialog->isVisible()) {
      m_dialog->raise();
      m_dialog->activateWindow();
    }
  });

  // Handling if the screen in the settings was changed
  connect(m_settings, &Settings::screenChanged, [this, window](int screenIdx)
  {
    if (screenIdx >= screens().size())
      return;

    const auto screen = screens()[screenIdx];
    const bool wasVisible = window->isVisible();

    window->setGeometry(QRect(screen->geometry().topLeft(), QSize(300,200)));
    window->setScreen(screen);
    window->setGeometry(screen->geometry());

    if (wasVisible) {
      QTimer::singleShot(0, [this](){
        emit m_spotlight->spotActiveChanged(true);
      });
    }
  });

  // Open local server for local IPC commands, e.g. from other command line instances
  QLocalServer::removeServer(localServerName());
  if (m_localServer->listen(localServerName()))
  {
    connect(m_localServer, &QLocalServer::newConnection, this, [this]()
    {
      while(QLocalSocket *clientConnection = m_localServer->nextPendingConnection())
      {
        connect(clientConnection, &QLocalSocket::readyRead, this, [this, clientConnection]() {
          this->readCommand(clientConnection);
        });
        connect(clientConnection, &QLocalSocket::disconnected, this, [this, clientConnection]() {
          const auto it = m_commandConnections.find(clientConnection);
          if (it != m_commandConnections.end())
          {
            quint32& commandSize = it->second;
            while (clientConnection->bytesAvailable() && commandSize <= clientConnection->bytesAvailable()) {
              this->readCommand(clientConnection);
            }
            m_commandConnections.erase(it);
          }
          clientConnection->close();
          clientConnection->deleteLater();
        });

        // Timeout timer - if after 5 seconds the connection is still open just disconnect...
        const auto clientConnPtr = QPointer<QLocalSocket>(clientConnection);
        QTimer::singleShot(5000, [clientConnPtr](){
          if (clientConnPtr) {
            // time out
            clientConnPtr->disconnectFromServer();
          }
        });

        m_commandConnections.emplace(clientConnection, 0);
      }
    });
  }
  else
  {
    logError(cmdserver) << tr("Error starting local socket for inter-process communication.");
  }
}

ProjecteurApplication::~ProjecteurApplication()
{
  if (m_localServer) m_localServer->close();
}

void ProjecteurApplication::cursorExitedWindow()
{
  setScreenForCursorPos();
}

void ProjecteurApplication::setScreenForCursorPos()
{
#if (QT_VERSION >= QT_VERSION_CHECK(5, 10, 0))
  int screenNumber = 0;
  const auto screen_cursor = this->screenAt(QCursor::pos());

  for (const auto& screen : screens()) {
    if (screen_cursor == screen) {
      m_settings->setScreen(screenNumber);
      break;
    }
    ++screenNumber;
  }
#else
  m_settings->setScreen(this->desktop()->screenNumber(QCursor::pos()));
#endif
}

void ProjecteurApplication::readCommand(QLocalSocket* clientConnection)
{
  auto it = m_commandConnections.find(clientConnection);
  if (it == m_commandConnections.end()) {
    return;
  }

  quint32& commandSize = it->second;

  // Read size of command (always quint32) if not already done.
  if (commandSize == 0) {
    if (clientConnection->bytesAvailable() < static_cast<int>(sizeof(quint32)))
      return;

    QDataStream in(clientConnection);
    in >> commandSize;

    if (commandSize > 256)
    {
      logWarning(cmdserver) << tr("Received invalid command size (%1)").arg(commandSize);
      clientConnection->disconnectFromServer();
      return ;
    }
  }

  if (clientConnection->bytesAvailable() < commandSize || clientConnection->atEnd()) {
    return;
  }

  const auto command = QString::fromLocal8Bit(clientConnection->read(commandSize));
  const QString cmdKey = command.section('=', 0, 0).trimmed();
  const QString cmdValue = command.section('=', 1).trimmed();

  if (cmdKey == "quit")
  {
    logDebug(cmdserver) << tr("Received quit command.");
    this->quit();
  }
  else if (cmdKey == "spot")
  {
    const bool active = (cmdValue == "on" || cmdValue == "1" || cmdValue == "true");
    logDebug(cmdserver) << tr("Received command spot = %1").arg(active);
    emit m_spotlight->spotActiveChanged(active);
  }
  else if (cmdKey == "settings" || cmdKey == "preferences")
  {
    const bool show = !(cmdValue == "hide" || cmdValue == "0");
    logDebug(cmdserver) << tr("Received command settings = %1").arg(show);
    showPreferences(show);
  }
  else if (cmdValue.size())
  {
    const auto& properties = m_settings->stringProperties();
    const auto it = std::find_if(properties.cbegin(), properties.cend(),
    [&cmdKey](const auto& pair){
      return (pair.first == cmdKey);
    });
    if (it != m_settings->stringProperties().cend()) {
      logDebug(cmdserver) << tr("Received command '%1'='%2'").arg(cmdKey, cmdValue);
      it->second.setFunction(cmdValue);
    }
    else {
      // string property not found...
      logWarning(cmdserver) << tr("Received unknown command key (%1)").arg(cmdKey);
    }
  }
  // reset command size, for next command
  commandSize = 0;
}

void ProjecteurApplication::showPreferences(bool show)
{
  if (show)
  {
    m_dialog->show();
    m_dialog->raise();
    m_dialog->activateWindow();
  }
  else {
    m_dialog->hide();
  }
}

ProjecteurCommandClientApp::ProjecteurCommandClientApp(const QStringList& ipcCommands, int &argc, char **argv)
  : QCoreApplication(argc, argv)
{
  if (ipcCommands.isEmpty())
  {
    QMetaObject::invokeMethod(this, "quit", Qt::QueuedConnection);
    return;
  }

  QLocalSocket* const localSocket = new QLocalSocket(this);

  connect(localSocket,
          static_cast<void (QLocalSocket::*)(QLocalSocket::LocalSocketError)>(&QLocalSocket::error),
  [this, localSocket](QLocalSocket::LocalSocketError /*socketError*/) {
    logError(cmdclient) << tr("Error sending commands: %1", "%1=error message").arg(localSocket->errorString());
    localSocket->close();
    QMetaObject::invokeMethod(this, "quit", Qt::QueuedConnection);
  });

  connect(localSocket, &QLocalSocket::connected, [localSocket, &ipcCommands]()
  {
    for (const auto& ipcCommand : ipcCommands)
    {
      if (ipcCommand.isEmpty()) continue;

      const QByteArray commandBlock = [&ipcCommand]()
      {
        const QByteArray ipcBytes = ipcCommand.toLocal8Bit();
        QByteArray block;
        {
          QDataStream out(&block, QIODevice::WriteOnly);
          out << static_cast<quint32>(ipcBytes.size());
        }
        block.append(ipcBytes);
        return block;
      }();

      localSocket->write(commandBlock);
      localSocket->flush();
    }
    localSocket->disconnectFromServer();
  });

  connect(localSocket, &QLocalSocket::disconnected, [this, localSocket]() {
    localSocket->close();
    QMetaObject::invokeMethod(this, "quit", Qt::QueuedConnection);
  });

  localSocket->connectToServer(localServerName());
}
