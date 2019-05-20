// This file is part of Projecteur - https://github.com/jahnf/projecteur - See LICENSE.md and README.md
# pragma once

#include <QColor>
#include <QObject>
#include <QVariant>

class QSettings;
class QQmlPropertyMap;

class Settings : public QObject
{
  Q_OBJECT
  Q_PROPERTY(bool showSpot READ showSpot WRITE setShowSpot NOTIFY showSpotChanged)
  Q_PROPERTY(int spotSize READ spotSize WRITE setSpotSize NOTIFY spotSizeChanged)
  Q_PROPERTY(bool showCenterDot READ showCenterDot WRITE setShowCenterDot NOTIFY showCenterDotChanged)
  Q_PROPERTY(int dotSize READ dotSize WRITE setDotSize NOTIFY dotSizeChanged)
  Q_PROPERTY(QColor dotColor READ dotColor WRITE setDotColor NOTIFY dotColorChanged)
  Q_PROPERTY(QColor shadeColor READ shadeColor WRITE setShadeColor NOTIFY shadeColorChanged)
  Q_PROPERTY(double shadeOpacity READ shadeOpacity WRITE setShadeOpacity NOTIFY shadeOpacityChanged)
  Q_PROPERTY(int screen READ screen WRITE setScreen NOTIFY screenChanged)
  Q_PROPERTY(Qt::CursorShape cursor READ cursor WRITE setCursor NOTIFY cursorChanged)
  Q_PROPERTY(QString spotShape READ spotShape WRITE setSpotShape NOTIFY spotShapeChanged)
  Q_PROPERTY(double spotRotation READ spotRotation WRITE setSpotRotation NOTIFY spotRotationChanged)
  Q_PROPERTY(QObject* shapes READ shapeSettingsRootObject CONSTANT)

public:
  explicit Settings(QObject* parent = nullptr);
  virtual ~Settings() override;

  void setDefaults();

  bool showSpot() const { return m_showSpot; }
  void setShowSpot(bool show);
  int spotSize() const { return m_spotSize; }
  void setSpotSize(int size);
  bool showCenterDot() const { return m_showCenterDot; }
  void setShowCenterDot(bool show);
  int dotSize() const { return m_dotSize; }
  void setDotSize(int size);
  QColor dotColor() const { return m_dotColor; }
  void setDotColor(const QColor& color);
  QColor shadeColor() const { return m_shadeColor; }
  void setShadeColor(const QColor& color);
  double shadeOpacity() const { return m_shadeOpacity; }
  void setShadeOpacity(double opacity);
  int screen() const { return m_screen; }
  void setScreen(int screen);
  Qt::CursorShape cursor() const { return m_cursor; }
  void setCursor(Qt::CursorShape cursor);
  QString spotShape() const { return m_spotShape; }
  void setSpotShape(const QString& spotShapeQmlComponent);
  double spotRotation() const { return m_spotRotation; }
  void setSpotRotation(double rotation);

  class SpotShapeSetting {
  public:
    SpotShapeSetting(const QString& displayName, const QString& key, const QVariant& defaultValue,
                     const QVariant& minValue, const QVariant& maxValue, int decimals = 0)
      : m_displayName(displayName), m_settingsKey(key), m_minValue(minValue),
        m_maxValue(maxValue), m_defaultValue(defaultValue), m_decimals(decimals) {}
    const QString& displayName() const { return m_displayName; }
    const QString& settingsKey() const { return m_settingsKey; }
    const QVariant& minValue() const { return m_minValue; }
    const QVariant& maxValue() const { return m_maxValue; }
    const QVariant& defaultValue() const { return m_defaultValue; }
    int decimals() const { return m_decimals; }
  private:
    QString m_displayName;
    QString m_settingsSection;
    QString m_settingsKey;
    QVariant m_minValue = 0;
    QVariant m_maxValue = 100;
    QVariant m_defaultValue = m_minValue;
    int m_decimals = 0;
  };

  class SpotShape {
  public:
    QString qmlComponent() const { return m_qmlComponent; }
    QString name() const { return m_name; }
    QString displayName() const  { return m_displayName; }
    bool allowRotation() const { return m_allowRotation; }
    const QList<SpotShapeSetting>& shapeSettings() const { return m_shapeSettings; }
  private:
    SpotShape(const QString& qmlComponent, const QString& name,
              const QString& displayName, bool allowRotation, QList<SpotShapeSetting> shapeSettings= {})
      : m_qmlComponent(qmlComponent), m_name(name), m_displayName(displayName), m_allowRotation(allowRotation),
        m_shapeSettings(std::move(shapeSettings)){}
    QString m_qmlComponent;
    QString m_name;
    QString m_displayName;
    bool m_allowRotation = true;
    QList<SpotShapeSetting> m_shapeSettings;
    friend class Settings;
  };

  const QList<SpotShape>& spotShapes() const { return m_spotShapes; }
  QQmlPropertyMap* shapeSettings(const QString& shapeName);

signals:
  void showSpotChanged(bool show);
  void spotSizeChanged(int size);
  void dotSizeChanged(int size);
  void showCenterDotChanged(bool show);
  void dotColorChanged(const QColor& color);
  void shadeColorChanged(const QColor& color);
  void shadeOpacityChanged(double opcacity);
  void screenChanged(int screen);
  void cursorChanged(Qt::CursorShape cursor);
  void spotShapeChanged(const QString& spotShapeQmlComponent);
  void spotRotationChanged(double rotation);

private:
  QSettings* m_settings = nullptr;

  QMap<QString, QQmlPropertyMap*> m_shapeSettings;
  QQmlPropertyMap* m_shapeSettingsRoot;

  bool m_showSpot = true;
  int m_spotSize = 30; ///< Spot size in percentage of available screen height, but at least 50 pixels.
  bool m_showCenterDot = false;
  int m_dotSize = 5; ///< Center Dot Size (3-100 pixels)
  QColor m_dotColor;
  QColor m_shadeColor;
  double m_shadeOpacity = 0.3;
  int m_screen = 0;
  Qt::CursorShape m_cursor = Qt::BlankCursor;
  QString m_spotShape;
  double m_spotRotation = 0.0;
  const QList<SpotShape> m_spotShapes;

private:
  void load();
  QObject* shapeSettingsRootObject();
  void shapeSettingsPopulateRoot();
  void shapeSettingsInitialize();
  void shapeSettingsSetDefaults();
  void shapeSettingsLoad();
};
