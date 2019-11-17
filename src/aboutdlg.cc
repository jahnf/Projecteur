// This file is part of Projecteur - https://github.com/jahnf/projecteur - See LICENSE.md and README.md

#include "aboutdlg.h"

#include "projecteur-GitVersion.h"

#include <algorithm>
#include <random>
#include <vector>

#include <QCoreApplication>
#include <QIcon>
#include <QLabel>
#include <QPushButton>
#include <QTabWidget>
#include <QTextBrowser>
#include <QVBoxLayout>
#include <QDialogButtonBox>

namespace {
  struct Contributor
  {
    Contributor(const QString& name = "", const QString& github_name ="", const QString& email ="", const QString& url ="")
      : name(name), github_name(github_name), email(email), url(url) {}

    QString toHtml() const
    {
      auto html = QString("<b>%1</b>").arg(name.isEmpty() ? QString("<a href=\"https://github.com/%1\">%1</a>").arg(github_name)
                                                          : name);
      if (email.size()) {
        html += QString(" &lt;%1&gt;").arg(email);
      }

      if (url.size()) {
        html += QString(" <a href=\"%1\">%1</a>").arg(url);
      }
      else if (!name.isEmpty()) {
        html += QString(" - <i>github:</i> <a href=\"https://github.com/%1\">%1</a>").arg(github_name);
      }
      return html;
    }

    QString name;
    QString github_name;
    QString email;
    QString url;
  };

  QString getContributorsHtml()
  {
    static std::vector<Contributor> contributors =
    {
      Contributor("Ricardo Jesus", "rj-jesus"),
      Contributor("Mayank Suman", "mayanksuman"),
      Contributor("Tiziano Müller", "dev-zero"),
      Contributor("Torsten Maehne", "maehne"),
      Contributor("TBK", "TBK"),
      Contributor("Louie Lu", "mlouielu"),
    };

    std::random_device rd;
    std::mt19937 g(rd());
    std::shuffle(contributors.begin(), contributors.end(), g);

    QStringList contributorsHtml;
    for (const auto& contributor : contributors) {
      contributorsHtml.append(contributor.toHtml());
    }
    return contributorsHtml.join("<br>");
  }
}

AboutDialog::AboutDialog(QWidget* parent)
  : QDialog(parent)
{
  setWindowTitle(tr("About %1", "%1=application name").arg(QCoreApplication::applicationName()));
  setWindowIcon(QIcon(":/icons/projecteur-tray.svg"));

  const auto hbox = new QHBoxLayout();
  const auto iconLabel = new QLabel(this);
  iconLabel->setPixmap(QIcon(":/icons/projecteur-tray.svg").pixmap(QSize(128,128)));
  hbox->addWidget(iconLabel);

  const auto tabWidget = new QTabWidget(this);
  hbox->addWidget(tabWidget, 1);

  tabWidget->addTab(createVersionInfoWidget(), tr("Version"));
  tabWidget->addTab(createContributorInfoWidget(tabWidget), tr("Contributors"));

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
                                 .arg(QCoreApplication::applicationName(),
                                      tr("Version %1", "%1=application version number")
                                      .arg(projecteur::version_string())), this);
  vbox->addWidget(versionLabel);
  const auto vInfo = QString("<i>git-branch:</i> %1<br><i>git-hash:</i> %2")
                              .arg(projecteur::version_branch(), projecteur::version_shorthash());
  versionLabel->setToolTip(vInfo);

  if (QString(projecteur::version_flag()).size() || 
       (QString(projecteur::version_branch()) != "master"
        && QString(projecteur::version_branch()) != "not-within-git-repo"))
  {
    vbox->addSpacing(4);
    vbox->addWidget(new QLabel(vInfo, this));
  }

  vbox->addSpacing(4);
  const auto weblinkLabel = new QLabel(QString("<a href=\"https://github.com/jahnf/Projecteur\">"
                                               "https://github.com/jahnf/Projecteur</a>"), this);
  weblinkLabel->setOpenExternalLinks(true);
  vbox->addWidget(weblinkLabel);
  vbox->addSpacing(8);

  auto qtVerText = tr("Qt Version: %1", "%1=qt version number").arg(QT_VERSION_STR);
  if (QString(QT_VERSION_STR) != qVersion()) {
    qtVerText += QString(" (runtime: %1)").arg(qVersion());
  }
  vbox->addWidget(new QLabel(qtVerText, this));
  vbox->addSpacing(15);
  vbox->addWidget(new QLabel("Copyright 2018-2019 Jahn Fuchs", this));
  auto licenseText = new QLabel(tr("This project is distributed under the <br>"
                                   "<a href=\"https://github.com/jahnf/Projecteur/blob/develop/LICENSE.md\">"
                                   "MIT License</a>"), this);
  licenseText->setWordWrap(true);
  licenseText->setTextFormat(Qt::TextFormat::RichText);
  licenseText->setOpenExternalLinks(true);
  vbox->addWidget(licenseText);

  vbox->addStretch(1);
  return versionInfoWidget;
}

QWidget* AboutDialog::createContributorInfoWidget(QTabWidget* tabWidget)
{
  const auto contributorWidget = new QWidget(this);
  const auto vbox = new QVBoxLayout(contributorWidget);

  const auto label = new QLabel(tr("Contributors, in random order:"), contributorWidget);
  vbox->addWidget(label);

  const auto textBrowser = new QTextBrowser(contributorWidget);
  textBrowser->setWordWrapMode(QTextOption::NoWrap);
  textBrowser->setOpenLinks(true);
  textBrowser->setOpenExternalLinks(true);
  textBrowser->setFont([textBrowser]()
  {
    auto font = textBrowser->font();
    font.setPointSize(font.pointSize() - 2);
    return font;
  }());

  // randomize contributors list on every contributors tab selection
  connect(tabWidget, &QTabWidget::currentChanged, this,
  [contributorWidget, tabWidget, textBrowser](int){
    if (contributorWidget == tabWidget->currentWidget()) {
      textBrowser->setHtml(getContributorsHtml());
    }
  });

  vbox->addWidget(textBrowser);
  return contributorWidget;
}
