list(APPEND _PkgDeps_Projecteur_opensuse
  "libqt5-qtgraphicaleffects >= 5.6"
  "libQt5Widgets5 >= 5.6"
  "shadow"
  "udev"
)

list(APPEND _PkgDeps_Projecteur_debian
  "qml-module-qtgraphicaleffects (>= 5.6)"
  "libqt5widgets5 (>= 5.6)"
  "passwd"
  "udev"
)

list(APPEND _PkgDeps_Projecteur_ubuntu_1604
  "qml-module-qtgraphicaleffects (>= 5.5)"
  "libqt5widgets5 (>= 5.5)"
  "passwd"
  "udev"
)

list(APPEND PkgDependencies_MAP_Projecteur
  "debian::_PkgDeps_Projecteur_debian"
  "ubuntu::_PkgDeps_Projecteur_debian"
  "ubuntu-16.04::_PkgDeps_Projecteur_ubuntu_1604"
  "opensuse::_PkgDeps_Projecteur_opensuse"
  "opensuse-leap::_PkgDeps_Projecteur_opensuse"
)
