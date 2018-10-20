#include "settings.h"

#include <QCoreApplication>
#include <QSettings>

#include <QDebug>
#include <QTimer>

namespace {
  namespace settings {
    constexpr char spotSize[] = "spotSize";
    constexpr char dotSize[] = "dotSize";
    constexpr char dotColor[] = "dotColor";
  }
}

Settings::Settings(QObject* parent)
  : QObject(parent)
  , m_settings(new QSettings(QCoreApplication::applicationName(),
                             QCoreApplication::applicationName(), this))
{
  load();
//  auto t = new QTimer(this);
//  t->setSingleShot(false);
//  t->setInterval(200);
//  connect(t, &QTimer::timeout, [this](){
//    static bool up = true;
//    if( up ) {
//      if( spotSize() < 35 )
//        setSpotSize( spotSize()+1);
//      else
//        up = false;
//    }
//    else {
//      if( spotSize() > 20 )
//        setSpotSize( spotSize()-1);
//      else
//        up = true;    }
//  });
//  t->start();
}

Settings::~Settings()
{
}

void Settings::load()
{
  setSpotSize(m_settings->value(::settings::spotSize, 25).toInt());
}

void Settings::setSpotSize(int size)
{
  if (size == m_spotSize)
    return;

  m_spotSize = size;
  emit spotSizeChanged();
}
