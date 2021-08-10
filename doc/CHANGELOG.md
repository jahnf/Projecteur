# Projecteur Changelog

## v1.0

### Changes/Updates:

- Logitech Spotlight Bluetooth vibration & hidraw support ([#140][p140]);
- Logitech Spotlight Scrolling and Audio Volume functionality ([#85][i85]); Many thanks to @mayanksuman
- Bug fix for high CPU load in certain situations ([#133][i133])
- Bug fix for wrong button mapping for inputs with same length ([#144][i144])
- Added automated builds for Fedora 34, Debian 11 (Bullseye) and OpenSUSE 15.3 ([#148][i148])

Many thanks to *[@mayanksuman][c-mayanksuman]* for Logitech Bluetooth, Scrolling and Audio volume
support.

[p140]: https://github.com/jahnf/Projecteur/pull/140
[i85]:  https://github.com/jahnf/Projecteur/issues/85
[i133]: https://github.com/jahnf/Projecteur/issues/133
[i144]: https://github.com/jahnf/Projecteur/issues/144
[i148]: https://github.com/jahnf/Projecteur/issues/148
[c-mayanksuman]: https://github.com/mayanksuman

## v0.9.1

### Changes/Updates:

- Fixes for automatically generated RPM Packages (especially Fedora)
- Fixes for version numbers in generated packages (DEB and RPM)

## v0.9

### Changes/Updates:

- Added man pages and Appstream files - thanks to *[@llimeht][c-llimeht]* ([#97][p97]);
- Command line option to toggle the spotlight ([#104][i104]);
- Bugfix when moving the cursor from one screen to a different screen with higher resolution;
- Multi-screen overlay option ([#80][i80]);
- Added bash-completion ([#110][p110]);
- Added automated Fedora-33 build ([#111)][p111];
- Added automated OpenSUSE 15.2 build ([#115][p115]);
- Automated build: Added automated CodeQL security analysis ([#113][p113]);
- Added vibration support for the Logitech Spotlight (USB) ([#6][i6]);

[p97]:  https://github.com/jahnf/Projecteur/pull/97
[i104]: https://github.com/jahnf/Projecteur/issues/104
[i80]:  https://github.com/jahnf/Projecteur/issues/80
[p110]: https://github.com/jahnf/Projecteur/pull/110
[p111]: https://github.com/jahnf/Projecteur/pull/111
[p115]: https://github.com/jahnf/Projecteur/pull/115
[p113]: https://github.com/jahnf/Projecteur/pull/113
[i6]:   https://github.com/jahnf/Projecteur/issues/6
[c-llimeht]: https://github.com/llimeht

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
