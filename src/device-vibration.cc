// This file is part of Projecteur - https://github.com/jahnf/projecteur - See LICENSE.md and README.md
#include "device-vibration.h"

#include "iconwidgets.h"

#include <QCheckBox>
#include <QFontDatabase>
#include <QGridLayout>
#include <QGroupBox>
#include <QSpinBox>
#include <QStackedWidget>
#include <QTimer>
#include <QVBoxLayout>

#include <array>
#include <chrono>

// TODO add vibration support for Logitech Spotlight and
// TODO generalize features and protocol for proprietary device features like vibration
//      for not only the Spotlight device.
//                                                    len         intensity
// unsigned char vibrate[] = {0x10, 0x01, 0x09, 0x1a, 0x00, 0xe8, 0x80};
// ::write(notifier->socket(), vibrate, 7);

// -------------------------------------------------------------------------------------------------
namespace {
  constexpr int numTimers = 3;
}

// -------------------------------------------------------------------------------------------------
struct TimerWidget::Impl
{
  // -----------------------------------------------------------------------------------------------
  Impl(QWidget* parent)
    : stack(new QStackedWidget(parent))
    , editor(new QWidget(parent))
    , overlay(new QWidget(parent))
    , checkbox(new QCheckBox(parent))
    , sbHours(new QSpinBox(parent))
    , sbMinutes(new QSpinBox(parent))
    , sbSeconds(new QSpinBox(parent))
    , btnStartStop(new IconButton(Font::Icon::media_control_48, parent))
    , timer(new QTimer(parent))
    , countdownTimer(new QTimer(parent))
    , overlayLabel(new QLabel(parent))
  {
    const auto layout = new QHBoxLayout(parent);
    layout->addWidget(checkbox);
    layout->addWidget(stack);
    layout->setMargin(0);

    stack->addWidget(editor);
    stack->addWidget(overlay);
    const auto editLayout = new QHBoxLayout(editor);
    const auto m = editLayout->contentsMargins();
    editLayout->setContentsMargins(m.left(), 0, m.right(), 0);
    editLayout->addWidget(sbHours);
    editLayout->addWidget(new QLabel(TimerWidget::tr("h"), editor));
    editLayout->addWidget(sbMinutes);
    editLayout->addWidget(new QLabel(TimerWidget::tr("m"), editor));
    editLayout->addWidget(sbSeconds);
    editLayout->addWidget(new QLabel(TimerWidget::tr("s"), editor));
    editLayout->addStretch(1);

    sbHours->setRange(0, 24);
    sbMinutes->setRange(0, 59);
    sbSeconds->setRange(0, 59);

    layout->addWidget(btnStartStop);
    btnStartStop->setCheckable(true);
    QObject::connect(btnStartStop, &IconButton::toggled, parent, [this](bool checked) {
      stack->setCurrentWidget(checked ? overlay : editor);
      btnStartStop->setText(checked ? QChar(Font::Icon::media_control_50)
                                    : QChar(Font::Icon::media_control_48));
      if (checked) {
        secondsLeft = valueSeconds();
        updateOverlayLabel(secondsLeft);
        countdownTimer->start();
        timer->start();
      } else {
        timer->stop();
        countdownTimer->stop();
      }
    });

    const auto overlayLayout = new QHBoxLayout(overlay);
    overlayLayout->addWidget(overlayLabel);
    overlayLayout->setContentsMargins(m.left(), 0, m.right(), 0);
    overlayLabel->setFont(QFontDatabase::systemFont(QFontDatabase::FixedFont));

    editor->setEnabled(checkbox->isChecked());
    btnStartStop->setEnabled(checkbox->isChecked());
    QObject::connect(checkbox, &QCheckBox::toggled, parent, [this](bool checked) {
      editor->setEnabled(checked);
      if (!checked) btnStartStop->setChecked(false);
      btnStartStop->setEnabled(checked);
    });

    QObject::connect(timer, &QTimer::timeout, parent, [this](){ btnStartStop->setChecked(false); });
    QObject::connect(sbHours, static_cast<void (QSpinBox::*)(int)>(&QSpinBox::valueChanged),
                     parent, [this](){ updateTimerInterval(); });
    QObject::connect(sbMinutes, static_cast<void (QSpinBox::*)(int)>(&QSpinBox::valueChanged),
                     parent, [this](){ updateTimerInterval(); });
    QObject::connect(sbSeconds, static_cast<void (QSpinBox::*)(int)>(&QSpinBox::valueChanged),
                     parent, [this](){ updateTimerInterval(); });

    timer->setSingleShot(true);
    countdownTimer->setInterval(1000);

    QObject::connect(countdownTimer, &QTimer::timeout, parent, [this](){
      updateOverlayLabel(--secondsLeft);
    });
  }

  int valueSeconds() const {
    return sbSeconds->value() + sbMinutes->value() * 60 + sbHours->value() * 60 * 60;
  }

