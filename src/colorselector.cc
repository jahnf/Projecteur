// This file is part of Projecteur - https://github.com/jahnf/projecteur - See LICENSE.md and README.md
#include "colorselector.h"

#include <QColorDialog>
#include <QPainterPath>
#include <QStyleOption>
#include <QStylePainter>

#include <memory>

namespace {
  std::unique_ptr<QStyle> colorButtonStyle = std::make_unique<ColorSelectorButtonStyle>();

  QColor mixColors(const QColor& a, const QColor& b, double ratio = 0.5) {
    return QColor(
      a.red()  *(1.0-ratio) + b.red()  *ratio,
      a.green()*(1.0-ratio) + b.green()*ratio,
      a.blue() *(1.0-ratio) + b.blue() *ratio,
      255
    );
  }
} // end anonymous namespace

ColorSelectorButtonStyle::ColorSelectorButtonStyle()
{
  setObjectName("ColorSelectorButtontyle");
}

void ColorSelectorButtonStyle::drawPrimitive(PrimitiveElement element, const QStyleOption *option,
                                             QPainter *p, const QWidget *widget) const
{
  if (element != PE_PanelButtonCommand)
    QProxyStyle::drawPrimitive(element, option, p, widget);

  p->save();

  p->setRenderHint(QPainter::Antialiasing);
  p->translate(0.5, -0.5);
  QPainterPath path;
  const auto rect = option->rect.adjusted(1,1,-1,0);
  path.addRoundedRect(rect, 4, 4);


  const auto borderColor = [option]()
  { // Set border color based on window color
    const auto w = option->palette.color(QPalette::Window);
    const auto c = (w.redF() * 0.299 + w.greenF() * 0.587 + w.blueF() * 0.114 ) > 0.6 ? Qt::darkGray
                                                                                      : Qt::lightGray;
    if (option->state & State_Enabled) return QColor(c);
    return mixColors(c, option->palette.color(QPalette::Disabled, QPalette::Button));
  }();

  const auto buttonBrush = [option]() {
    if (option->state & State_Enabled) return option->palette.button();
    return QBrush(mixColors(option->palette.color(QPalette::Normal, QPalette::Button),
                            option->palette.color(QPalette::Disabled, QPalette::Button)));
  }();

  p->setPen(QPen(borderColor, 1));
  p->fillPath(path, buttonBrush);
  p->drawPath(path);

  p->restore();
}

ColorSelector::ColorSelector(QWidget* parent)
  : ColorSelector(tr("Select Color"), Qt::black, parent)
{
}

ColorSelector::ColorSelector(const QString& selectionDialogTitle, const QColor& color, QWidget* parent)
  : QPushButton(parent)
  , m_color(color)
{
  setStyle(colorButtonStyle.get());

  setMinimumWidth(30);
  updateButton();
  connect(this, &QPushButton::clicked, [this, selectionDialogTitle](){
    const QColor c = QColorDialog::getColor(m_color, this, selectionDialogTitle);
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
