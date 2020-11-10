// This file is part of Projecteur - https://github.com/jahnf/projecteur - See LICENSE.md and README.md
import QtQuick 2.3
import QtQuick.Window 2.2
import QtGraphicalEffects 1.0

import Projecteur.Utils 1.0 as Utils

Window {
    id: mainWindow
    property var screenId: -1
    readonly property bool showSpot: ProjecteurApp.currentSpotScreen === screenId

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

        opacity: ProjecteurApp.overlayVisible ? 1.0 : 0.0
        Behavior on opacity { PropertyAnimation { easing.type: Easing.OutQuad } }

        Item {
            id: desktopItem
            anchors.centerIn: centerRect
            visible: false; enabled: false; clip: true
            scale: Settings.zoomFactor
            width: centerRect.width / scale; height: centerRect.height / scale

            Utils.Image {
                id: desktopImage
                pixmap: DesktopImage.pixmap
                smooth: rotation == 0 ? false : true
                rotation: -rotationItem.rotation
                readonly property real xOffset: Math.floor(parent.width/2.0 + ((rotationItem.width-mainWindow.width)/2))
                readonly property real yOffset: Math.floor(parent.height/2.0 + ((rotationItem.height-mainWindow.height)/2))
                x: -ma.mouseX + xOffset
                y: -ma.mouseY + yOffset
                width: mainWindow.width; height: mainWindow.height
            }
        }

        OpacityMask {
            visible: Settings.zoomEnabled
            cached: true
            anchors.fill: centerRect
            source: desktopItem
            maskSource: spotShapeLoader.item
            enabled: false
        }

        Item {
            anchors.fill: parent
            MouseArea {
                id: ma
                cursorShape: Settings.cursor
                anchors.fill: parent
                hoverEnabled: true
                onClicked: { ProjecteurApp.spotlightWindowClicked() }
                onExited: { ProjecteurApp.cursorExitedWindow() }
                onEntered: { ProjecteurApp.cursorEntered(screenId) }
            }
        }

        Rectangle {
            property int spotSize: (mainWindow.height / 100.0) * Settings.spotSize
            id: centerRect
            readonly property int dynamicHeight:
                mainWindow.showSpot ? spotSize > 50
                                    ? Math.min(spotSize, mainWindow.height) : 50 : 0;
            opacity: Settings.shadeOpacity
            height: dynamicHeight
            width: height
            x: ma.mouseX - width/2
            y: ma.mouseY - height/2
            color: Settings.shadeColor
            visible: false
            enabled: false

        }

        Loader {
            id: spotShapeLoader
            visible: false; enabled: false
            anchors.centerIn: centerRect
            width: centerRect.width;  height: width
            sourceComponent: Qt.createComponent(Settings.spotShape)
        }

        OpacityMask {
            id: spot
            visible: Settings.showSpotShade
            opacity: centerRect.opacity
            cached: true
            invert: true
            anchors.fill: centerRect
            source: centerRect
            maskSource: spotShapeLoader.item
            enabled: false
        }

        Loader {
            id: borderShapeLoader
            anchors.centerIn: centerRect
            width: centerRect.width;  height: width
            visible: false; enabled: false
            sourceComponent: spotShapeLoader.sourceComponent
            onStatusChanged: {
                if (status == Loader.Ready) {
                    borderShapeLoader.item.color = Qt.binding(function(){ return Settings.borderColor; })
                }
            }
        }

        Item {
            id: borderShapeMask
            anchors.centerIn: centerRect
            width: centerRect.width;  height: width
            enabled: false; visible: false
            Item {
                id: borderShapeScaled
                anchors.centerIn: parent
                width: parent.width; height: width
                scale: (100 - Settings.borderSize) * 1.0 / 100.0
                property Component component: borderShapeLoader.sourceComponent
                property QtObject innerObject
                onComponentChanged: {
                    if (innerObject) innerObject.destroy()
                    innerObject = component.createObject(borderShapeScaled, {visible: true})
                }
            }
        }

        OpacityMask {
            id: spotBorder
            visible: Settings.showBorder && Settings.borderSize > 0
            opacity: Settings.borderOpacity
            cached: true
            invert: true
            anchors.fill: centerRect
            source: borderShapeLoader.item
            maskSource: borderShapeMask
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
            opacity: Settings.dotOpacity
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
} // Window
