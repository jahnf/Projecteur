// This file is part of Projecteur - https://github.com/jahnf/projecteur - See LICENSE.md and README.md
#include "settings.h"

#include <QCoreApplication>
#include <QSettings>

namespace {
  namespace settings {
    constexpr char spotSize[] = "spotSize";
    constexpr char showCenterDot[] = "showCenterDot";
    constexpr char dotSize[] = "dotSize";
    constexpr char dotColor[] = "dotColor";
    constexpr char shadeColor[] = "shadeColor";
    constexpr char shadeOpacity[] = "shadeOpacity";
    constexpr char screen[] = "screen";
  }
}

Settings::Settings(QObject* parent)
  : QObject(parent)
  , m_settings(new QSettings(QCoreApplication::applicationName(),
                             QCoreApplication::applicationName(), this))
{
  load();
}

Settings::~Settings()
{
}

void Settings::setDefaults()
{
  setSpotSize(30);
  setShowCenterDot(false);
  setDotSize(5);
  setDotColor(Qt::red);
  setShadeColor(QColor("#222222"));
  setShadeOpacity(0.3);
  setScreen(0);
}

void Settings::load()
{
  setSpotSize(m_settings->value(::settings::spotSize, 30).toInt());
  setShowCenterDot(m_settings->value(::settings::showCenterDot, false).toBool());
  setDotSize(m_settings->value(::settings::dotSize, 5).toInt());
  setDotColor(m_settings->value(::settings::dotColor, QColor(Qt::red)).value<QColor>());
  setShadeColor(m_settings->value(::settings::shadeColor, QColor("#222222")).value<QColor>());
  setShadeOpacity(m_settings->value(::settings::shadeOpacity, 0.3).toDouble());
  setScreen(m_settings->value(::settings::screen, 0.3).toInt());
}

void Settings::setSpotSize(int size)
{
  if (size == m_spotSize)
    return;

  m_spotSize = qMin(qMax(3, size), 100);
  m_settings->setValue(::settings::spotSize, m_spotSize);
  emit spotSizeChanged(m_spotSize);
}

void Settings::setShowCenterDot(bool show)
{
  if (show == m_showCenterDot)
    return;

  m_showCenterDot = show;
  m_settings->setValue(::settings::showCenterDot, m_showCenterDot);
  emit showCenterDotChanged(m_showCenterDot);
}

void Settings::setDotSize(int size)
{
  if (size == m_dotSize)
    return;

  m_dotSize = qMin(qMax(3, size), 100);
  m_settings->setValue(::settings::dotSize, m_dotSize);
  emit dotSizeChanged(m_dotSize);
}

void Settings::setDotColor(const QColor& color)
{
  if (color == m_dotColor)
    return;

  m_dotColor = color;
  m_settings->setValue(::settings::dotColor, m_dotColor);
  emit dotColorChanged(m_dotColor);
}

void Settings::setShadeColor(const QColor& color)
{
  if (color == m_shadeColor)
    return;

  m_shadeColor = color;
  m_settings->setValue(::settings::shadeColor, m_shadeColor);
  emit shadeColorChanged(m_shadeColor);
}

void Settings::setShadeOpacity(double opacity)
{
  if (opacity > m_shadeOpacity || opacity < m_shadeOpacity)
  {
    m_shadeOpacity = qMin(qMax(0.0, opacity), 1.0);
    m_settings->setValue(::settings::shadeOpacity, m_shadeOpacity);
    emit shadeOpacityChanged(m_shadeOpacity);
  }
}

void Settings::setScreen(int screen)
{
  if (screen == m_screen)
    return;

  m_screen = qMin(qMax(0, screen), 10);
  m_settings->setValue(::settings::screen, m_screen);
  emit screenChanged(m_screen);
}
