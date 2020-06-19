# Changelog

## v0.8

### Changes/Updates:

- Device button mapping: Map any button on your device to (almost)
  any button combination.
- Store and load different setting presets.
- Spotlight fade in/out effect.
- Additional command line options:
  - `-m, --minimize-only`: Preferences dialog can only be minimized,
    particular useful on desktops without system tray.
  - `--show-dialog` : Preferences dialog will be shown at application start.
- Show third-party licenses in about dialog.
- Spotlight center dot opacity configurable.
- Under the hood: Restructure device connection; Preparation for additional
  _hidraw_ communication with the device (vibration and other features).
- Under the hood: switched CI builds to Github actions.
- Automated Fedora 32 and Ubuntu 20.04 builds.
- Additional automated package/build artifact upload to
  [cloudsmith.io](https://cloudsmith.io/~jahnf/repos/projecteur-develop/).

## v0.7

### Changes/Updates:

- Added the support to use with other devices (compile and run time).
- Added logging output (UI and console) with different log levels.
- Under the hood: Integration of a virtual device via uinput
  (preparation for button mapping feature in v0.8)
- Rename `55-spotlight.rules` to `55-projecteur.rules`
- CentOS-8 package build.

## v0.6

### Changes/Updates:

- Spotlight zoom Feature.
- Updated udev rules, no need to add the user to a special group anymore.
- Automated build of Fedora packages and Arch Linux packages.
- Configurable spotlight borders.
- Scriptability: Properties can be set via command line.
- New Command line option for device scan.
