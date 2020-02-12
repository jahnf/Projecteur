// This file is part of Projecteur - https://github.com/jahnf/projecteur - See LICENSE.md and README.md
#include "preferencesdlg.h"

#include "colorselector.h"
#include "logging.h"
#include "settings.h"
#include "spotlight.h"

#include <QComboBox>
#include <QCoreApplication>
#include <QDateTime>
#include <QDoubleSpinBox>
#include <QFileDialog>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QIcon>
#include <QLabel>
#include <QGridLayout>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QQmlPropertyMap>
#include <QScreen>
#include <QSpinBox>
#include <QStyle>
#include <QTabWidget>
#include <QTimer>
#include <QVBoxLayout>

#if HAS_Qt5_X11Extras
#include <QX11Info>
#endif

#include <map>

LOGGING_CATEGORY(preferences, "preferences")

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

  const auto tabWidget = new QTabWidget(this);
  tabWidget->addTab(createSettingsTabWidget(settings, spotlight), tr("Settings"));
  tabWidget->addTab(createLogTabWidget(), tr("Log"));

  const auto closeBtn = new QPushButton(tr("&Close"), this);
  closeBtn->setToolTip(tr("Close the preferences dialog."));
  connect(closeBtn, &QPushButton::clicked, this, [this](){ this->close(); });
  const auto defaultsBtn = new QPushButton(tr("&Reset Defaults"), this);
  defaultsBtn->setToolTip(tr("Reset all settings to their default value."));
  connect(defaultsBtn, &QPushButton::clicked, settings, &Settings::setDefaults);

  const auto btnHBox = new QHBoxLayout;
  btnHBox->addWidget(defaultsBtn);
  btnHBox->addStretch(1);
  btnHBox->addWidget(closeBtn);

  const auto mainVBox = new QVBoxLayout(this);
  mainVBox->addWidget(tabWidget);
  mainVBox->addLayout(btnHBox);
}

QWidget* PreferencesDialog::createSettingsTabWidget(Settings* settings, Spotlight* spotlight)
{
  const auto widget = new QWidget(this);
  const auto mainHBox = new QHBoxLayout;
  const auto spotScreenVBoxLeft = new QVBoxLayout();
  spotScreenVBoxLeft->addWidget(createShapeGroupBox(settings));
  spotScreenVBoxLeft->addWidget(createZoomGroupBox(settings));
  spotScreenVBoxLeft->addWidget(createCursorGroupBox(settings));
  const auto spotScreenVBoxRight = new QVBoxLayout();
  spotScreenVBoxRight->addWidget(createSpotGroupBox(settings));
  spotScreenVBoxRight->addWidget(createDotGroupBox(settings));
  spotScreenVBoxRight->addWidget(createBorderGroupBox(settings));
  mainHBox->addLayout(spotScreenVBoxLeft);
  mainHBox->addLayout(spotScreenVBoxRight);

  const auto testBtn = new QPushButton(tr("&Show test..."), this);
  connect(testBtn, &QPushButton::clicked, this, &PreferencesDialog::testButtonClicked);

  const auto mainVBox = new QVBoxLayout(widget);
  mainVBox->addLayout(mainHBox);
#if HAS_Qt5_X11Extras
  mainVBox->addWidget(createCompositorWarningWidget());
#endif
  mainVBox->addWidget(createConnectedStateWidget(spotlight));
  mainVBox->addWidget(testBtn);

  return widget;
}

QWidget* PreferencesDialog::createConnectedStateWidget(Spotlight* spotlight)
{
  static const auto deviceText = tr("Device connected: %1", "%1=True or False");
  const auto group = new QGroupBox(this);
  const auto hbox = new QHBoxLayout(group);
  const auto lbl = new QLabel(deviceText.arg(
                                spotlight->anySpotlightDeviceConnected() ? tr("True")
                                                                         : tr("False")), this);
  lbl->setToolTip(tr("Connection status of the spotlight device."));

  auto icon = style()->standardIcon(QStyle::SP_MessageBoxWarning);
  const auto iconLbl = new QLabel(this);
  iconLbl->setPixmap(icon.pixmap(16,16));

  hbox->addWidget(iconLbl);
  hbox->addWidget(lbl);
  hbox->setStretch(1,2);

  auto updateStatus = [this, lbl, iconLbl](bool connected) {
    lbl->setText(deviceText.arg(connected ? tr("True") : tr("False")));
    iconLbl->setPixmap(style()->standardIcon(connected ? QStyle::SP_DialogOkButton
                                                       : QStyle::SP_MessageBoxWarning).pixmap(16,16));
  };
  updateStatus(spotlight->anySpotlightDeviceConnected());
  connect(spotlight, &Spotlight::anySpotlightDeviceConnectedChanged, std::move(updateStatus));
  return group;
}

