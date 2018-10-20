import QtQuick 2.3
import QtQuick.Window 2.2
import QtGraphicalEffects 1.0

Window {
    id: mainWindow
    visible: true
    width: 3; height: 3

    flags: Qt.FramelessWindowHint | Qt.Window
           | Qt.WindowDoesNotAcceptFocus | Qt.WindowStaysOnTopHint

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
        opacity: 0.3 // TODO: get from settings
        height: spotSize > 50 ? Math.min(spotSize, mainWindow.height) : 50;
        width: height
        x: ma.mouseX - width/2
        y: ma.mouseY - height/2
        color: "#222222" // TODO: get from settings.
        visible: false
    }

    Rectangle {
        id: circle
        width: centerRect.width;  height: width
        radius: width*0.5
        visible: false
    }

    OpacityMask {
        opacity: centerRect.opacity
        cached: true
        invert: true
        anchors.fill: centerRect
        source: centerRect
        maskSource: circle
    }

    Rectangle {
        id: dotCursor // TODO: configurable as "cursor"
        antialiasing: true
        anchors.centerIn: centerRect
        width: 5; height: width // TODO: color and size configurable
        radius: width*0.5
        color: "red"
    }

    Rectangle {
        id: topRect
        color: centerRect.color
        opacity: centerRect.opacity
        anchors{ top: parent.top; bottom: centerRect.top; left: parent.left; right: parent.right }
    }

    Rectangle {
        id: bottomRect
        color: centerRect.color
        opacity: centerRect.opacity
        anchors{ top: centerRect.bottom; bottom: parent.bottom; left: parent.left; right: parent.right }
    }

    Rectangle {
        id: leftRect
        color: centerRect.color
        opacity: centerRect.opacity
        anchors{ top: topRect.bottom; bottom: bottomRect.top; left: parent.left; right: centerRect.left }
    }

    Rectangle {
        id: rightRect
        color: centerRect.color
        opacity: centerRect.opacity
        anchors{ top: topRect.bottom; bottom: bottomRect.top; left: centerRect.right; right: parent.right }
    }
}
