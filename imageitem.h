// This file is part of Projecteur - https://github.com/jahnf/projecteur - See LICENSE.md and README.md
#pragma once

#include <QQuickPaintedItem>
#include <QPixmap>

class PixmapProvider : public QObject
{
  Q_OBJECT
  Q_PROPERTY(QPixmap pixmap READ pixmap NOTIFY pixmapChanged)

public:
  explicit PixmapProvider(QObject* parent = nullptr);
  virtual ~PixmapProvider() override = default;

  QPixmap pixmap() const { return m_pixmap; }
  void setPixmap(QPixmap pm);

signals:
  void pixmapChanged();

private:
  QPixmap m_pixmap;
};


class ProjecteurImage : public QQuickPaintedItem
{
  Q_OBJECT
  Q_PROPERTY(QPixmap pixmap WRITE setPixmap)

public:
  static int qmlRegister();

  explicit ProjecteurImage(QQuickItem *parent = nullptr);
  virtual ~ProjecteurImage() override = default;

  virtual void paint(QPainter *painter) override;
  void setPixmap(QPixmap pm);

private:
  QPixmap m_pixmap;
};
