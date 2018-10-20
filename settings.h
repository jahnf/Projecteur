# pragma once

#include <QObject>
#include <QSettings>

class Settings : public QObject
{
  Q_OBJECT
  Q_PROPERTY(int spotSize READ spotSize WRITE setSpotSize NOTIFY spotSizeChanged)
//  Q_PROPERTY(qreal begin READ begin WRITE setBegin NOTIFY beginChanged)
//  Q_PROPERTY(qreal end READ end WRITE setEnd NOTIFY endChanged)
//  Q_PROPERTY(QVariantList majorTicks READ majorTicks NOTIFY ticksChanged)

public:
  explicit Settings(QObject* parent = nullptr);
  virtual ~Settings() override;

  int spotSize() const { return m_spotSize; }
  void setSpotSize(int size);

signals:
  void spotSizeChanged();

private:
  QSettings* m_settings = nullptr;

  int m_spotSize = 25; ///< Spot size in percentage of available screen height, but at least 50 pixels.

private:
  void load();
};