  // -----------------------------------------------------------------------------------------------
  void updateTimerInterval() {
    timer->setInterval(valueSeconds() * 1000);
  }

  // -----------------------------------------------------------------------------------------------
  void updateOverlayLabel(int remainingSeconds)
  {
    const std::chrono::seconds remainingTime(remainingSeconds);
    const auto hours = std::chrono::duration_cast<std::chrono::hours>(remainingTime);
    const auto mins = std::chrono::duration_cast<std::chrono::minutes>(remainingTime-hours);
    const auto secs = std::chrono::duration_cast<std::chrono::seconds>(remainingTime-hours-mins);

    overlayLabel->setText(QString("%1:%2:%3")
                          .arg(hours.count(), 2, 10, QChar('0'))
                          .arg(mins.count(), 2, 10, QChar('0'))
                          .arg(secs.count(), 2, 10, QChar('0')));
  }

  // -----------------------------------------------------------------------------------------------
  QStackedWidget* stack = nullptr;
  QWidget* editor = nullptr;
  QWidget* overlay = nullptr;
  QCheckBox* checkbox = nullptr;
  QSpinBox* sbHours = nullptr;
  QSpinBox* sbMinutes = nullptr;
  QSpinBox* sbSeconds = nullptr;
  IconButton* btnStartStop = nullptr;
  QTimer* timer = nullptr;
  QTimer* countdownTimer = nullptr;
  QLabel* overlayLabel = nullptr;
  int secondsLeft = 0;
};

// -------------------------------------------------------------------------------------------------
TimerWidget::TimerWidget(QWidget* parent)
  : QWidget(parent)
  , m_impl(new Impl(this))
{
  connect(m_impl->timer, &QTimer::timeout, this, &TimerWidget::timeout);
}

// -------------------------------------------------------------------------------------------------
TimerWidget::~TimerWidget() = default;

// -------------------------------------------------------------------------------------------------
bool TimerWidget::timerEnabled() const {
  return m_impl->checkbox->isChecked();
}

// -------------------------------------------------------------------------------------------------
void TimerWidget::setTimerEnabled(bool enabled) {
  m_impl->checkbox->setChecked(enabled);
}

// -------------------------------------------------------------------------------------------------
bool TimerWidget::timerRunning() const {
  return m_impl->timer->isActive();
}

// -------------------------------------------------------------------------------------------------
void TimerWidget::start() {
  m_impl->btnStartStop->setChecked(true);
}

// -------------------------------------------------------------------------------------------------
void TimerWidget::stop() {
  m_impl->btnStartStop->setChecked(false);
}

// -------------------------------------------------------------------------------------------------
void TimerWidget::setValueSeconds(int seconds)
{
  const std::chrono::seconds totalSecs(seconds);
  const auto hours = std::chrono::duration_cast<std::chrono::hours>(totalSecs);
  const auto mins = std::chrono::duration_cast<std::chrono::minutes>(totalSecs-hours);
  const auto secs = std::chrono::duration_cast<std::chrono::seconds>(totalSecs-hours-mins);
  m_impl->sbHours->setValue(hours.count());
  m_impl->sbMinutes->setValue(mins.count());
  m_impl->sbSeconds->setValue(secs.count());
}

// -------------------------------------------------------------------------------------------------
void TimerWidget::setValueMinutes(int minutes) {
  setValueSeconds(minutes * 60);
}

// -------------------------------------------------------------------------------------------------
struct MultiTimerWidget::Impl
{
  Impl(QWidget* parent)
  {
    for (size_t i = 0; i < numTimers; ++i) {
      timers.at(i) = new TimerWidget(parent);
    }
  }

  std::array<TimerWidget*, numTimers> timers = {};
};

// -------------------------------------------------------------------------------------------------
MultiTimerWidget::MultiTimerWidget(QWidget* parent)
  : QWidget(parent)
  , m_impl(new Impl(this))
{
  const auto layout = new QHBoxLayout(this);
  const auto iconLabel = new IconLabel(Font::time_19, this);
  layout->addWidget(iconLabel);
  layout->setAlignment(iconLabel, Qt::AlignTop);

  const auto groupBox = new QGroupBox(tr("Timers"), this);
  groupBox->setSizePolicy(groupBox->sizePolicy().horizontalPolicy(),
                          QSizePolicy::Maximum);
  layout->addWidget(groupBox);
  layout->setAlignment(groupBox, Qt::AlignTop);
  const auto timerLayout = new QVBoxLayout(groupBox);

  for (size_t i = 0; i < numTimers; ++i) {
    timerLayout->addWidget(m_impl->timers.at(i), i, 0);
    m_impl->timers.at(i)->setValueMinutes(15 + i * 15);
  }

  layout->setStretch(0, 0);
  layout->setStretch(1, 1);
}

// -------------------------------------------------------------------------------------------------
MultiTimerWidget::~MultiTimerWidget() = default;

// -------------------------------------------------------------------------------------------------
int MultiTimerWidget::timerCount() const
{
  return numTimers;
}
