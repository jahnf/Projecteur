// This file is part of Projecteur - https://github.com/jahnf/projecteur - See LICENSE.md and README.md
#include "projecteurapp.h"

#include "aboutdlg.h"
#include "imageitem.h"
#include "preferencesdlg.h"
#include "qglobalshortcutx11.h"
#include "settings.h"
#include "spotlight.h"

#include <QDialog>
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

#include <QDebug>

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
{
  if (screens().size() < 1)
  {
    QMessageBox::critical(nullptr, tr("No Screens"), tr("screens().size() returned a size < 1."));
    QTimer::singleShot(0, [this](){ this->exit(2); });
    return;
  }

  setQuitOnLastWindowClosed(false);

  m_spotlight = new Spotlight(this);
  const auto settings = options.configFile.isEmpty() ? new Settings(this)
                                                     : new Settings(options.configFile, this);
  m_settings = settings;
  m_dialog.reset(new PreferencesDialog(settings, m_spotlight));
  m_dialog->updateAvailableScreens(screens());

  connect(&*m_dialog, &PreferencesDialog::testButtonClicked, [this](){
    emit m_spotlight->spotActiveChanged(true);
  });

  auto screen = screens().first();
  if (settings->screen() < screens().size()) {
    screen = screens().at(settings->screen());
  }

  const auto desktopImageProvider = new PixmapProvider(this);

  const auto engine = new QQmlApplicationEngine(this);
  engine->rootContext()->setContextProperty("Settings", settings);
  engine->rootContext()->setContextProperty("PreferencesDialog", &*m_dialog);
  engine->rootContext()->setContextProperty("DesktopImage", desktopImageProvider);
  engine->load(QUrl(QStringLiteral("qrc:/main.qml")));
  const auto window = topLevelWindows().first();

  const auto actionPref = m_trayMenu->addAction(tr("&Preferences..."));
  connect(actionPref, &QAction::triggered, [this](){
    this->showPreferences(true);
  });

  const auto actionAbout = m_trayMenu->addAction(tr("&About"));
  connect(actionAbout, &QAction::triggered, [this]()
  {
    if (!m_aboutDialog)
      m_aboutDialog.reset(new AboutDialog);

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
        // it behaves as one (or most) would expect it behaves differently on other Desktop
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

  window->setFlags(window->flags() | Qt::WindowTransparentForInput | Qt::Tool);
  window->setScreen(screen);
  window->setPosition(screen->geometry().topLeft());
  window->setWidth(screen->geometry().width());
  window->setHeight(screen->geometry().height());
  connect(this, &ProjecteurApplication::aboutToQuit, [window](){ if (window) window->close(); });

  // Example code for global shortcuts...
  //  const auto shortcut = new QGlobalShortcutX11(QKeySequence("Ctrl+F3"), this);
  //  connect(shortcut, &QGlobalShortcutX11::activated, [window](){
  //    qDebug() << "GlobalShortCut Ctrl+F3" << window;
  //  });

  // Handling of spotlight window when input from spotlight device is detected
  connect(m_spotlight, &Spotlight::spotActiveChanged,
  [window, settings, desktopImageProvider](bool active)
  {
    if (active)
    {
      window->setFlags(window->flags() | Qt::SplashScreen);
      window->setFlags(window->flags() & ~Qt::WindowTransparentForInput);
      window->setFlags(window->flags() | Qt::WindowStaysOnTopHint);
      window->hide();
      window->setFlags(window->flags() & ~Qt::SplashScreen);
      window->setFlags(window->flags() | Qt::ToolTip);

      if (window->screen())
      {
        if (settings->zoomEnabled()) {
          desktopImageProvider->setPixmap(window->screen()->grabWindow(0));
        }

        const auto screenGeometry = window->screen()->geometry();
        if (window->geometry() != screenGeometry) {
          window->setGeometry(screenGeometry);
        }
      }
      window->showFullScreen();
    }
    else
    {
      window->setFlags(window->flags() | Qt::SplashScreen | Qt::WindowStaysOnTopHint);
      window->hide();
    }
  });

  connect(window, &QWindow::visibleChanged, [this](bool v){
    if (!v && m_dialog->isVisible()) {
      m_dialog->raise();
      m_dialog->activateWindow();
    }
  });

  // Handling if the screen in the settings was changed
  connect(settings, &Settings::screenChanged, [this, window](int screenIdx)
  {
    if (screenIdx >= screens().size())
      return;

    const auto screen = screens()[screenIdx];
    const bool wasVisible = window->isVisible();

    window->setFlags(window->flags() | Qt::SplashScreen | Qt::WindowStaysOnTopHint);
    window->hide();
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
    connect(m_localServer, &QLocalServer::newConnection, [this]()
    {
      while(QLocalSocket *clientConnection = m_localServer->nextPendingConnection())
      {
        connect(clientConnection, &QLocalSocket::readyRead, [this, clientConnection]() {
          this->readCommand(clientConnection);
        });
        connect(clientConnection, &QLocalSocket::disconnected, [this, clientConnection]() {
          const auto it = m_commandConnections.find(clientConnection);
          if (it != m_commandConnections.end()) {
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
    qDebug() << tr("Error starting local socket for inter-process communication.");
  }
}

ProjecteurApplication::~ProjecteurApplication()
{
  if (m_localServer) m_localServer->close();
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
    this->quit();
  }
  else if (cmdKey == "spot")
  {
    const bool active = (cmdValue == "on" || cmdValue == "1" || cmdValue == "true");
    emit m_spotlight->spotActiveChanged(active);
  }
  else if (cmdKey == "settings" || cmdKey == "preferences")
  {
    const bool show = !(cmdValue == "hide" || cmdValue == "0");
    showPreferences(show);
  }

  clientConnection->disconnectFromServer();
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

ProjecteurCommandClientApp::ProjecteurCommandClientApp(const QString& ipcCommand, int &argc, char **argv)
  : QCoreApplication(argc, argv)
{
  if (ipcCommand.isEmpty())
  {
    QMetaObject::invokeMethod(this, "quit", Qt::QueuedConnection);
    return;
  }

  QLocalSocket* const localSocket = new QLocalSocket(this);

  connect(localSocket,
          static_cast<void (QLocalSocket::*)(QLocalSocket::LocalSocketError)>(&QLocalSocket::error),
  [this, localSocket](QLocalSocket::LocalSocketError /*socketError*/) {
    qDebug() << tr("Error sending command: %1", "%1=error message").arg(localSocket->errorString());
    localSocket->close();
    QMetaObject::invokeMethod(this, "quit", Qt::QueuedConnection);
  });

  connect(localSocket, &QLocalSocket::connected, [localSocket, ipcCommand]()
  {
    const QByteArray commandBlock = [&ipcCommand](){
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
    localSocket->disconnectFromServer();
  });

  connect(localSocket, &QLocalSocket::disconnected, [this, localSocket]() {
    localSocket->close();
    QMetaObject::invokeMethod(this, "quit", Qt::QueuedConnection);
  });

  localSocket->connectToServer(localServerName());
}
