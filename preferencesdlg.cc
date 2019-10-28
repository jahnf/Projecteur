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
#include <QQmlPropertyMap>
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

  const auto mainHBox = new QHBoxLayout();
  mainHBox->addWidget(createSpotGroupBox(settings));
  const auto spotScreenVBox = new QVBoxLayout();
  spotScreenVBox->addWidget(createDotGroupBox(settings));
  spotScreenVBox->addWidget(createBorderGroupBox(settings));
  spotScreenVBox->addWidget(createScreenGroupBox(settings));
  mainHBox->addLayout(spotScreenVBox);

  const auto closeBtn = new QPushButton(tr("&Close"), this);
  closeBtn->setToolTip(tr("Close the preferences dialog."));
  connect(closeBtn, &QPushButton::clicked, [this](){ this->close(); });
  const auto defaultsBtn = new QPushButton(tr("&Reset Defaults"), this);
  defaultsBtn->setToolTip(tr("Reset all settings to their default value."));
  connect(defaultsBtn, &QPushButton::clicked, settings, &Settings::setDefaults);

  const auto testBtn = new QPushButton(tr("&Show test..."), this);
  connect(testBtn, &QPushButton::clicked, this, &PreferencesDialog::testButtonClicked);

  const auto btnHBox = new QHBoxLayout;
  btnHBox->addWidget(defaultsBtn);
  btnHBox->addStretch(1);
  btnHBox->addWidget(closeBtn);

  const auto mainVBox = new QVBoxLayout(this);
  mainVBox->addLayout(mainHBox);
  mainVBox->addStretch(1);
  mainVBox->addWidget(createConnectedStateWidget(spotlight));
  mainVBox->addWidget(testBtn);
  mainVBox->addSpacing(10);
  mainVBox->addLayout(btnHBox);
}

