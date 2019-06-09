// This file is part of Projecteur - https://github.com/jahnf/projecteur - See LICENSE.md and README.md
#pragma once

#include <QQuickItem>

class SpotShapeStar : public QQuickItem
{
  Q_OBJECT
  Q_PROPERTY(QColor color READ color WRITE setColor NOTIFY colorChanged)
  Q_PROPERTY(int points READ points WRITE setPoints NOTIFY pointsChanged)
  Q_PROPERTY(int innerRadius READ innerRadius WRITE setInnerRadius NOTIFY innerRadiusChanged)

public:
  static int qmlRegister();

  explicit SpotShapeStar(QQuickItem* parent = nullptr);

  QColor color() const;
  void setColor(const QColor &color);

  int points() const;
  void setPoints(int points);

  int innerRadius() const; // inner star radius in percent (between 5 and 100)
  void setInnerRadius(int radiusPercentage);

signals:
  void colorChanged(QColor color);
  void pointsChanged(int points);
  void innerRadiusChanged(int innerRadius);

protected:
  virtual QSGNode* updatePaintNode(QSGNode* oldNode, UpdatePaintNodeData* updatePaintNodeData) override;

private:
  QColor m_color = Qt::black;
  int m_points = 3;
  int m_innerRadius = 50;
};

class SpotShapeNGon : public QQuickItem
{
  Q_OBJECT
  Q_PROPERTY(QColor color READ color WRITE setColor NOTIFY colorChanged)
  Q_PROPERTY(int sides READ sides WRITE setSides NOTIFY sidesChanged)

public:
  static int qmlRegister();

  explicit SpotShapeNGon(QQuickItem* parent = nullptr);

  QColor color() const;
  void setColor(const QColor &color);

  int sides() const;
  void setSides(int points);

signals:
  void colorChanged(QColor color);
  void sidesChanged(int sides);

protected:
  virtual QSGNode* updatePaintNode(QSGNode* oldNode, UpdatePaintNodeData* updatePaintNodeData) override;

private:
  QColor m_color = Qt::black;
  int m_sides = 3;
};
