// This file is part of Projecteur - https://github.com/jahnf/projecteur - See LICENSE.md and README.md
#pragma once

#include <QWidget>

// -------------------------------------------------------------------------------------------------
class InputMapper;

// -------------------------------------------------------------------------------------------------
class InputSeqEdit : public QWidget
{
  Q_OBJECT

public:
  InputSeqEdit(InputMapper* im, QWidget* parent = nullptr);

  void setInputMapper(InputMapper* im);

private:
  InputMapper* m_inputMapper = nullptr;
};
