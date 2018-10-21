#include "preferencesdlg.h"

#include "colorselector.h"
#include "settings.h"

#include <QCoreApplication>
#include <QDoubleSpinBox>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QIcon>
#include <QLabel>
#include <QGridLayout>
#include <QPushButton>
#include <QSpinBox>
#include <QVBoxLayout>

PreferencesDialog::PreferencesDialog(Settings* settings, QWidget* parent)
  : QDialog(parent)
{
  setWindowTitle(QCoreApplication::applicationName() + " - " + tr("Preferences"));
  setWindowIcon(QIcon(":/icons/projecteur-tray.svg"));

  auto spotSizeSpinBox = new QSpinBox(this);
  spotSizeSpinBox->setMaximum(100);
  spotSizeSpinBox->setMinimum(5);
  spotSizeSpinBox->setValue(settings->spotSize());
  auto spotsizeHBox = new QHBoxLayout;
  spotsizeHBox->addWidget(spotSizeSpinBox);
  spotsizeHBox->addWidget(new QLabel(QString("% ")+tr("of screen height")));
  connect(spotSizeSpinBox, static_cast<void (QSpinBox::*)(int)>(&QSpinBox::valueChanged),
          settings, &Settings::setSpotSize);
  connect(settings, &Settings::spotSizeChanged, spotSizeSpinBox, &QSpinBox::setValue);

  auto grid = new QGridLayout();
  grid->addWidget(new QLabel(tr("Spot Size"), this), 0, 0);
  grid->addLayout(spotsizeHBox, 0, 1);

  auto shadeColor = new ColorSelector(settings->shadeColor(), this);
  connect(shadeColor, &ColorSelector::colorChanged, settings, &Settings::setShadeColor);
  connect(settings, &Settings::shadeColorChanged, shadeColor, &ColorSelector::setColor);
  grid->addWidget(new QLabel(tr("Shade Color"), this), 1, 0);
  grid->addWidget(shadeColor, 1, 1);

  auto shadeOpacitySb = new QDoubleSpinBox(this);
  shadeOpacitySb->setMaximum(1.0);
  shadeOpacitySb->setMinimum(0.0);
  shadeOpacitySb->setDecimals(2);
  shadeOpacitySb->setSingleStep(0.1);
  shadeOpacitySb->setValue(settings->shadeOpacity());
  connect(shadeOpacitySb, static_cast<void (QDoubleSpinBox::*)(double)>(&QDoubleSpinBox::valueChanged),
          settings, &Settings::setShadeOpacity);
  connect(settings, &Settings::shadeOpacityChanged, shadeOpacitySb, &QDoubleSpinBox::setValue);
  grid->addWidget(new QLabel(tr("Shade Opacity"), this), 2, 0);
  grid->addWidget(shadeOpacitySb, 2, 1);

  auto vspacer = new QVBoxLayout;
  vspacer->addSpacing(10);
  grid->addLayout(vspacer, 3, 0, 1, 2);

  auto dotGroup = new QGroupBox(tr("Show Center Dot"), this);
  dotGroup->setCheckable(true);
  dotGroup->setChecked(settings->showCenterDot());
  connect(dotGroup, &QGroupBox::toggled, settings, &Settings::setShowCenterDot);
  connect(settings, &Settings::showCenterDotChanged, dotGroup, &QGroupBox::setChecked);
  grid->addWidget(dotGroup, 4, 0, 1, 2);

  auto dotSizeSpinBox = new QSpinBox(this);
  dotSizeSpinBox->setMaximum(100);
  dotSizeSpinBox->setMinimum(3);
  dotSizeSpinBox->setValue(settings->dotSize());
  auto dotsizeHBox = new QHBoxLayout;
  dotsizeHBox->addWidget(dotSizeSpinBox);
  dotsizeHBox->addWidget(new QLabel(tr("pixel")));
  connect(dotSizeSpinBox, static_cast<void (QSpinBox::*)(int)>(&QSpinBox::valueChanged),
          settings, &Settings::setDotSize);
  connect(settings, &Settings::dotSizeChanged, dotSizeSpinBox, &QSpinBox::setValue);

  auto dotGrid = new QGridLayout(dotGroup);
  dotGrid->addWidget(new QLabel(tr("Dot Size"), this), 0, 0);
  dotGrid->addLayout(dotsizeHBox, 0, 1);

  auto dotColor = new ColorSelector(settings->dotColor(), this);
  connect(dotColor, &ColorSelector::colorChanged, settings, &Settings::setDotColor);
  connect(settings, &Settings::dotColorChanged, dotColor, &ColorSelector::setColor);
  dotGrid->addWidget(new QLabel(tr("Dot Color"), this), 1, 0);
  dotGrid->addWidget(dotColor, 1, 1);

  auto closeBtn = new QPushButton(tr("&Close"), this);
  connect(closeBtn, &QPushButton::clicked, [this](){ this->close(); });
  auto defaultsBtn = new QPushButton(tr("&Reset Defaults"), this);
  connect(defaultsBtn, &QPushButton::clicked, settings, &Settings::setDefaults);

  auto btnHBox = new QHBoxLayout;
  btnHBox->addWidget(defaultsBtn);
  btnHBox->addStretch(1);
  btnHBox->addWidget(closeBtn);

//  auto bottomLine = new QFrame(this);
//  bottomLine->setFrameShape(QFrame::HLine);
//  bottomLine->setFrameShadow(QFrame::Sunken);
//  bottomLine->setLineWidth(1);

  auto vbox = new QVBoxLayout(this);
  vbox->addLayout(grid);
  vbox->addStretch(1);
  vbox->addSpacing(10);
  vbox->addLayout(btnHBox);
}

void PreferencesDialog::setDialogActive(bool active)
{
  if (active == m_active)
    return;

  m_active = active;
  emit dialogActiveChanged(active);
}

bool PreferencesDialog::event(QEvent* e)
{
  if (e->type() == QEvent::WindowActivate) {
    setDialogActive(true);
  }
  else if (e->type() == QEvent::WindowDeactivate) {
    setDialogActive(false);
  }
  return QDialog::event(e);
}
