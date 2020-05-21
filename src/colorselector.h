// This file is part of Projecteur - https://github.com/jahnf/projecteur - See LICENSE.md and README.md
#pragma once

#include <QProxyStyle>
#include <QPushButton>

class ColorSelectorButtonStyle : public QProxyStyle
{
    Q_OBJECT

public:
    ColorSelectorButtonStyle();
    void drawPrimitive(PrimitiveElement element, const QStyleOption *option,
                       QPainter *painter, const QWidget *widget) const override;
};


class ColorSelector : public QPushButton
{
  Q_OBJECT
  Q_PROPERTY(QColor color READ color WRITE setColor NOTIFY colorChanged)

public:
  explicit ColorSelector(QWidget* parent = nullptr);
  explicit ColorSelector(const QString& selectionDialogTitle, const QColor& color, QWidget* parent = nullptr);

  void setColor(const QColor& color);
  QColor color() const { return m_color; }

signals:
  void colorChanged(QColor);

private:
  void updateButton();

private:
  QColor m_color;
};
