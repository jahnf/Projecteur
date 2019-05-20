// This file is part of Projecteur - https://github.com/jahnf/projecteur - See LICENSE.md and README.md
import QtQuick 2.3

// (Rounded) Square spotlight shape
Rectangle {
    anchors.fill: parent
    radius: width * 0.5 * (Settings.shapes.Square.radius / 100.0)
    visible: false
    enabled: false
}