QWidget* PreferencesDialog::createConnectedStateWidget(Spotlight* spotlight)
{
  static const auto deviceText = tr("Device connected: %1", "%1=True or False");
  const auto group = new QGroupBox(this);
  const auto vbox = new QVBoxLayout(group);
  const auto lbl = new QLabel(deviceText.arg(
                                spotlight->anySpotlightDeviceConnected() ? "True"
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
  const auto spotGroup = new QGroupBox(tr("Show Spotlight"), this);
  spotGroup->setCheckable(true);
  spotGroup->setChecked(settings->showSpot());
  connect(spotGroup, &QGroupBox::toggled, settings, &Settings::setShowSpot);
  connect(settings, &Settings::showSpotChanged, spotGroup, &QGroupBox::setChecked);

  const auto spotSizeSpinBox = new QSpinBox(this);
  spotSizeSpinBox->setMaximum(100);
  spotSizeSpinBox->setMinimum(5);
  spotSizeSpinBox->setValue(settings->spotSize());
  const auto spotsizeHBox = new QHBoxLayout;
  spotsizeHBox->addWidget(spotSizeSpinBox);
  spotsizeHBox->addWidget(new QLabel(QString("% ")+tr("of screen height")));
  connect(spotSizeSpinBox, static_cast<void (QSpinBox::*)(int)>(&QSpinBox::valueChanged),
          settings, &Settings::setSpotSize);
  connect(settings, &Settings::spotSizeChanged, spotSizeSpinBox, &QSpinBox::setValue);

  const auto spotGrid = new QGridLayout(spotGroup);
  spotGrid->addWidget(new QLabel(tr("Spot Size"), this), 0, 0);
  spotGrid->addLayout(spotsizeHBox, 0, 1);

  // Shade color setting
  const auto shadeColor = new ColorSelector(settings->shadeColor(), this);
  connect(shadeColor, &ColorSelector::colorChanged, settings, &Settings::setShadeColor);
  connect(settings, &Settings::shadeColorChanged, shadeColor, &ColorSelector::setColor);
  spotGrid->addWidget(new QLabel(tr("Shade Color"), this), 1, 0);
  spotGrid->addWidget(shadeColor, 1, 1);

  // Spotlight shade opacity setting
  const auto shadeOpacitySb = new QDoubleSpinBox(this);
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
  const auto shapeCombo = new QComboBox(this);
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
  spotGrid->addWidget(new QLabel(tr("Shape"), this), 4, 0);
  spotGrid->addWidget(shapeCombo, 4, 1);

  // Spotlight rotation setting
  const auto shapeRotationSb = new QDoubleSpinBox(this);
  shapeRotationSb->setMaximum(360.0);
  shapeRotationSb->setMinimum(0.0);
  shapeRotationSb->setDecimals(1);
  shapeRotationSb->setSingleStep(1.0);
  shapeRotationSb->setValue(settings->spotRotation());
  connect(shapeRotationSb, static_cast<void (QDoubleSpinBox::*)(double)>(&QDoubleSpinBox::valueChanged),
          settings, &Settings::setSpotRotation);
  connect(settings, &Settings::spotRotationChanged, shapeRotationSb, &QDoubleSpinBox::setValue);
  const auto shapeRotationLabel = new QLabel(tr("Rotation"), this);
  spotGrid->addWidget(shapeRotationLabel, 5, 0);
  spotGrid->addWidget(shapeRotationSb, 5, 1);

  auto updateShapeSettingsWidgets = [settings, shapeCombo, shapeRotationSb, shapeRotationLabel, spotGrid, this]()
  {
    if (shapeCombo->currentIndex() == -1) return;
    const QString shapeQml = shapeCombo->itemData(shapeCombo->currentIndex()).toString();
    const auto& shapes = settings->spotShapes();
    auto it = std::find_if(shapes.cbegin(), shapes.cend(), [&shapeQml](const Settings::SpotShape& s) {
      return shapeQml == s.qmlComponent();
    });

    constexpr int startRow = 100;
    constexpr int maxRows = 10;

    for (int row = startRow; row < startRow + maxRows; ++row) {
      if (const auto li = spotGrid->itemAtPosition(row, 0)) {
        if (const auto w = li->widget()) {
          w->hide();
          w->deleteLater();
        }
      }
      if (const auto li = spotGrid->itemAtPosition(row, 1)) {
        if (const auto w = li->widget()) {
          w->hide();
          w->deleteLater();
        }
      }
    }

    if (it != shapes.cend())
    {
      shapeRotationLabel->setVisible(it->allowRotation());
      shapeRotationSb->setVisible(it->allowRotation());
      const auto& shape = *it;
      int row = startRow;
      for (const auto& s : it->shapeSettings())
      {
        if (row >= startRow + maxRows) break;
        spotGrid->addWidget(new QLabel(s.displayName(), this),row, 0);
        if (s.defaultValue().type() == QVariant::Int)
        {
          const auto spinbox = new QSpinBox(this);
          spinbox->setMaximum(s.maxValue().toInt());
          spinbox->setMinimum(s.minValue().toInt());
          spinbox->setValue(s.defaultValue().toInt());
          spotGrid->addWidget(spinbox, row, 1);

          const auto pm = settings->shapeSettings(shape.name());
          if (pm && pm->property(s.settingsKey().toLocal8Bit()).isValid())
          {
            spinbox->setValue(pm->property(s.settingsKey().toLocal8Bit()).toInt());
            connect(spinbox, static_cast<void (QSpinBox::*)(int)>(&QSpinBox::valueChanged),
            [s, pm](int newValue){
              pm->setProperty(s.settingsKey().toLocal8Bit(), newValue);
            });
            connect(pm, &QQmlPropertyMap::valueChanged, spinbox, [s, spinbox](const QString& key, const QVariant& value)
            {
              if (key != s.settingsKey() || !value.isValid()) return;
              spinbox->setValue(value.toInt());
            });
          }
        }
        ++row;
      }
    }
  };

  connect(shapeCombo, static_cast<void (QComboBox::*)(int)>(&QComboBox::currentIndexChanged),
  [settings, shapeCombo, updateShapeSettingsWidgets](int index)
  {
    const QString shapeQml = shapeCombo->itemData(index).toString();
    settings->setSpotShape(shapeQml);
    updateShapeSettingsWidgets();
  });

  updateShapeSettingsWidgets();

  spotGrid->addWidget(new QWidget(this), 200, 0);
  spotGrid->setRowStretch(200, 200);

  spotGrid->setColumnStretch(1, 1);
  return spotGroup;
}

QGroupBox* PreferencesDialog::createDotGroupBox(Settings* settings)
{
  const auto dotGroup = new QGroupBox(tr("Show Center Dot"), this);
  dotGroup->setCheckable(true);
  dotGroup->setChecked(settings->showCenterDot());
  connect(dotGroup, &QGroupBox::toggled, settings, &Settings::setShowCenterDot);
  connect(settings, &Settings::showCenterDotChanged, dotGroup, &QGroupBox::setChecked);

  const auto dotSizeSpinBox = new QSpinBox(this);
  dotSizeSpinBox->setMaximum(100);
  dotSizeSpinBox->setMinimum(3);
  dotSizeSpinBox->setValue(settings->dotSize());
  auto dotsizeHBox = new QHBoxLayout;
  dotsizeHBox->addWidget(dotSizeSpinBox);
  dotsizeHBox->addWidget(new QLabel(tr("pixel")));
  connect(dotSizeSpinBox, static_cast<void (QSpinBox::*)(int)>(&QSpinBox::valueChanged),
          settings, &Settings::setDotSize);
  connect(settings, &Settings::dotSizeChanged, dotSizeSpinBox, &QSpinBox::setValue);

  const auto dotGrid = new QGridLayout(dotGroup);
  dotGrid->addWidget(new QLabel(tr("Dot Size"), this), 0, 0);
  dotGrid->addLayout(dotsizeHBox, 0, 1);

  const auto dotColor = new ColorSelector(settings->dotColor(), this);
  connect(dotColor, &ColorSelector::colorChanged, settings, &Settings::setDotColor);
  connect(settings, &Settings::dotColorChanged, dotColor, &ColorSelector::setColor);
  dotGrid->addWidget(new QLabel(tr("Dot Color"), this), 1, 0);
  dotGrid->addWidget(dotColor, 1, 1);

  dotGrid->addWidget(new QWidget(this), 100, 0);
  dotGrid->setRowStretch(100, 100);

  dotGrid->setColumnStretch(1, 1);
  return dotGroup;
}

QGroupBox* PreferencesDialog::createBorderGroupBox(Settings* settings)
{
  const auto borderGroup = new QGroupBox(tr("Show Border"), this);
  borderGroup->setCheckable(true);
  borderGroup->setChecked(settings->showBorder());
  connect(borderGroup, &QGroupBox::toggled, settings, &Settings::setShowBorder);
  connect(settings, &Settings::showBorderChanged, borderGroup, &QGroupBox::setChecked);

  const auto borderSizeSpinBox = new QSpinBox(this);
  borderSizeSpinBox->setMaximum(50);
  borderSizeSpinBox->setMinimum(3);
  borderSizeSpinBox->setValue(settings->borderSize());
  auto bordersizeHBox = new QHBoxLayout;
  bordersizeHBox->addWidget(borderSizeSpinBox);
  bordersizeHBox->addWidget(new QLabel(tr("% of spotsize")));
  connect(borderSizeSpinBox, static_cast<void (QSpinBox::*)(int)>(&QSpinBox::valueChanged),
          settings, &Settings::setBorderSize);
  connect(settings, &Settings::borderSizeChanged, borderSizeSpinBox, &QSpinBox::setValue);

  const auto borderGrid = new QGridLayout(borderGroup);
  borderGrid->addWidget(new QLabel(tr("Border Size"), this), 0, 0);
  borderGrid->addLayout(bordersizeHBox, 0, 1);

  const auto borderColor = new ColorSelector(settings->borderColor(), this);
  connect(borderColor, &ColorSelector::colorChanged, settings, &Settings::setBorderColor);
  connect(settings, &Settings::borderColorChanged, borderColor, &ColorSelector::setColor);
  borderGrid->addWidget(new QLabel(tr("Border Color"), this), 1, 0);
  borderGrid->addWidget(borderColor, 1, 1);

  borderGrid->addWidget(new QWidget(this), 100, 0);
  borderGrid->setRowStretch(100, 100);

  borderGrid->setColumnStretch(1, 1);
  return borderGroup;
}

QGroupBox* PreferencesDialog::createScreenGroupBox(Settings* settings)
{
  const auto screenGroup = new QGroupBox(tr("Screen Settings"), this);
  screenGroup->setCheckable(false);
  const auto grid = new QGridLayout(screenGroup);

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
  grid->addWidget(new QLabel(tr("Screen"), this), 1, 0);
  grid->addWidget(m_screenCb, 1, 1);

  const auto cursorCb = new QComboBox(this);
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
  grid->addWidget(new QLabel(tr("Cursor"), this), 2, 0);
  grid->addWidget(cursorCb, 2, 1);
  grid->setColumnStretch(1, 1);

  return screenGroup;
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
