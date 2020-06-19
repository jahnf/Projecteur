// This file is part of Projecteur - https://github.com/jahnf/projecteur - See LICENSE.md and README.md
#pragma once

#include "projecteur-icons-def.h"

#include <QToolButton>

// -------------------------------------------------------------------------------------------------
class IconButton : public QToolButton
{
  Q_OBJECT

public:
  IconButton(Font::Icon symbol, QWidget* parent = nullptr);
};

