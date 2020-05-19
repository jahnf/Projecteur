// This file is part of Projecteur - https://github.com/jahnf/projecteur - See LICENSE.md and README.md
#pragma once

#include <QDialog>
#include <QToolButton>

class QComboBox;
class QGroupBox;
class Settings;
class Spotlight;

// -------------------------------------------------------------------------------------------------
class IconButton : public QToolButton
{
  Q_OBJECT
public:
  // Symbols in projecteur-icons.ttf - Icons from https://iconmonstr.com/
  enum Icon { // plus_5 and similar relate directly to iconmonstr name (e.g. plus-5 or control-panel-9)
    Add = 0xe930, plus_5 = Add,
    Trash = 0xe931, trash_can_1 = Trash,
    ControlPanel = 0xe932, control_panel_9 = ControlPanel,
    Share = 0xe933, share_8 = Share,
  };

  IconButton(Icon symbol, QWidget* parent = nullptr);
};

// -------------------------------------------------------------------------------------------------
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
  QWidget* createPresetSelector(Settings* settings);
#if HAS_Qt5_X11Extras
  QWidget* createCompositorWarningWidget();
#endif
  QWidget* createLogTabWidget();

private:
  bool m_active = false;
  QComboBox* m_screenCb = nullptr;
  quint32 m_discardedLogCount = 0;
};
