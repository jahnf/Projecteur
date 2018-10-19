
import QtQuick 2.3
import QtQuick.Window 2.2
import QtGraphicalEffects 1.0

Window {
    id: mainWindow
    visible: true
    width: 99; height: 99

    flags: Qt.FramelessWindowHint | Qt.Window
           | Qt.WindowDoesNotAcceptFocus | Qt.WindowStaysOnTopHint

    color: "transparent"

    Item {
        anchors.fill:parent
        MouseArea {
            id: ma
            anchors.fill: parent
            hoverEnabled: true
        }
    }

    Rectangle {
        id: centerRect
        opacity: 0.3 // TODO: get from settings
        width: 200; height:200 // TODO: get size of spot from settings, with sane default settings depending screen resolution
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
