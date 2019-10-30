// This file is part of Projecteur - https://github.com/jahnf/projecteur - See LICENSE.md and README.md
#include "settings.h"

#include <QCoreApplication>
#include <QQmlPropertyMap>
#include <QSettings>

#include <QDebug>

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
    constexpr char showBorder[] = "showBorder";
    constexpr char borderColor[] ="borderColor";
    constexpr char borderSize[] = "borderSize";
    constexpr char borderOpacity[] = "borderOpacity";
    constexpr char zoomEnabled[] = "enableZoom";
    constexpr char zoomFactor[] = "zoomFactor";

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
      constexpr bool showBorder = false;
      constexpr auto borderColor = Qt::red;
      constexpr int borderSize = 3;
      constexpr double borderOpacity = 0.8;
      constexpr bool zoomEnabled = false;
      constexpr double zoomFactor = 2.0;
    }
  }
}

Settings::Settings(QObject* parent)
  : QObject(parent)
  , m_settings(new QSettings(QCoreApplication::applicationName(),
                             QCoreApplication::applicationName(), this))
  , m_shapeSettingsRoot(new QQmlPropertyMap(this))
  , m_spotShapes{ SpotShape(::settings::defaultValue::spotShape, "Circle", tr("Circle"), false),
                  SpotShape("spotshapes/Square.qml", "Square", tr("(Rounded) Square"), true,
                    {SpotShapeSetting(tr("Border-radius (%)"), "radius", 20, 0, 100, 0)} ),
                  SpotShape("spotshapes/Star.qml", "Star", tr("Star"), true,
                    {SpotShapeSetting(tr("Star points"), "points", 5, 3, 100, 0),
                     SpotShapeSetting(tr("Inner radius (%)"), "innerRadius", 50, 5, 100, 0)} ),
                  SpotShape("spotshapes/Ngon.qml", "Ngon", tr("N-gon"), true,
                    {SpotShapeSetting(tr("Sides"), "sides", 3, 3, 100, 0)} ) }
{
  shapeSettingsInitialize();
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
  setShowBorder(settings::defaultValue::showBorder);
  setBorderColor(settings::defaultValue::borderColor);
  setBorderSize(settings::defaultValue::borderSize);
  setBorderOpacity(settings::defaultValue::borderOpacity);
  setZoomEnabled(settings::defaultValue::zoomEnabled);
  setZoomFactor(settings::defaultValue::zoomFactor);
  shapeSettingsSetDefaults();
}

void Settings::shapeSettingsSetDefaults()
{
  for (const auto& shape : m_spotShapes)
  {
    for (const auto& settingDefinition : shape.shapeSettings())
    {
      if (auto propertyMap = shapeSettings(shape.name()))
      {
        const QString key = settingDefinition.settingsKey();
        if (propertyMap->property(key.toLocal8Bit()).isValid()) {
          propertyMap->setProperty(key.toLocal8Bit(), settingDefinition.defaultValue());
        } else {
          propertyMap->insert(key, settingDefinition.defaultValue());
        }
      }
    }
  }
  shapeSettingsPopulateRoot();
}

void Settings::shapeSettingsLoad()
{
  for (const auto& shape : m_spotShapes)
  {
    for (const auto& settingDefinition : shape.shapeSettings())
    {
      if (auto propertyMap = shapeSettings(shape.name()))
      {
        const QString key = settingDefinition.settingsKey();
        const QString settingsKey = QString("Shape.%1/%2").arg(shape.name()).arg(key);
        const QVariant loadedValue = m_settings->value(settingsKey, settingDefinition.defaultValue());
        if (propertyMap->property(key.toLocal8Bit()).isValid()) {
          propertyMap->setProperty(key.toLocal8Bit(), loadedValue);
        } else {
          propertyMap->insert(key, loadedValue);
        }
      }
    }
  }
  shapeSettingsPopulateRoot();
}

