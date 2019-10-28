// This file is part of Projecteur - https://github.com/jahnf/projecteur - See LICENSE.md and README.md
import QtQuick 2.3
import QtQuick.Window 2.2
import QtGraphicalEffects 1.0

Window {
    id: mainWindow
    width: 300; height: 200

    flags: Qt.FramelessWindowHint | Qt.WindowStaysOnTopHint | Qt.SplashScreen

    color: "transparent"

    readonly property double diagonal: Math.sqrt(Math.pow(Math.max(width, height),2)*2)

    Item {
        id: rotationItem
        anchors.centerIn: parent
        width: rotation === 0 ? mainWindow.width : mainWindow.diagonal;
        height: rotation === 0 ? mainWindow.height : width
        rotation: Settings.spotRotationAllowed ? Settings.spotRotation : 0

        Item {
            anchors.fill: parent
            MouseArea {
                id: ma
                cursorShape: Settings.cursor
                anchors.fill: parent
                hoverEnabled: true
                onClicked: { mainWindow.hide() }
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

        Loader {
            id: spotShapeLoader
            anchors.centerIn: centerRect
            width: centerRect.width;  height: width
            sourceComponent: Qt.createComponent(Settings.spotShape)
        }
        Loader {
            id: spotShapeLoader2
            objectName: "spotarea"
            anchors.centerIn: centerRect
            width: centerRect.width;  height: width
            sourceComponent: Qt.createComponent(Settings.spotShape)
            visible: Settings.showBorder
            onVisibleChanged: {
                spotShapeLoader2.item.color="transparent";
                //if (Settings.showBorder){
                    spotShapeLoader2.item.opacity=Settings.shadeOpacity;
                    spotShapeLoader2.item.border.width=Settings.borderSize/100*spotShapeLoader2.width;
                    spotShapeLoader2.item.border.color=Settings.borderColor;
                    spotShapeLoader2.item.visible=true;
                //}

            }
        }

        OpacityMask {
            id: spot
            visible: Settings.showSpot
            opacity: centerRect.opacity
            cached: true
            invert: true
            anchors.fill: centerRect
            source: centerRect
            maskSource: spotShapeLoader.item
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
            visible: spot.visible
            color: centerRect.color
            opacity: centerRect.opacity
            anchors{ top: parent.top; bottom: centerRect.top; left: parent.left; right: parent.right }
            enabled: false
        }

        Rectangle {
            id: bottomRect
            visible: spot.visible
            color: centerRect.color
            opacity: centerRect.opacity
            anchors{ top: centerRect.bottom; bottom: parent.bottom; left: parent.left; right: parent.right }
            enabled: false
        }

        Rectangle {
            id: leftRect
            visible: spot.visible
            color: centerRect.color
            opacity: centerRect.opacity
            anchors{ top: topRect.bottom; bottom: bottomRect.top; left: parent.left; right: centerRect.left }
            enabled: false
        }

        Rectangle {
            id: rightRect
            visible: spot.visible
            color: centerRect.color
            opacity: centerRect.opacity
            anchors{ top: topRect.bottom; bottom: bottomRect.top; left: centerRect.right; right: parent.right }
            enabled: false
        }
    }
}
