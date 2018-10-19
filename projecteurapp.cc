#include "projecteurapp.h"

#include "qglobalshortcutx11.h"

#include <QQmlApplicationEngine>
#include <QQuickWindow>
#include <QScreen>
#include <QSystemTrayIcon>

#include <QDebug>

ProjecteurApplication::ProjecteurApplication(int &argc, char **argv)
  : QApplication(argc, argv)
  , m_trayIcon(new QSystemTrayIcon(this))
  , m_globalShortcut(new QGlobalShortcutX11(QKeySequence("Ctrl+F3"), this))
{
  auto engine = new QQmlApplicationEngine(QUrl(QStringLiteral("qrc:/main.qml")), this);
  auto window = qobject_cast<QQuickWindow*>(engine->rootObjects().first());
  if (window)
  {
    if (screens().size())
    {
      const auto g = screens().first()->availableGeometry();
      window->setGeometry(g);
      QObject::connect(screens().first(),
                       &QScreen::availableGeometryChanged,
                       [](const QRect& /*g*/){});

      auto setFlags = [window](){
        window->setFlags(window->flags() | Qt::WindowTransparentForInput | Qt::Tool);
      };
      window->connect( window, &QWindow::heightChanged, [g, window, setFlags](int h) {
        if(g.height() == h && window->width() == g.width() ) { setFlags(); }
      });
      window->connect( window, &QWindow::widthChanged, [g, window, setFlags](int w) {
        if(g.width() == w && window->height() == g.height() ) { setFlags(); }
      });
    }
    connect(m_globalShortcut, &QGlobalShortcutX11::activated, [window](){
      if(window->flags() & Qt::WindowTransparentForInput)
      {
        qDebug() << "activated";
        window->setFlag(Qt::WindowTransparentForInput, false);
        window->show();
      }
      else {
        qDebug() << "de-activated";
        window->setFlag(Qt::WindowTransparentForInput, true);
        window->hide();
      }
    });

  }
  m_trayIcon->setIcon(QIcon(":/icons/projecteur-tray.png"));
  m_trayIcon->show();


  //connect(m_trayIcon, &QSystemTrayIcon::activated...)
  //m_trayIcon->showMessage("Title", "Message....", QSystemTrayIcon::Information, 10000);

  const auto shortcut2 = new QGlobalShortcutX11(QKeySequence("Ctrl+Alt+7"), this);
  connect(shortcut2, &QGlobalShortcutX11::activated, [this, window](){
    qDebug() << "activated ctrl+alt+7";
    if(window) window->close();
    this->quit();
  });

}

ProjecteurApplication::~ProjecteurApplication()
{

}

