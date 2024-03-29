// This file is part of Projecteur - https://github.com/jahnf/projecteur
// - See LICENSE.md and README.md
#pragma once

#include <QQuickPaintedItem>
#include <QPixmap>

class ProjecteurImage : public QQuickPaintedItem
{
  Q_OBJECT
  Q_PROPERTY(QPixmap pixmap READ pixmap WRITE setPixmap)

public:
  static int qmlRegister();

  explicit ProjecteurImage(QQuickItem *parent = nullptr);
  virtual ~ProjecteurImage() override = default;

  virtual void paint(QPainter *painter) override;
  void setPixmap(QPixmap pm);
  QPixmap pixmap() const { return m_pixmap; }

private:
  QPixmap m_pixmap;
};
