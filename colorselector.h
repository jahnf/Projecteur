#pragma once

#include <QPushButton>

class ColorSelector : public QPushButton
{
  Q_OBJECT
  Q_PROPERTY(QColor color READ color WRITE setColor NOTIFY colorChanged)

public:
  explicit ColorSelector(QWidget* parent = nullptr);
  explicit ColorSelector(const QColor& color, QWidget* parent = nullptr);

  void setColor(const QColor& color);
  QColor color() const { return m_color; }

signals:
  void colorChanged(const QColor);

private:
  void updateButton();

private:
  QColor m_color;
};