#if HAS_Qt5_X11Extras
QWidget* PreferencesDialog::createCompositorWarningWidget()
{
  if (!QX11Info::isPlatformX11())
  { // Platform ist not X11, possibly wayland or others...
    const auto widget = new QWidget(this);
    widget->setVisible(false);
    return widget;
  }

  const auto widget = new QFrame(this);
  widget->setFrameStyle(QFrame::StyledPanel | QFrame::Plain);
  const auto hbox = new QHBoxLayout(widget);

  const auto iconLabel = new QLabel(this);
  iconLabel->setPixmap(style()->standardPixmap(QStyle::SP_MessageBoxCritical));
  hbox->addWidget(iconLabel);
  const auto textLabel = new QLabel(tr("<b>Warning: No running compositing manager detected!</b>"), this);
  textLabel->setTextFormat(Qt::RichText);
  textLabel->setToolTip(tr("Please make sure a compositing manager is running. "
                           "On some systems one way is to run <tt>xcompmgr</tt> manually."));
  hbox->addWidget(textLabel);
  hbox->setStretch(1, 1);

  const auto timer = new QTimer(this);
  timer->setInterval(1000);
  timer->setSingleShot(false);

  auto checkForCompositorAndUpdate = [widget](){
    // Warning visible if no compositor is running.
    widget->setVisible(!QX11Info::isCompositingManagerRunning());
  };

  checkForCompositorAndUpdate();

  connect(this, &PreferencesDialog::dialogActiveChanged, this, [timer, checkForCompositorAndUpdate](bool active) {
    if (active) { checkForCompositorAndUpdate(); timer->start(); } else { timer->stop(); }
  });

  connect(timer, &QTimer::timeout, this, [checkForCompositorAndUpdate=std::move(checkForCompositorAndUpdate)]() {
    checkForCompositorAndUpdate();
  });

  return widget;
}
#endif

QGroupBox* PreferencesDialog::createShapeGroupBox(Settings* settings)
{
  const auto shapeGroup = new QGroupBox(tr("Shape Settings"), this);

  const auto spotSizeSpinBox = new QSpinBox(this);
  spotSizeSpinBox->setMaximum(settings->spotSizeRange().max);
  spotSizeSpinBox->setMinimum(settings->spotSizeRange().min);
  spotSizeSpinBox->setValue(settings->spotSize());
  const auto spotsizeHBox = new QHBoxLayout;
  spotsizeHBox->addWidget(spotSizeSpinBox);
  spotsizeHBox->addWidget(new QLabel(QString("% ")+tr("of screen height")));
  connect(spotSizeSpinBox, static_cast<void (QSpinBox::*)(int)>(&QSpinBox::valueChanged),
          settings, &Settings::setSpotSize);
  connect(settings, &Settings::spotSizeChanged, spotSizeSpinBox, &QSpinBox::setValue);

  const auto spotGrid = new QGridLayout(shapeGroup);
  spotGrid->addWidget(new QLabel(tr("Spot Size"), this), 0, 0);
  spotGrid->addLayout(spotsizeHBox, 0, 1);

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
  shapeRotationSb->setMaximum(settings->spotRotationRange().max);
  shapeRotationSb->setMinimum(settings->spotRotationRange().min);
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
  return shapeGroup;
}

