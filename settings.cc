// This file is part of Projecteur - https://github.com/jahnf/projecteur - See LICENSE.md and README.md
#include "settings.h"

#include <QCoreApplication>
#include <QQmlPropertyMap>
#include <QSettings>

namespace {
  namespace settings {
    constexpr char showSpot[] = "showSpot";
    constexpr char spotSize[] = "spotSize";
    constexpr char showCenterDot[] = "showCenterDot";
    constexpr char dotSize[] = "dotSize";
    constexpr char dotColor[] = "dotColor";
    constexpr char shadeColor[] = "shadeColor";
    constexpr char shadeOpacity[] = "shadeOpacity";
    constexpr char screen[] = "screen";
    constexpr char cursor[] = "cursor";
    constexpr char spotShape[] = "spotShape";
    constexpr char spotRotation[] ="spotRotation";

    namespace defaultValue {
      constexpr bool showSpot = true;
      constexpr int spotSize = 32;
      constexpr bool showCenterDot = false;
      constexpr int dotSize = 5;
      constexpr auto dotColor = Qt::red;
      constexpr char shadeColor[] = "#222222";
      constexpr double shadeOpacity = 0.3;
      constexpr int screen = 0;
      constexpr Qt::CursorShape cursor = Qt::BlankCursor;
      constexpr char spotShape[] = "spotshapes/Circle.qml";
      constexpr double spotRotation = 0.0;
    }
  }
}

Settings::Settings(QObject* parent)
  : QObject(parent)
  , m_settings(new QSettings(QCoreApplication::applicationName(),
                             QCoreApplication::applicationName(), this))
  , m_dynamicShapeSettings(new QQmlPropertyMap(this))
  , m_spotShapes{ SpotShape(::settings::defaultValue::spotShape, tr("Circle"), false),
                  SpotShape("spotshapes/Square.qml", tr("(Rounded) Square"), true)}
{
  load();
}

Settings::~Settings()
{
}

void Settings::setDefaults()
{
  setShowSpot(settings::defaultValue::showSpot);
  setSpotSize(settings::defaultValue::spotSize);
  setShowCenterDot(settings::defaultValue::showCenterDot);
  setDotSize(settings::defaultValue::dotSize);
  setDotColor(QColor(settings::defaultValue::dotColor));
  setShadeColor(QColor(settings::defaultValue::shadeColor));
  setShadeOpacity(settings::defaultValue::shadeOpacity);
  setScreen(settings::defaultValue::screen);
  setCursor(settings::defaultValue::cursor);
  setSpotShape(settings::defaultValue::spotShape);
  setSpotRotation(settings::defaultValue::spotRotation);
}

void Settings::load()
{
  setShowSpot(m_settings->value(::settings::showSpot, settings::defaultValue::showSpot).toBool());
  setSpotSize(m_settings->value(::settings::spotSize, settings::defaultValue::spotSize).toInt());
  setShowCenterDot(m_settings->value(::settings::showCenterDot, settings::defaultValue::showCenterDot).toBool());
  setDotSize(m_settings->value(::settings::dotSize, settings::defaultValue::dotSize).toInt());
  setDotColor(m_settings->value(::settings::dotColor, QColor(settings::defaultValue::dotColor)).value<QColor>());
  setShadeColor(m_settings->value(::settings::shadeColor, QColor(settings::defaultValue::shadeColor)).value<QColor>());
  setShadeOpacity(m_settings->value(::settings::shadeOpacity, settings::defaultValue::shadeOpacity).toDouble());
  setScreen(m_settings->value(::settings::screen, settings::defaultValue::screen).toInt());
  setCursor(static_cast<Qt::CursorShape>(m_settings->value(::settings::cursor, static_cast<int>(settings::defaultValue::cursor)).toInt()));
  setSpotShape(m_settings->value(::settings::spotShape, settings::defaultValue::spotShape).toString());
  setSpotRotation(m_settings->value(::settings::spotRotation, settings::defaultValue::spotRotation).toDouble());
}

void Settings::setShowSpot(bool show)
{
  if (show == m_showSpot)
    return;

  m_showSpot = show;
  m_settings->setValue(::settings::showSpot, m_showSpot);
  emit showSpotChanged(m_showSpot);
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

void Settings::setCursor(Qt::CursorShape cursor)
{
  if (cursor == m_cursor)
    return;

  m_cursor = qMin(qMax(static_cast<Qt::CursorShape>(0), cursor), Qt::LastCursor);
  m_settings->setValue(::settings::cursor, static_cast<int>(m_cursor));
  emit cursorChanged(m_cursor);
}

void Settings::setSpotShape(const QString& spotShapeQmlComponent)
{
  if (m_spotShape == spotShapeQmlComponent)
    return;

  m_spotShape = spotShapeQmlComponent;
  m_settings->setValue(::settings::spotShape, m_spotShape);
  emit spotShapeChanged(m_spotShape);
}

void Settings::setSpotRotation(double rotation)
{
  if (rotation > m_spotRotation || rotation < m_spotRotation)
  {
    m_spotRotation = qMin(qMax(0.0, rotation), 360.0);
    m_settings->setValue(::settings::spotRotation, m_spotRotation);
    emit spotRotationChanged(m_spotRotation);
  }
}

QObject* Settings::shapeSettings() const {
  return m_dynamicShapeSettings;
}
