import QtQuick 2.3
import QtQuick.Window 2.2
import QtGraphicalEffects 1.0

Window {
    id: mainWindow
    visible: true
    width: 3; height: 3

    flags: Qt.FramelessWindowHint | Qt.Window | Qt.WindowStaysOnTopHint

    color: "transparent"

    Item {
        anchors.fill:parent
        MouseArea {
            id: ma
            cursorShape: Qt.BlankCursor // TODO make configurable
            anchors.fill: parent
            hoverEnabled: true
        }
    }

    Rectangle {
        property int spotSize: (mainWindow.height / 100.0) * Settings.spotSize
        id: centerRect
        opacity: Settings.shadeOpacity
        height: spotSize > 50 ? Math.min(spotSize, mainWindow.height) : 50;
        width: height
        x: ma.mouseX - width/2
        y: ma.mouseY - height/2
        color: Settings.shadeColor
        visible: false
        enabled: false
    }

    Rectangle {
        id: circle
        width: centerRect.width;  height: width
        radius: width*0.5
        visible: false
        enabled: false
    }

    OpacityMask {
        opacity: centerRect.opacity
        cached: true
        invert: true
        anchors.fill: centerRect
        source: centerRect
        maskSource: circle
        enabled: false
    }

    Rectangle {
        id: dotCursor
        antialiasing: true
        anchors.centerIn: centerRect
        width: Settings.dotSize; height: width
        radius: width*0.5
        color: Settings.dotColor
        visible: Settings.showCenterDot
        enabled: false
    }

    Rectangle {
        id: topRect
        color: centerRect.color
        opacity: centerRect.opacity
        anchors{ top: parent.top; bottom: centerRect.top; left: parent.left; right: parent.right }
        enabled: false
    }

    Rectangle {
        id: bottomRect
        color: centerRect.color
        opacity: centerRect.opacity
        anchors{ top: centerRect.bottom; bottom: parent.bottom; left: parent.left; right: parent.right }
        enabled: false
    }

    Rectangle {
        id: leftRect
        color: centerRect.color
        opacity: centerRect.opacity
        anchors{ top: topRect.bottom; bottom: bottomRect.top; left: parent.left; right: centerRect.left }
        enabled: false
    }

    Rectangle {
        id: rightRect
        color: centerRect.color
        opacity: centerRect.opacity
        anchors{ top: topRect.bottom; bottom: bottomRect.top; left: centerRect.right; right: parent.right }
        enabled: false
    }
}
