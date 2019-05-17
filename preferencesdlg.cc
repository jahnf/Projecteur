// This file is part of Projecteur - https://github.com/jahnf/projecteur - See LICENSE.md and README.md
#include "preferencesdlg.h"

#include "colorselector.h"
#include "settings.h"
#include "spotlight.h"

#include <QComboBox>
#include <QCoreApplication>
#include <QDoubleSpinBox>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QIcon>
#include <QLabel>
#include <QGridLayout>
#include <QPushButton>
#include <QScreen>
#include <QSpinBox>
#include <QtGlobal>
#include <QVBoxLayout>

#include <map>

namespace {
  #define CURSOR_PATH ":/icons/cursors/"
  static const std::map<const QString, const QPair<const QString, const Qt::CursorShape>> cursorMap {
    { "", {"No Cursor", Qt::BlankCursor}},
    { CURSOR_PATH "cursor-arrow.png", {"Arrow Cursor", Qt::ArrowCursor}},
    { CURSOR_PATH "cursor-busy.png", {"Busy Cursor", Qt::BusyCursor}},
    { CURSOR_PATH "cursor-cross.png", {"Cross Cursor", Qt::CrossCursor}},
    { CURSOR_PATH "cursor-hand.png", {"Pointing Hand Cursor", Qt::PointingHandCursor}},
    { CURSOR_PATH "cursor-openhand.png", {"Open Hand Cursor", Qt::OpenHandCursor}},
    { CURSOR_PATH "cursor-uparrow.png", {"Up Arrow Cursor", Qt::UpArrowCursor}},
    { CURSOR_PATH "cursor-whatsthis.png", {"What't This Cursor", Qt::WhatsThisCursor}},
  };
}

PreferencesDialog::PreferencesDialog(Settings* settings, Spotlight* spotlight, QWidget* parent)
  : QDialog(parent)
{
  setWindowTitle(QCoreApplication::applicationName() + " - " + tr("Preferences"));
  setWindowIcon(QIcon(":/icons/projecteur-tray.svg"));

  auto vspacer = new QVBoxLayout;
  vspacer->addSpacing(10);

  auto grid = new QGridLayout();
  grid->addWidget(createSpotGroupBox(settings), 0, 0, 1, 2);
  grid->addLayout(vspacer, 1, 0, 1, 2);
  grid->addWidget(createDotGroupBox(settings), 2, 0, 1, 2);

  m_screenCb = new QComboBox(this);
  m_screenCb->addItem(tr("%1: (not connected)").arg(settings->screen()), settings->screen());
  connect(m_screenCb, static_cast<void (QComboBox::*)(int)>(&QComboBox::currentIndexChanged),
  [settings, this](int index) {
    settings->setScreen(m_screenCb->itemData(index).toInt());
  });
  connect(settings, &Settings::screenChanged, [this](int screen){
    const int idx = m_screenCb->findData(screen);
    if (idx == -1) {
      m_screenCb->addItem(tr("%1: (not connected)").arg(screen), screen);
    } else {
      m_screenCb->setCurrentIndex(idx);
    }
  });
  grid->addWidget(new QLabel(tr("Screen"), this), 3, 0);
  grid->addWidget(m_screenCb, 3, 1);

  auto cursorCb = new QComboBox(this);
  for (const auto& item : cursorMap) {
    cursorCb->addItem(QIcon(item.first), item.second.first, static_cast<int>(item.second.second));
  }
  connect(settings, &Settings::cursorChanged, [cursorCb](int cursor){
    const int idx = cursorCb->findData(cursor);
    cursorCb->setCurrentIndex((idx == -1) ? Qt::BlankCursor : idx);
  });
  emit settings->cursorChanged(settings->cursor()); // set initial value
  connect(cursorCb, static_cast<void (QComboBox::*)(int)>(&QComboBox::currentIndexChanged),
  [settings, cursorCb](int index) {
    settings->setCursor(static_cast<Qt::CursorShape>(cursorCb->itemData(index).toInt()));
  });
  grid->addWidget(new QLabel(tr("Cursor"), this), 4, 0);
  grid->addWidget(cursorCb, 4, 1);

  auto closeBtn = new QPushButton(tr("&Close"), this);
  closeBtn->setToolTip(tr("Close the preferences dialog."));
  connect(closeBtn, &QPushButton::clicked, [this](){ this->close(); });
  auto defaultsBtn = new QPushButton(tr("&Reset Defaults"), this);
  defaultsBtn->setToolTip(tr("Reset all settings to their default value."));
  connect(defaultsBtn, &QPushButton::clicked, settings, &Settings::setDefaults);

  auto btnHBox = new QHBoxLayout;
  btnHBox->addWidget(defaultsBtn);
  btnHBox->addStretch(1);
  btnHBox->addWidget(closeBtn);

  auto testBtn = new QPushButton(tr("&Show test..."), this);
  connect(testBtn, &QPushButton::clicked, this, &PreferencesDialog::testButtonClicked);

  auto vbox = new QVBoxLayout(this);
  vbox->addLayout(grid);
  vbox->addStretch(1);
  vbox->addWidget(createConnectedStateWidget(spotlight));
  vbox->addWidget(testBtn);
  vbox->addSpacing(10);
  vbox->addLayout(btnHBox);
}

