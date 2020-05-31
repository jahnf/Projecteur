// This file is part of Projecteur - https://github.com/jahnf/projecteur - See LICENSE.md and README.md
#include "iconwidgets.h"

namespace  {
  // -----------------------------------------------------------------------------------------------
  bool isLight(const QColor& c) {
    return (c.redF() * 0.299 + c.greenF() * 0.587 + c.blueF() * 0.114 ) > 0.6;
  }

  bool isDark(const QColor& c) { return !isLight(c); }
}

// -------------------------------------------------------------------------------------------------
IconButton::IconButton(Font::Icon symbol, QWidget* parent)
  : QToolButton(parent)
{
  QFont iconFont("projecteur-icons");
  iconFont.setPointSizeF(font().pointSizeF());

  setFont(iconFont);
  setText(QChar(symbol));

  auto p = palette();
  p.setColor(QPalette::ColorGroup::Normal, QPalette::ButtonText,
             isDark(p.color(QPalette::ButtonText)) ? QColor(Qt::darkGray).darker()
                                                   : QColor(Qt::lightGray).lighter());
  setPalette(p);
}
