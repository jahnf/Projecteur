list(APPEND _PkgDeps_Projecteur_opensuse
  "libqt5-qtgraphicaleffects >= 5.7"
  "libQt5Widgets5 >= 5.7"
  "shadow"
  "udev"
)

list(APPEND _PkgDeps_Projecteur_fedora
  "qt5 >= 5.7"
  "passwd"
  "udev"
)

list(APPEND _PkgDeps_Projecteur_debian
  "qml-module-qtgraphicaleffects (>= 5.7)"
  "libqt5widgets5 (>= 5.7)"
  "passwd"
  "udev"
)

list(APPEND PkgDependencies_MAP_Projecteur
  "debian::_PkgDeps_Projecteur_debian"
  "ubuntu::_PkgDeps_Projecteur_debian"
  "fedora::_PkgDeps_Projecteur_fedora"
  "opensuse::_PkgDeps_Projecteur_opensuse"
  "opensuse-leap::_PkgDeps_Projecteur_opensuse"
)
