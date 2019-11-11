// This file is part of Projecteur - https://github.com/jahnf/projecteur - See LICENSE.md and README.md
#include "colorselector.h"

#include <QColorDialog>

ColorSelector::ColorSelector(QWidget* parent)
  : ColorSelector(Qt::black, parent)
{
}

ColorSelector::ColorSelector(const QColor& color, QWidget* parent)
  : QPushButton(parent)
  , m_color(color)
{
  setMinimumWidth(30);
  updateButton();
  connect(this, &QPushButton::clicked, [this](){
    const QColor c = QColorDialog::getColor(m_color, this, tr("Select Dot Color"));
    if (c.isValid())
      setColor(c);
  });
}

void ColorSelector::setColor(const QColor& color)
{
  if (m_color == color)
    return;

  m_color = color;
  updateButton();
  emit colorChanged(color);
}

void ColorSelector::updateButton()
{
  QPalette p(palette());
  p.setColor(QPalette::Button, m_color);
  p.setColor(QPalette::ButtonText, m_color);
  setPalette(p);
  setToolTip(m_color.name());
}
