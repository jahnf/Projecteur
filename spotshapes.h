#pragma once

#include <QQuickItem>

class SpotShapeStar : public QQuickItem
{
  Q_OBJECT
  Q_PROPERTY(QColor color READ color WRITE setColor NOTIFY colorChanged)
  Q_PROPERTY(int points READ points WRITE setPoints NOTIFY pointsChanged)
  Q_PROPERTY(float innerRadius READ innerRadius WRITE setInnerRadius NOTIFY innerRadiusChanged)

public:
  static int qmlRegister();

  explicit SpotShapeStar(QQuickItem* parent = nullptr);

  QColor color() const;
  void setColor(const QColor &color);

  int points() const;
  void setPoints(int points);

  float innerRadius() const; // inner star radius in percent (between 0.05 and 1.0)
  void setInnerRadius(float radiusPercentage);

signals:
  void colorChanged(QColor color);
  void pointsChanged(int points);
  void innerRadiusChanged(float innerRadius);

protected:
  virtual QSGNode* updatePaintNode(QSGNode* oldNode, UpdatePaintNodeData* updatePaintNodeData) override;

private:
  QColor m_color = Qt::black;
  int m_points = 3;
  float m_innerRadius = 0.5f;
};