QGroupBox* PreferencesDialog::createSpotGroupBox(Settings* settings)
{
  const auto spotGroup = new QGroupBox(tr("Show Spotlight Shade"), this);
  spotGroup->setCheckable(true);
  spotGroup->setChecked(settings->showSpotShade());
  connect(spotGroup, &QGroupBox::toggled, settings, &Settings::setShowSpotShade);
  connect(settings, &Settings::showSpotShadeChanged, spotGroup, &QGroupBox::setChecked);

  const auto spotGrid = new QGridLayout(spotGroup);

  // Shade color setting
  const auto shadeColor = new ColorSelector(settings->shadeColor(), this);
  connect(shadeColor, &ColorSelector::colorChanged, settings, &Settings::setShadeColor);
  connect(settings, &Settings::shadeColorChanged, shadeColor, &ColorSelector::setColor);
  spotGrid->addWidget(new QLabel(tr("Shade Color"), this), 1, 0);
  spotGrid->addWidget(shadeColor, 1, 1);

  // Spotlight shade opacity setting
  const auto shadeOpacitySb = new QDoubleSpinBox(this);
  shadeOpacitySb->setMaximum(settings->shadeOpacityRange().max);
  shadeOpacitySb->setMinimum(settings->shadeOpacityRange().min);
  shadeOpacitySb->setDecimals(2);
  shadeOpacitySb->setSingleStep(0.1);
  shadeOpacitySb->setValue(settings->shadeOpacity());
  connect(shadeOpacitySb, static_cast<void (QDoubleSpinBox::*)(double)>(&QDoubleSpinBox::valueChanged),
          settings, &Settings::setShadeOpacity);
  connect(settings, &Settings::shadeOpacityChanged, shadeOpacitySb, &QDoubleSpinBox::setValue);
  spotGrid->addWidget(new QLabel(tr("Shade Opacity"), this), 2, 0);
  spotGrid->addWidget(shadeOpacitySb, 2, 1);

  spotGrid->addWidget(new QWidget(this), 100, 0);
  spotGrid->setRowStretch(100, 100);

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
  dotSizeSpinBox->setMaximum(settings->dotSizeRange().max);
  dotSizeSpinBox->setMinimum(settings->dotSizeRange().min);
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
  borderSizeSpinBox->setMaximum(settings->borderSizeRange().max);
  borderSizeSpinBox->setMinimum(settings->borderSizeRange().min);
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

  // Spotlight border opacity setting
  const auto borderOpacitySb = new QDoubleSpinBox(this);
  borderOpacitySb->setMaximum(settings->borderOpacityRange().max);
  borderOpacitySb->setMinimum(settings->borderOpacityRange().min);
  borderOpacitySb->setDecimals(2);
  borderOpacitySb->setSingleStep(0.1);
  borderOpacitySb->setValue(settings->borderOpacity());
  connect(borderOpacitySb, static_cast<void (QDoubleSpinBox::*)(double)>(&QDoubleSpinBox::valueChanged),
          settings, &Settings::setBorderOpacity);
  connect(settings, &Settings::borderOpacityChanged, borderOpacitySb, &QDoubleSpinBox::setValue);
  borderGrid->addWidget(new QLabel(tr("Border Opacity"), this), 2, 0);
  borderGrid->addWidget(borderOpacitySb, 2, 1);

  borderGrid->addWidget(new QWidget(this), 100, 0);
  borderGrid->setRowStretch(100, 100);

  borderGrid->setColumnStretch(1, 1);
  return borderGroup;
}

QGroupBox* PreferencesDialog::createZoomGroupBox(Settings* settings)
{
  const auto zoomGroup = new QGroupBox(tr("Enable Zoom"), this);
  zoomGroup->setCheckable(true);
  zoomGroup->setChecked(settings->zoomEnabled());
  connect(zoomGroup, &QGroupBox::toggled, settings, &Settings::setZoomEnabled);
  connect(settings, &Settings::zoomEnabledChanged, zoomGroup, &QGroupBox::setChecked);

  const auto zoomGrid = new QGridLayout(zoomGroup);

  // zoom level setting
  const auto zoomLevelSb = new QDoubleSpinBox(this);
  zoomLevelSb->setMaximum(settings->zoomFactorRange().max);
  zoomLevelSb->setMinimum(settings->zoomFactorRange().min);
  zoomLevelSb->setDecimals(2);
  zoomLevelSb->setSingleStep(0.1);
  zoomLevelSb->setValue(settings->zoomFactor());
  connect(zoomLevelSb, static_cast<void (QDoubleSpinBox::*)(double)>(&QDoubleSpinBox::valueChanged),
          settings, &Settings::setZoomFactor);
  connect(settings, &Settings::zoomFactorChanged, zoomLevelSb, &QDoubleSpinBox::setValue);
  zoomGrid->addWidget(new QLabel(tr("Zoom Level"), this), 0, 0);
  zoomGrid->addWidget(zoomLevelSb, 0, 1);
  zoomGrid->setColumnStretch(1, 1);
  return zoomGroup;
}

