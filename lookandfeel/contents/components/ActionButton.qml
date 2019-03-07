/*
 *   Copyright 2016 David Edmundson <davidedmundson@kde.org>
 *
 *   This program is free software; you can redistribute it and/or modify
 *   it under the terms of the GNU Library General Public License as
 *   published by the Free Software Foundation; either version 2 or
 *   (at your option) any later version.
 *
 *   This program is distributed in the hope that it will be useful,
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *   GNU General Public License for more details
 *
 *   You should have received a copy of the GNU Library General Public
 *   License along with this program; if not, write to the
 *   Free Software Foundation, Inc.,
 *   51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
 */

import QtQuick 2.2
import org.kde.plasma.core 2.0 as PlasmaCore
import org.kde.plasma.components 2.0 as PlasmaComponents

Item {
    id: root
    property alias text: label.text
    property alias iconSource: icon.source
    property alias containsMouse: mouseArea.containsMouse
    property alias font: label.font
    property alias labelRendering: label.renderType
    property alias circleOpacity: iconCircle.opacity
    property alias circleVisiblity: iconCircle.visible
    signal clicked

    activeFocusOnTab: true

    property int iconSize: units.gridUnit * 3

    implicitWidth: Math.max(iconSize + units.largeSpacing * 2, label.contentWidth)
    implicitHeight: iconSize + units.smallSpacing + label.implicitHeight

    opacity: activeFocus || containsMouse ? 1 : 0.85
        Behavior on opacity {
            PropertyAnimation { // OpacityAnimator makes it turn black at random intervals
                duration: units.longDuration * 2
                easing.type: Easing.InOutQuad
            }
    }

    Rectangle {
        id: iconCircle
        anchors.centerIn: icon
        width: iconSize + units.smallSpacing
        height: width
        radius: width / 2
        color: PlasmaCore.ColorScope.textColor
        opacity: activeFocus || containsMouse ? 0.15 : 0
        Behavior on opacity {
                PropertyAnimation { // OpacityAnimator makes it turn black at random intervals
                    duration: units.longDuration * 3
                    easing.type: Easing.InOutQuad
                }
        }
    }

    PlasmaCore.IconItem {
        id: icon
        anchors {
            top: parent.top
            horizontalCenter: parent.horizontalCenter
        }
        width: iconSize
        height: iconSize

        colorGroup: PlasmaCore.ColorScope.colorGroup
        active: mouseArea.containsMouse || root.activeFocus
    }

    PlasmaComponents.Label {
        id: label
        font.pointSize: theme.defaultFont.pointSize + 1
        anchors {
            top: icon.bottom
            topMargin: units.smallSpacing
            left: parent.left
            right: parent.right
        }
        horizontalAlignment: Text.AlignHCenter
        verticalAlignment: Text.AlignTop
        wrapMode: Text.WordWrap
        font.underline: root.activeFocus
    }

    MouseArea {
        id: mouseArea
        hoverEnabled: true
        onClicked: root.clicked()
        anchors.fill: parent
    }

    Keys.onEnterPressed: clicked()
    Keys.onReturnPressed: clicked()
    Keys.onSpacePressed: clicked()

    Accessible.onPressAction: clicked()
    Accessible.role: Accessible.Button
    Accessible.name: label.text
}