void Settings::shapeSettingsInitialize()
{
  for (const auto& shape : m_spotShapes)
  {
    if (shape.shapeSettings().size() && !m_shapeSettings.contains(shape.name()))
    {
      auto pm = new QQmlPropertyMap(this);
      connect(pm, &QQmlPropertyMap::valueChanged, [this, shape, pm](const QString& key, const QVariant& value)
      {
        const auto& s = shape.shapeSettings();
        auto it = std::find_if(s.cbegin(), s.cend(), [&key](const SpotShapeSetting& sss) {
          return key == sss.settingsKey();
        });

        if (it != s.cend())
        {
          if (it->defaultValue().type() == QVariant::Int)
          {
            const auto setValue = value.toInt();
            const auto min = it->minValue().toInt();
            const auto max = it->maxValue().toInt();
            const auto newValue = qMin(qMax(min, setValue), max);
            if (newValue != setValue) {
              pm->setProperty(key.toLocal8Bit(), newValue);
            }
            m_settings->setValue(QString("Shape.%1/%2").arg(shape.name()).arg(key), newValue);
          }
        }
      });
      m_shapeSettings.insert(shape.name(), pm);
    }
  }
  shapeSettingsPopulateRoot();
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
  setShowBorder(m_settings->value(::settings::showBorder, settings::defaultValue::showBorder).toBool());
  setBorderColor(m_settings->value(::settings::borderColor, QColor(settings::defaultValue::borderColor)).value<QColor>());
  setBorderSize(m_settings->value(::settings::borderSize, settings::defaultValue::borderSize).toInt());
  setBorderOpacity(m_settings->value(::settings::borderOpacity, settings::defaultValue::borderOpacity).toDouble());
  setZoomEnabled(m_settings->value(::settings::zoomEnabled, settings::defaultValue::zoomEnabled).toBool());
  setZoomFactor(m_settings->value(::settings::zoomFactor, settings::defaultValue::zoomFactor).toDouble());
  shapeSettingsLoad();
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

  const auto it = std::find_if(spotShapes().cbegin(), spotShapes().cend(),
  [&spotShapeQmlComponent](const SpotShape& s) {
    return s.qmlComponent() == spotShapeQmlComponent;
  });

  if (it != spotShapes().cend()) {
    m_spotShape = it->qmlComponent();
    m_settings->setValue(::settings::spotShape, m_spotShape);
    emit spotShapeChanged(m_spotShape);
    setSpotRotationAllowed(it->allowRotation());
  }
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

QObject* Settings::shapeSettingsRootObject()
{
  return m_shapeSettingsRoot;
}

QQmlPropertyMap* Settings::shapeSettings(const QString &shapeName)
{
  const auto it = m_shapeSettings.find(shapeName);
  if (it != m_shapeSettings.cend()) {
    return it.value();
  }
  return nullptr;
}

void Settings::shapeSettingsPopulateRoot()
{
  for (auto it = m_shapeSettings.cbegin(), end = m_shapeSettings.cend(); it != end; ++it)
  {
    if (m_shapeSettingsRoot->property(it.key().toLocal8Bit()).isValid()) {
      m_shapeSettingsRoot->setProperty(it.key().toLocal8Bit(), QVariant::fromValue(it.value()));
    } else {
      m_shapeSettingsRoot->insert(it.key(), QVariant::fromValue(it.value()));
    }
  }
}

bool Settings::spotRotationAllowed() const
{
  return m_spotRotationAllowed;
}

void Settings::setSpotRotationAllowed(bool allowed)
{
  if (allowed == m_spotRotationAllowed)
    return;

  m_spotRotationAllowed = allowed;
  emit spotRotationAllowedChanged(allowed);
}

void Settings::setShowBorder(bool show)
{
  if (show == m_showBorder)
    return;

  m_showBorder = show;
  m_settings->setValue(::settings::showBorder, m_showBorder);
  emit showBorderChanged(m_showBorder);
}

void Settings::setBorderColor(const QColor& color)
{
  if (color == m_borderColor)
    return;

  m_borderColor = color;
  m_settings->setValue(::settings::borderColor, m_borderColor);
  emit borderColorChanged(m_borderColor);
}

void Settings::setBorderSize(int size)
{
  if (size == m_borderSize)
    return;

  m_borderSize = qMin(qMax(0, size), 100);
  m_settings->setValue(::settings::borderSize, m_borderSize);
  emit borderSizeChanged(m_borderSize);
}

void Settings::setBorderOpacity(double opacity)
{
  if (opacity > m_borderOpacity || opacity < m_borderOpacity)
  {
    m_borderOpacity = qMin(qMax(0.0, opacity), 1.0);
    m_settings->setValue(::settings::borderOpacity, m_borderOpacity);
    emit borderOpacityChanged(m_borderOpacity);
  }
}

void Settings::setZoomEnabled(bool enabled)
{
  if (enabled == m_zoomEnabled)
    return;

  m_zoomEnabled = enabled;
  m_settings->setValue(::settings::zoomEnabled, m_zoomEnabled);
  emit zoomEnabledChanged(m_zoomEnabled);
}

void Settings::setZoomFactor(double factor)
{
  if (factor > m_zoomFactor || factor < m_zoomFactor)
  {
    m_zoomFactor = qMin(qMax(1.5, factor), 20.0);
    m_settings->setValue(::settings::zoomFactor, m_zoomFactor);
    emit zoomFactorChanged(m_zoomFactor);
  }
}
