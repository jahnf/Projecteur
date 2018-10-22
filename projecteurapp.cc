#include "projecteurapp.h"

#include "aboutdlg.h"
#include "preferencesdlg.h"
#include "qglobalshortcutx11.h"
#include "settings.h"
#include "spotlight.h"

#include <QDialog>
#include <QMenu>
#include <QMessageBox>
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include <QQuickWindow>
#include <QScopedPointer>
#include <QScreen>
#include <QSystemTrayIcon>
#include <QTimer>

#include <QDebug>

ProjecteurApplication::ProjecteurApplication(int &argc, char **argv)
  : QApplication(argc, argv)
  , m_trayIcon(new QSystemTrayIcon())
  , m_trayMenu(new QMenu())
{
  if (screens().size() < 1)
  {
    QMessageBox::critical(nullptr, tr("No Screens"), tr("screens().size() returned a size < 1."));
    QTimer::singleShot(0, [this](){ this->exit(2); });
    return;
  }

  setQuitOnLastWindowClosed(false);

  auto settings = new Settings(this);
  m_dialog.reset(new PreferencesDialog(settings));
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

  window->setScreen(screen);
  window->setPosition(screen->availableGeometry().topLeft());
  window->setFlag(Qt::WindowTransparentForInput, true);
  window->setFlag(Qt::Tool, true);

  connect(this, &ProjecteurApplication::aboutToQuit, [window](){ if (window) window->close(); });

  // Example code for global shortcuts...
  //  const auto shortcut = new QGlobalShortcutX11(QKeySequence("Ctrl+F3"), this);
  //  connect(shortcut, &QGlobalShortcutX11::activated, [window](){
  //    qDebug() << "GlobalShortCut Ctrl+F3" << window;
  //  });

  auto spotlight = new Spotlight(this);

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
    if (wasVisible) {
      QTimer::singleShot(0,[window,this]() {
        window->showMaximized();
        if(m_dialog->isVisible()) {
          m_dialog->raise();
        }
      });
    }
  });
}

ProjecteurApplication::~ProjecteurApplication()
{
}
