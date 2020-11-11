// This file is part of Projecteur - https://github.com/jahnf/projecteur - See LICENSE.md and README.md
#include "imageitem.h"

#include <QPainter>

namespace {
  const bool registered = [](){
    ProjecteurImage::qmlRegister();
    return true;
  }();
}

ProjecteurImage::ProjecteurImage(QQuickItem *parent)
  : QQuickPaintedItem(parent)
{
  setRenderTarget(QQuickPaintedItem::FramebufferObject);
}

int ProjecteurImage::qmlRegister()
{
  return qmlRegisterType<ProjecteurImage>("Projecteur.Utils", 1, 0, "Image");
}

void ProjecteurImage::setPixmap(QPixmap pm)
{
  m_pixmap = pm;
  update();
}

void ProjecteurImage::paint(QPainter *painter)
{
  painter->drawPixmap(QRectF(0, 0, width(), height()), m_pixmap, m_pixmap.rect());
}