QGroupBox* PreferencesDialog::createCursorGroupBox(Settings* settings)
{
  const auto cursorGroup = new QGroupBox(tr("Cursor Settings"), this);
  cursorGroup->setCheckable(false);
  const auto grid = new QGridLayout(cursorGroup);

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

  grid->addWidget(new QLabel(tr("Cursor"), this), 0, 0);
  grid->addWidget(cursorCb, 0, 1);
  grid->setColumnStretch(1, 1);
  return cursorGroup;
}

QWidget* PreferencesDialog::createLogTabWidget()
{
  const auto widget = new QWidget(this);
  const auto mainVBox = new QVBoxLayout(widget);

  const auto te = new QPlainTextEdit(widget);
  te->setReadOnly(true);
  te->setWordWrapMode(QTextOption::NoWrap);
  te->setMaximumBlockCount(1000);
  te->setFont([te]()
  {
    auto font = te->font();
    font.setPointSize(font.pointSize() - 1);
    return font;
  }());
  logging::registerTextEdit(te);

  const auto lvlHBox = new QHBoxLayout();
  lvlHBox->addWidget(new QLabel(tr("Log Level"), widget));
  // Log level combo box
  const auto logLvlCombo = new QComboBox(widget);
  logLvlCombo->addItem(tr("Debug"), static_cast<int>(logging::level::debug));
  logLvlCombo->addItem(tr("Info"), static_cast<int>(logging::level::info));
  logLvlCombo->addItem(tr("Warning"), static_cast<int>(logging::level::warning));
  logLvlCombo->addItem(tr("Error"), static_cast<int>(logging::level::error));
  lvlHBox->addWidget(logLvlCombo);

  const int idx = logLvlCombo->findData(static_cast<int>(logging::currentLevel()));
  logLvlCombo->setCurrentIndex((idx == -1) ? 0 : idx);

  connect(logLvlCombo, static_cast<void (QComboBox::*)(int)>(&QComboBox::currentIndexChanged),
  [logLvlCombo, te](int index) {
    const auto lvl = static_cast<logging::level>(logLvlCombo->itemData(index).toInt());
    te->appendPlainText(tr("--- Setting new log level: %1").arg(logging::levelToString(lvl)));
    logging::setCurrentLevel(lvl);
  });

  const auto saveLogBtn = new QPushButton(tr("&Save log..."), this);
  saveLogBtn->setToolTip(tr("Save log to file."));
  connect(saveLogBtn, &QPushButton::clicked, this, [this, te]()
  {
    static auto saveDir = QDir::homePath();
    const auto defaultName = QString("projecteur_%1.log")
                             .arg(QDateTime::currentDateTime().toString("yyyy-MM-dd_hh-mm"));

    const auto defaultFile = QDir(saveDir).filePath(defaultName);
    QString logFilter(tr("Log files (*.log *.txt)"));
    const auto logFile = QFileDialog::getSaveFileName(this, tr("Save log file"),
                                                      defaultFile, logFilter, &logFilter);
    if (logFile.isEmpty())  return;
    saveDir = QFileInfo(logFile).path();

    QFile f(logFile);
    if (f.open(QIODevice::WriteOnly))
    {
      // TODO add application info (version, build type, qt version, environemnt) to output file
      f.write(te->toPlainText().toLocal8Bit());
      logInfo(preferences) << tr("Log saved to: ") << logFile;
    }
    else {
      logError(preferences) << tr("Could not open '%1' for writing.").arg(logFile);
    }
  });

  lvlHBox->addWidget(saveLogBtn);
  lvlHBox->setStretch(0, 0);
  lvlHBox->setStretch(1, 1);
  lvlHBox->setStretch(2, 1);

  mainVBox->addLayout(lvlHBox);
  mainVBox->addWidget(te);
  return widget;
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
