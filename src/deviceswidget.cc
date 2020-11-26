// This file is part of Projecteur - https://github.com/jahnf/projecteur - See LICENSE.md and README.md
#include "deviceswidget.h"

#include "deviceinput.h"
#include "iconwidgets.h"
#include "inputmapconfig.h"
#include "logging.h"
#include "settings.h"
#include "spotlight.h"

#include <QComboBox>
#include <QLabel>
#include <QLayout>
#include <QShortcut>
#include <QSpinBox>
#include <QStackedLayout>
#include <QStyle>
#include <QTabWidget>

DECLARE_LOGGING_CATEGORY(preferences)

// -------------------------------------------------------------------------------------------------
namespace {
  QString descriptionString(const QString& name, const DeviceId& id) {
    return QString("%1 (%2:%3) [%4]").arg(name).arg(id.vendorId, 4, 16, QChar('0'))
                                     .arg(id.productId, 4, 16, QChar('0')).arg(id.phys);
  }

  const auto invalidDeviceId = DeviceId(); // vendorId = 0, productId = 0
}

// -------------------------------------------------------------------------------------------------
DevicesWidget::DevicesWidget(Settings* settings, Spotlight* spotlight, QWidget* parent)
  : QWidget(parent)
{
  createDeviceComboBox(spotlight);

  const auto stackLayout = new QStackedLayout(this);
  const auto disconnectedWidget = createDisconnectedStateWidget();
  stackLayout->addWidget(disconnectedWidget);
  const auto deviceWidget = createDevicesWidget(settings, spotlight);
  stackLayout->addWidget(deviceWidget);

  const bool anyDeviceConnected = spotlight->anySpotlightDeviceConnected();
  stackLayout->setCurrentWidget(anyDeviceConnected ? deviceWidget : disconnectedWidget);

  connect(spotlight, &Spotlight::anySpotlightDeviceConnectedChanged, this,
  [stackLayout, deviceWidget, disconnectedWidget](bool anyConnected){
    stackLayout->setCurrentWidget(anyConnected ? deviceWidget : disconnectedWidget);
  });
}

// -------------------------------------------------------------------------------------------------
const DeviceId DevicesWidget::currentDeviceId() const
{
  if (m_devicesCombo->currentIndex() < 0)
    return invalidDeviceId;

  return qvariant_cast<DeviceId>(m_devicesCombo->currentData());
}

// -------------------------------------------------------------------------------------------------
QWidget* DevicesWidget::createDevicesWidget(Settings* settings, Spotlight* spotlight)
{
  const auto dw = new QWidget(this);
  const auto vLayout = new QVBoxLayout(dw);
  const auto devHLayout = new QHBoxLayout();
  vLayout->addLayout(devHLayout);

  devHLayout->addWidget(new QLabel(tr("Device"), dw));
  devHLayout->addWidget(m_devicesCombo);
  devHLayout->setStretch(1, 1);

  vLayout->addSpacing(10);

  const auto tabWidget = new QTabWidget(dw);
  vLayout->addWidget(tabWidget);

  tabWidget->addTab(createInputMapperWidget(settings, spotlight), tr("Input Mapping"));

  return dw;
}

// -------------------------------------------------------------------------------------------------
QWidget* DevicesWidget::createDeviceInfoWidget(Spotlight* /*spotlight*/)
{
  const auto diWidget = new QWidget(this);
  const auto layout = new QHBoxLayout(diWidget);
  layout->addStretch(1);
  layout->addWidget(new QLabel(tr("Not yet implemented"), this));
  layout->addStretch(1);
  diWidget->setDisabled(true);
  return diWidget;
}

