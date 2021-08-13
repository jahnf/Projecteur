// This file is part of Projecteur - https://github.com/jahnf/projecteur - See LICENSE.md and README.md
#pragma once

#include "projecteur-icons-def.h"

#include <QToolButton>
#include <QLabel>

// -------------------------------------------------------------------------------------------------
/// @brief Icon button class used throughout the application's widget based dialogs.
class IconButton : public QToolButton
{
  Q_OBJECT

public:
  IconButton(Font::Icon symbol, QWidget* parent = nullptr);
};

// -------------------------------------------------------------------------------------------------
/// @brief Icon label class used throughout the application's widget based dialogs.
class IconLabel : public QLabel
{
  Q_OBJECT

public:
  IconLabel(Font::Icon symbol, QWidget* parent = nullptr);

  void setPixelSize(int pixelSize);
};