QWidget* PreferencesDialog::createConnectedStateWidget(Spotlight* spotlight)
{
  static const auto deviceText = tr("Device connected: %1", "%1=True or False");
  auto group = new QGroupBox(this);
  auto vbox = new QVBoxLayout(group);
  auto lbl = new QLabel(deviceText.arg(spotlight->anySpotlightDeviceConnected() ? "True"
                                                                                : "False"), this);
  lbl->setToolTip(tr("Connection status of the spotlight device."));

  vbox->addWidget(lbl);
  connect(spotlight, &Spotlight::anySpotlightDeviceConnectedChanged, [lbl](bool connected) {
    lbl->setText(deviceText.arg(connected ? "True" : "False"));
  });
  return group;
}

QGroupBox* PreferencesDialog::createSpotGroupBox(Settings* settings)
{
  auto spotGroup = new QGroupBox(tr("Show Spotlight"), this);
  spotGroup->setCheckable(true);
  spotGroup->setChecked(settings->showSpot());
  connect(spotGroup, &QGroupBox::toggled, settings, &Settings::setShowSpot);
  connect(settings, &Settings::showSpotChanged, spotGroup, &QGroupBox::setChecked);

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

  auto spotGrid = new QGridLayout(spotGroup);
  spotGrid->addWidget(new QLabel(tr("Spot Size"), this), 0, 0);
  spotGrid->addLayout(spotsizeHBox, 0, 1);

  // Shade color setting
  auto shadeColor = new ColorSelector(settings->shadeColor(), this);
  connect(shadeColor, &ColorSelector::colorChanged, settings, &Settings::setShadeColor);
  connect(settings, &Settings::shadeColorChanged, shadeColor, &ColorSelector::setColor);
  spotGrid->addWidget(new QLabel(tr("Shade Color"), this), 1, 0);
  spotGrid->addWidget(shadeColor, 1, 1);

  // Spotlight shade opacity setting
  auto shadeOpacitySb = new QDoubleSpinBox(this);
  shadeOpacitySb->setMaximum(1.0);
  shadeOpacitySb->setMinimum(0.0);
  shadeOpacitySb->setDecimals(2);
  shadeOpacitySb->setSingleStep(0.1);
  shadeOpacitySb->setValue(settings->shadeOpacity());
  connect(shadeOpacitySb, static_cast<void (QDoubleSpinBox::*)(double)>(&QDoubleSpinBox::valueChanged),
          settings, &Settings::setShadeOpacity);
  connect(settings, &Settings::shadeOpacityChanged, shadeOpacitySb, &QDoubleSpinBox::setValue);
  spotGrid->addWidget(new QLabel(tr("Shade Opacity"), this), 2, 0);
  spotGrid->addWidget(shadeOpacitySb, 2, 1);

  // Spotlight shape setting
  auto shapeCombo = new QComboBox(this);
  for (const auto& shape : settings->spotShapes()) {
    shapeCombo->addItem(shape.displayName(), shape.qmlComponent());
  }
  connect(settings, &Settings::spotShapeChanged, [shapeCombo](const QString& spotShape){
    const int idx = shapeCombo->findData(spotShape);
    if (idx != -1) {
      shapeCombo->setCurrentIndex(idx);
    }
  });
  emit settings->spotShapeChanged(settings->spotShape());
  connect(shapeCombo, static_cast<void (QComboBox::*)(int)>(&QComboBox::currentIndexChanged),
  [settings, shapeCombo](int index) {
    settings->setSpotShape(shapeCombo->itemData(index).toString());
  });
  spotGrid->addWidget(new QLabel(tr("Shape"), this), 3, 0);
  spotGrid->addWidget(shapeCombo, 3, 1);

  // Spotlight rotation setting
  auto shapeRotationSb = new QDoubleSpinBox(this);
  shapeRotationSb->setMaximum(360.0);
  shapeRotationSb->setMinimum(0.0);
  shapeRotationSb->setDecimals(1);
  shapeRotationSb->setSingleStep(1.0);
  shapeRotationSb->setValue(settings->spotRotation());
  connect(shapeRotationSb, static_cast<void (QDoubleSpinBox::*)(double)>(&QDoubleSpinBox::valueChanged),
          settings, &Settings::setSpotRotation);
  connect(settings, &Settings::spotRotationChanged, shapeRotationSb, &QDoubleSpinBox::setValue);
  spotGrid->addWidget(new QLabel(tr("Rotation"), this), 4, 0);
  spotGrid->addWidget(shapeRotationSb, 4, 1);

  return spotGroup;
}

QGroupBox* PreferencesDialog::createDotGroupBox(Settings* settings)
{
  auto dotGroup = new QGroupBox(tr("Show Center Dot"), this);
  dotGroup->setCheckable(true);
  dotGroup->setChecked(settings->showCenterDot());
  connect(dotGroup, &QGroupBox::toggled, settings, &Settings::setShowCenterDot);
  connect(settings, &Settings::showCenterDotChanged, dotGroup, &QGroupBox::setChecked);

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

  return dotGroup;
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

void PreferencesDialog::updateAvailableScreens(QList<QScreen*> screens)
{
  for (int i = 0; i < screens.size(); ++i)
  {
    const int idx = m_screenCb->findData(i);
    if (idx == -1) {
      m_screenCb->addItem(QString("%1: %2 (%3x%4)").arg(i)
                                                   .arg(screens[i]->name())
                                                   .arg(screens[i]->size().width())
                                                   .arg(screens[i]->size().height()), i);
    }
    else {
      m_screenCb->setItemText(idx, QString("%1: %2 (%3x%4)").arg(i)
                                                   .arg(screens[i]->name())
                                                   .arg(screens[i]->size().width())
                                                   .arg(screens[i]->size().height()));
    }
  }
  m_screenCb->model()->sort(0);
}
