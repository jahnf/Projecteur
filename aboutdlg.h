#pragma once

#include <QDialog>

class AboutDialog : public QDialog
{
  Q_OBJECT

public:
  explicit AboutDialog(QWidget* parent = nullptr);
  virtual ~AboutDialog() override = default;
};
