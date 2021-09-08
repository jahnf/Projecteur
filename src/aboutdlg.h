// This file is part of Projecteur - https://github.com/jahnf/projecteur
// - See LICENSE.md and README.md
#pragma once

#include <QDialog>

class QTabWidget;

class AboutDialog : public QDialog
{
  Q_OBJECT

public:
  explicit AboutDialog(QWidget* parent = nullptr);

protected:
  void showEvent(QShowEvent*) override;

private:
  QTabWidget* m_tabWidget = nullptr;

  QWidget* createVersionInfoWidget();
  QWidget* createContributorInfoWidget();
  QWidget* createThirdPartyLicensesWidget();
};
