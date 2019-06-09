// This file is part of Projecteur - https://github.com/jahnf/projecteur - See LICENSE.md and README.md
import QtQuick 2.3
import Projecteur.Shapes 1.0 as Shapes

// N-gon spotlight shape
Shapes.NGon {
    anchors.fill: parent
    sides: Settings.shapes.Ngon.sides
    visible: false
    enabled: false
}
