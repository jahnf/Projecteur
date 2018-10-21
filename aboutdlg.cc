#include "aboutdlg.h"

#include "projecteur-GitVersion.h"

#include <QCoreApplication>
#include <QIcon>
#include <QLabel>
#include <QPushButton>
#include <QVBoxLayout>
#include <QDialogButtonBox>

AboutDialog::AboutDialog(QWidget* parent)
  : QDialog(parent)
{
  setWindowTitle(tr("About %1").arg(QCoreApplication::applicationName()));
  setWindowIcon(QIcon(":/icons/projecteur-tray.svg"));

  auto hbox = new QHBoxLayout();
  auto iconLabel = new QLabel(this);
  iconLabel->setPixmap(QIcon(":/icons/projecteur-tray.svg").pixmap(QSize(128,128)));
  hbox->addWidget(iconLabel);

  auto vbox = new QVBoxLayout();
  vbox->addWidget(new QLabel(QString("<b>%1</b><br>Version %2")
                             .arg(QCoreApplication::applicationName())
                             .arg(projecteur::version_string()), this));

  if (QString(projecteur::version_branch()) != "master")
  {
    vbox->addSpacing(10);
    vbox->addWidget(new QLabel(QString("<i>git-branch:</i> %1<br><i>git-hash:</i> %2")
                               .arg(projecteur::version_branch())
                               .arg(projecteur::version_shorthash()), this));
  }

  vbox->addSpacing(10);
  auto weblinkLabel = new QLabel(QString("<a href=\"https://github.com/jahnf/projecteur\">"
                                         "https://github.com/jahnf/projecteur</a>"), this);
  weblinkLabel->setOpenExternalLinks(true);
  vbox->addWidget(weblinkLabel);

  vbox->addSpacing(20);
  vbox->addWidget(new QLabel(QString("Qt Version: %1").arg(QT_VERSION_STR), this));

  vbox->addStretch(1);
  hbox->addLayout(vbox);

  auto bbox = new QDialogButtonBox(QDialogButtonBox::Ok, this);
  connect(bbox, &QDialogButtonBox::clicked, this, &QDialog::accept);

  auto mainVbox = new QVBoxLayout(this);
  mainVbox->addLayout(hbox);
  mainVbox->addSpacing(10);
  mainVbox->addWidget(bbox);
}
