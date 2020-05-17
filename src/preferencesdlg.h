// This file is part of Projecteur - https://github.com/jahnf/projecteur - See LICENSE.md and README.md
#pragma once

#include <QDialog>

class QComboBox;
class QGroupBox;
class Settings;
class Spotlight;

class PreferencesDialog : public QDialog
{
  Q_OBJECT
  Q_PROPERTY(bool dialogActive READ dialogActive NOTIFY dialogActiveChanged)

public:
  explicit PreferencesDialog(Settings* settings, Spotlight* spotlight, QWidget* parent = nullptr);
  virtual ~PreferencesDialog() override = default;

  bool dialogActive() const { return m_active; }

signals:
  void dialogActiveChanged(bool active);
  void testButtonClicked();

protected:
  virtual bool event(QEvent* event) override;

private:
  void setDialogActive(bool active);

  QWidget* createSettingsTabWidget(Settings* settings);
  QGroupBox* createShapeGroupBox(Settings* settings);
  QGroupBox* createSpotGroupBox(Settings* settings);
  QGroupBox* createDotGroupBox(Settings* settings);
  QGroupBox* createBorderGroupBox(Settings* settings);
  QGroupBox* createCursorGroupBox(Settings* settings);
  QGroupBox* createZoomGroupBox(Settings* settings);
#if HAS_Qt5_X11Extras
  QWidget* createCompositorWarningWidget();
#endif
  QWidget* createLogTabWidget();

private:
  bool m_active = false;
  QComboBox* m_screenCb = nullptr;
  quint32 m_discardedLogCount = 0;
};
