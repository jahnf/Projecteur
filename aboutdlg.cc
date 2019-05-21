// This file is part of Projecteur - https://github.com/jahnf/projecteur - See LICENSE.md and README.md

#include "aboutdlg.h"

#include "projecteur-GitVersion.h"

#include <QCoreApplication>
#include <QIcon>
#include <QLabel>
#include <QPushButton>
#include <QTabWidget>
#include <QVBoxLayout>
#include <QDialogButtonBox>

AboutDialog::AboutDialog(QWidget* parent)
  : QDialog(parent)
{
  setWindowTitle(tr("About %1").arg(QCoreApplication::applicationName()));
  setWindowIcon(QIcon(":/icons/projecteur-tray.svg"));

  const auto hbox = new QHBoxLayout();
  const auto iconLabel = new QLabel(this);
  iconLabel->setPixmap(QIcon(":/icons/projecteur-tray.svg").pixmap(QSize(128,128)));
  hbox->addWidget(iconLabel);

  const auto tabWidget = new QTabWidget(this);
  hbox->addWidget(tabWidget, 1);

  tabWidget->addTab(createVersionInfoWidget(), tr("Version"));
//  tabWidget->addTab(createContributorInfoWidget(), tr("Contributors"));

  const auto bbox = new QDialogButtonBox(QDialogButtonBox::Ok, this);
  connect(bbox, &QDialogButtonBox::clicked, this, &QDialog::accept);

  const auto mainVbox = new QVBoxLayout(this);
  mainVbox->addLayout(hbox);
  mainVbox->addSpacing(10);
  mainVbox->addWidget(bbox);
}

QWidget* AboutDialog::createVersionInfoWidget()
{
  const auto versionInfoWidget = new QWidget(this);
  const auto vbox = new QVBoxLayout(versionInfoWidget);
  const auto versionLabel = new QLabel(QString("<b>%1</b><br>%2")
                                 .arg(QCoreApplication::applicationName())
                                 .arg(tr("Version %1", "%1=application version number")
                                      .arg(projecteur::version_string())), this);
  vbox->addWidget(versionLabel);
  const auto vInfo = QString("<i>git-branch:</i> %1<br><i>git-hash:</i> %2")
                              .arg(projecteur::version_branch())
                              .arg(projecteur::version_shorthash());
  versionLabel->setToolTip(vInfo);

  if (QString(projecteur::version_flag()).size() || 
       (QString(projecteur::version_branch()) != "master"
        && QString(projecteur::version_branch()) != "not-within-git-repo"))
  {
    vbox->addSpacing(10);
    vbox->addWidget(new QLabel(vInfo, this));
  }

  vbox->addSpacing(10);
  const auto weblinkLabel = new QLabel(QString("<a href=\"https://github.com/jahnf/projecteur\">"
                                               "https://github.com/jahnf/projecteur</a>"), this);
  weblinkLabel->setOpenExternalLinks(true);
  vbox->addWidget(weblinkLabel);

  vbox->addSpacing(20);
  vbox->addWidget(new QLabel(tr("Qt Version: %1", "%1=qt version number").arg(QT_VERSION_STR), this));

  vbox->addStretch(1);
  return versionInfoWidget;
}

QWidget* AboutDialog::createContributorInfoWidget()
{
  const auto contributorWidget = new QWidget(this);
  const auto vbox = new QVBoxLayout(contributorWidget);

  // TODO: list contributors (scroll box)

  vbox->addStretch(1);
  return contributorWidget;
}
