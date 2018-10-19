
import QtQuick 2.3
import QtQuick.Window 2.2

Window {
    id: mainWindow
    visible: true
    width: 222
    height: 222
    flags: Qt.FramelessWindowHint | Qt.Window
           | Qt.WindowDoesNotAcceptFocus | Qt.WindowStaysOnTopHint

    color: "transparent"

    Rectangle {
        color: "brown"
        opacity: 0.3
        anchors.fill: parent
        anchors.margins: 10
        radius: 10

        MouseArea {
            id: ma
            anchors.fill: parent
            hoverEnabled: true
//            onClicked: {
//                Qt.quit();
//            }
        }
    }

    Rectangle {
        color: "red"
        width: 50
        height: 50
        x: ma.mouseX - width/2
        y: ma.mouseY - height/2
    }
}
