list(APPEND _PkgDeps_Projecteur_opensuse
  "libqt5-qtgraphicaleffects >= 5.6"
  "libQt5Widgets5 >= 5.6"
)

list(APPEND _PkgDeps_Projecteur_debian
  "qml-module-qtgraphicaleffects (>= 5.6)"
  "libqt5widgets5 (>= 5.6)"
)

list(APPEND PkgDependencies_MAP_Projecteur
  "debian::_PkgDeps_Projecteur_debian"
  "ubuntu::_PkgDeps_Projecteur_debian"
  "opensuse::_PkgDeps_Projecteur_opensuse"
  "opensuse-leap::_PkgDeps_Projecteur_opensuse"
)
