// This file is part of Projecteur - https://github.com/jahnf/projecteur - See LICENSE.md and README.md
#pragma once

#include <QWidget>

class Settings;
class Spotlight;

class DevicesWidget : public QWidget
{
  Q_OBJECT
public:
  explicit DevicesWidget(Settings* settings, Spotlight* spotlight, QWidget* parent = nullptr);

signals:

private:
  QWidget* createDisconnectedStateWidget(QWidget* parent);
  QWidget* createDevicesWidget(QWidget* parent);
};

