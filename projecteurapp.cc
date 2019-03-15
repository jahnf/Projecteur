// This file is part of Projecteur - https://github.com/jahnf/projecteur - See LICENSE.md and README.md
#include "projecteurapp.h"

#include "aboutdlg.h"
#include "preferencesdlg.h"
#include "qglobalshortcutx11.h"
#include "settings.h"
#include "spotlight.h"

#include <QDialog>
#include <QLocalServer>
#include <QLocalSocket>
#include <QMenu>
#include <QMessageBox>
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

ProjecteurApplication::ProjecteurApplication(int &argc, char **argv)
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

  auto spotlight = new Spotlight(this);
  auto settings = new Settings(this);
  m_dialog.reset(new PreferencesDialog(settings, spotlight));
  m_dialog->updateAvailableScreens(screens());

  auto screen = screens().first();
  if (settings->screen() < screens().size()) {
    screen = screens().at(settings->screen());
  }

  auto engine = new QQmlApplicationEngine(this);
  engine->rootContext()->setContextProperty("Settings", settings);
  engine->rootContext()->setContextProperty("PreferencesDialog", &*m_dialog);
  engine->load(QUrl(QStringLiteral("qrc:/main.qml")));
  auto window = topLevelWindows().first();

  m_trayMenu->addAction(tr("&Preferences..."), [this](){
    m_dialog->show();
    m_dialog->raise();
    m_dialog->activateWindow();
  });

  m_trayMenu->addAction(tr("&About"), [this](){
    AboutDialog().exec();
  });

  m_trayMenu->addSeparator();
  m_trayMenu->addAction(tr("&Quit"), [this](){ this->quit(); });
  m_trayIcon->setContextMenu(&*m_trayMenu);

  m_trayIcon->setIcon(QIcon(":/icons/projecteur-tray.svg"));
  m_trayIcon->show();

  connect(&*m_trayIcon, &QSystemTrayIcon::activated, [this](QSystemTrayIcon::ActivationReason reason) {
    if (reason == QSystemTrayIcon::Trigger) {
      m_trayIcon->contextMenu()->popup(m_trayIcon->geometry().center());
    }
  });

  window->setFlag(Qt::WindowTransparentForInput, true);
  window->setFlag(Qt::Tool, true);
  window->setScreen(screen);
  window->setPosition(screen->availableGeometry().topLeft());
  window->setWidth(screen->availableGeometry().width());
  window->setHeight(screen->availableGeometry().height());
  connect(this, &ProjecteurApplication::aboutToQuit, [window](){ if (window) window->close(); });

  // Example code for global shortcuts...
  //  const auto shortcut = new QGlobalShortcutX11(QKeySequence("Ctrl+F3"), this);
  //  connect(shortcut, &QGlobalShortcutX11::activated, [window](){
  //    qDebug() << "GlobalShortCut Ctrl+F3" << window;
  //  });

  // Handling of spotlight window when input from spotlight device is detected
  connect(spotlight, &Spotlight::spotActiveChanged, [this, window](bool active){
    if (active)
    {
      window->setFlag(Qt::WindowTransparentForInput, false);
      window->setFlag(Qt::WindowStaysOnTopHint, true);
      window->hide();
//      window->showMaximized();
      window->showFullScreen();
    }
    else {
      if (m_dialog->isActiveWindow()) {
        window->setFlag(Qt::WindowStaysOnTopHint, false);
        m_dialog->raise();
      }
      else {
        window->setFlag(Qt::WindowTransparentForInput, true);
        window->hide();
      }
    }
  });

  // Handling of spotlight window when preferences dialog is active
  connect(&*m_dialog, &PreferencesDialog::dialogActiveChanged,
  [this, window, spotlight](bool active)
  {
    if (active) {
      window->setFlag(Qt::WindowTransparentForInput, false);
      window->setFlag(Qt::WindowStaysOnTopHint, false);
      if (!window->isVisible()) {
        window->showMaximized();
        m_dialog->raise();
      }
    }
    else if (spotlight->spotActive()) {
      window->setFlag(Qt::WindowStaysOnTopHint, true);
    }
    else {
      window->setFlag(Qt::WindowTransparentForInput, true);
      window->hide();
    }
  });

  // Handling if the screen in the settings was changed
  connect(settings, &Settings::screenChanged, [this, window](int screenIdx)
  {
    if (screenIdx >= screens().size() )
      return;

    auto screen = screens()[screenIdx];
    const bool wasVisible = window->isVisible();
    window->hide();
    window->setGeometry(QRect(screen->availableGeometry().topLeft(), QSize(400,320)));
    window->setScreen(screen);
    window->setPosition(screen->availableGeometry().topLeft());
    window->setWidth(screen->availableGeometry().width());
    window->setHeight(screen->availableGeometry().height());
    if (wasVisible) {
      QTimer::singleShot(0, [window,this]() {
        window->showMaximized();
        if(m_dialog->isVisible()) {
          m_dialog->raise();
        }
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

        m_commandConnections.emplace(clientConnection, 0);
      }
    });
  }
  else
  {
    qDebug() << "Error starting local socket for inter-process communication.";
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
  }

  if (clientConnection->bytesAvailable() < commandSize || clientConnection->atEnd())
    return;

  const auto command = QString::fromLocal8Bit(clientConnection->read(commandSize));
  // TODO parse & execute command.

  clientConnection->disconnectFromServer();
}

ProjecteurCommandClientApp::ProjecteurCommandClientApp(const QString& ipcCommand, int &argc, char **argv)
  : QCoreApplication(argc, argv)
{
  if (ipcCommand.isEmpty())
  {
    QMetaObject::invokeMethod(this, "quit", Qt::QueuedConnection);
    return;
  }

  QLocalSocket* localSocket = new QLocalSocket(this);

  connect(localSocket, QOverload<QLocalSocket::LocalSocketError>::of(&QLocalSocket::error),
  [this, localSocket](QLocalSocket::LocalSocketError socketError) {
    qDebug() << "Error sending command: " << localSocket->errorString();
    localSocket->close();
    QMetaObject::invokeMethod(this, "quit", Qt::QueuedConnection);
  });

  connect(localSocket, &QLocalSocket::connected, [this, localSocket, ipcCommand]()
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