// -------------------------------------------------------------------------------------------------
QWidget* DevicesWidget::createInputMapperWidget(Settings* settings, Spotlight* /*spotlight*/)
{
  const auto delShortcut = new QShortcut( QKeySequence(Qt::ShiftModifier + Qt::Key_Delete), this);

  const auto imWidget = new QWidget(this);
  const auto layout = new QVBoxLayout(imWidget);
  const auto intervalLayout = new QHBoxLayout();

  const auto addBtn = new IconButton(Font::Icon::plus_5, imWidget);
  addBtn->setToolTip(tr("Add a new input mapping entry."));
  const auto delBtn = new IconButton(Font::Icon::trash_can_1, imWidget);
  delBtn->setToolTip(tr("Delete the selected input mapping entries (%1).", "%1=shortcut")
                       .arg(delShortcut->key().toString()));
  delBtn->setEnabled(false);

  const auto intervalLbl = new QLabel(tr("Input Sequence Interval"), imWidget);
  const auto intervalSb = new QSpinBox(this);
  const auto intervalUnitLbl = new QLabel(tr("ms"), imWidget);
  intervalSb->setMaximum(settings->inputSequenceIntervalRange().max);
  intervalSb->setMinimum(settings->inputSequenceIntervalRange().min);
  intervalSb->setValue(m_inputMapper ? m_inputMapper->keyEventInterval()
                                     : settings->deviceInputSeqInterval(currentDeviceId()));
  intervalSb->setSingleStep(50);

  intervalLayout->addWidget(addBtn);
  intervalLayout->addWidget(delBtn);
  intervalLayout->addStretch(1);
  intervalLayout->addWidget(intervalLbl);
  intervalLayout->addWidget(intervalSb);
  intervalLayout->addWidget(intervalUnitLbl);

  const auto tblView = new InputMapConfigView(imWidget);
  const auto imModel = new InputMapConfigModel(m_inputMapper, imWidget);
  if (m_inputMapper) imModel->setConfiguration(m_inputMapper->configuration());


  tblView->setModel(imModel);
  const auto selectionModel = tblView->selectionModel();

  auto updateImWidget = [this, imWidget]() {
    imWidget->setDisabled(!m_inputMapper || !m_inputMapper->hasVirtualDevice());
  };
  updateImWidget();

  connect(this, &DevicesWidget::currentDeviceChanged, this,
  [this, imModel, intervalSb, updateImWidget=std::move(updateImWidget)](){
    imModel->setInputMapper(m_inputMapper);
    if (m_inputMapper) {
      intervalSb->setValue(m_inputMapper->keyEventInterval());
      imModel->setConfiguration(m_inputMapper->configuration());
    }
    updateImWidget();
  });

  connect(intervalSb, static_cast<void (QSpinBox::*)(int)>(&QSpinBox::valueChanged),
  this, [this, settings](int valueMs) {
    if (m_inputMapper) {
      m_inputMapper->setKeyEventInterval(valueMs);
      settings->setDeviceInputSeqInterval(currentDeviceId(), valueMs);
    }
  });

  connect(selectionModel, &QItemSelectionModel::selectionChanged, this,
  [delBtn, selectionModel](){
    delBtn->setEnabled(selectionModel->hasSelection());
  });

  auto removeCurrentSelection = [imModel, selectionModel](){
    const auto selectedRows = selectionModel->selectedRows();
    std::vector<int> rows;
    rows.reserve(selectedRows.size());
    for (const auto& selectedRow : selectedRows) {
      rows.emplace_back(selectedRow.row());
    }
    imModel->removeConfigItemRows(std::move(rows));
  };

  connect(delBtn, &QToolButton::clicked, this, removeCurrentSelection);
  // --- Delete selected items on Shift + Delete
  connect(delShortcut, &QShortcut::activated, this, std::move(removeCurrentSelection));

  connect(addBtn, &QToolButton::clicked, this, [imModel, tblView](){
    tblView->selectRow(imModel->addNewItem(std::make_shared<KeySequenceAction>()));
  });

  layout->addLayout(intervalLayout);
  layout->addWidget(tblView);
  return imWidget;
}

// -------------------------------------------------------------------------------------------------
void DevicesWidget::createDeviceComboBox(Spotlight* spotlight)
{
  m_devicesCombo = new QComboBox(this);
  m_devicesCombo->setToolTip(tr("List of connected devices."));

  for (const auto& dev : spotlight->connectedDevices()) {
    const auto data = QVariant::fromValue(dev.id);
    if (m_devicesCombo->findData(data) < 0) {
      m_devicesCombo->addItem(descriptionString(dev.name, dev.id), data);
    }
  }

  connect(spotlight, &Spotlight::deviceDisconnected, this,
  [this](const DeviceId& id, const QString& /*name*/)
  {
    const auto idx = m_devicesCombo->findData(QVariant::fromValue(id));
    if (idx >= 0) {
      m_devicesCombo->removeItem(idx);
    }
  });

  connect(spotlight, &Spotlight::deviceConnected, this,
  [this](const DeviceId& id, const QString& name)
  {
    const auto data = QVariant::fromValue(id);
    if (m_devicesCombo->findData(data) < 0) {
      m_devicesCombo->addItem(descriptionString(name, id), data);
    }
  });

  connect(m_devicesCombo, static_cast<void (QComboBox::*)(int)>(&QComboBox::currentIndexChanged), this,
  [this, spotlight](int index)
  {
    if (index < 0)
    {
      m_inputMapper = nullptr;
      emit currentDeviceChanged(invalidDeviceId);
      return;
    }

    const auto devId = qvariant_cast<DeviceId>(m_devicesCombo->itemData(index));
    const auto currentConn = spotlight->deviceConnection(devId);
    m_inputMapper = currentConn ? currentConn->inputMapper().get() : nullptr;
    emit currentDeviceChanged(devId);
  });

  const auto currentConn = spotlight->deviceConnection(currentDeviceId());
  m_inputMapper = currentConn ? currentConn->inputMapper().get() : nullptr;
}

// -------------------------------------------------------------------------------------------------
QWidget* DevicesWidget::createDisconnectedStateWidget()
{
  const auto stateWidget = new QWidget(this);
  const auto hbox = new QHBoxLayout(stateWidget);
  const auto label = new QLabel(tr("No devices connected."), stateWidget);
  label->setToolTip(label->text());
  auto icon = style()->standardIcon(QStyle::SP_MessageBoxWarning);
  const auto iconLabel = new QLabel(stateWidget);
  iconLabel->setPixmap(icon.pixmap(16,16));
  hbox->addStretch();
  hbox->addWidget(iconLabel);
  hbox->addWidget(label);
  hbox->addStretch();
  return stateWidget;
}

