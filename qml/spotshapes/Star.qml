// This file is part of Projecteur - https://github.com/jahnf/projecteur - See LICENSE.md and README.md
import QtQuick 2.3
import Projecteur.Shapes 1.0 as Shapes

// Star spotlight shape
Shapes.Star {
    anchors.fill: parent
    points: Settings.shapes.Star.points
    innerRadius: Settings.shapes.Star.innerRadius

    visible: false
    enabled: false
    antialiasing: true
}
