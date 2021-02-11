// This file is part of Projecteur - https://github.com/jahnf/projecteur - See LICENSE.md and README.md
#include "device-vibration.h"

#include "iconwidgets.h"

#include <QVBoxLayout>

// TODO add vibration support for Logitech Spotlight and
// TODO generalize features and protocol for proprietary device features like vibration
//      for not only the Spotlight device.
//                                                    len         intensity
// unsigned char vibrate[] = {0x10, 0x01, 0x09, 0x1a, 0x00, 0xe8, 0x80};
// ::write(notifier->socket(), vibrate, 7);

// -------------------------------------------------------------------------------------------------
TimerWidget::TimerWidget(QWidget* parent)
  : QWidget(parent)
{
  // TODO Widget with up to 3 (or more) configurable timers..
  const auto layout = new QVBoxLayout(this);
  const auto iconLabel = new IconLabel(Font::time_19, this);
  layout->addWidget(iconLabel);
}
