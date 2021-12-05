/*
    SPDX-FileCopyrightText: 2012-2013 Daniel Nicoletti <dantti12@gmail.com>
    SPDX-FileCopyrightText: 2013-2015 Kai Uwe Broulik <kde@privat.broulik.de>

    SPDX-License-Identifier: LGPL-2.0-or-later
*/

import QtQuick 2.15
import QtQuick.Layouts 1.15

import org.kde.kcoreaddons 1.0 as KCoreAddons
import org.kde.plasma.components 3.0 as PlasmaComponents3
import org.kde.plasma.core 2.1 as PlasmaCore
import org.kde.plasma.extras 2.0 as PlasmaExtras
import org.kde.plasma.workspace.components 2.0

import "logic.js" as Logic

Item {
    id: batteryItem
    height: childrenRect.height

    property var battery

    // NOTE: According to the UPower spec this property is only valid for primary batteries, however
    // UPower seems to set the Present property false when a device is added but not probed yet
    readonly property bool isPresent: model["Plugged in"]

    readonly property bool isBroken: model.Capacity > 0 && model.Capacity < 50

    // Existing instance of a slider to use as a reference to calculate extra
    // margins for a progress bar, so that the row of labels on top of it
    // could visually look as if it were on the same distance from the bar as
    // they are from the slider.
    property PlasmaComponents3.Slider matchHeightOfSlider: PlasmaComponents3.Slider {}
    readonly property real extraMargin: Math.max(0, Math.floor((matchHeightOfSlider.height - chargeBar.height) / 2))

    component BatteryDetails : Flow { // GridLayout crashes with a Repeater in it somehow
        id: detailsLayout

        required property bool inListView

        property int leftColumnWidth: 0
        width: PlasmaCore.Units.gridUnit * 11

        enabled: false // makes PC3.Labels semi-transparent through implicit inheritance

        PlasmaComponents3.Label {
            id: brokenBatteryLabel
            width: parent ? parent.width : implicitWidth
            wrapMode: Text.WordWrap
            text: batteryItem.isBroken && typeof model.Capacity !== "undefined" ? i18n("This battery's health is at only %1% and should be replaced. Please contact your hardware vendor for more details.", model.Capacity) : ""
            font: detailsLayout.inListView ? PlasmaCore.Theme.smallestFont : PlasmaCore.Theme.defaultFont
            visible: batteryItem.isBroken
        }

        Repeater {
            model: Logic.batteryDetails(batteryItem.battery, batterymonitor.remainingTime)

            PlasmaComponents3.Label {
                id: detailsLabel
                width: modelData.value && parent
                    ? parent.width - detailsLayout.leftColumnWidth - PlasmaCore.Units.smallSpacing
                    : detailsLayout.leftColumnWidth + PlasmaCore.Units.smallSpacing
                wrapMode: Text.NoWrap
                onPaintedWidthChanged: { // horrible HACK to get a column layout
                    if (paintedWidth > detailsLayout.leftColumnWidth) {
                        detailsLayout.leftColumnWidth = paintedWidth
                    }
                }
                height: implicitHeight
                text: modelData.value ? modelData.value : modelData.label

                states: [
                    State {
                        when: detailsLayout.inListView // HACK
                        PropertyChanges {
                            target: detailsLabel
                            horizontalAlignment: modelData.value ? Text.AlignRight : Text.AlignLeft
                            font: PlasmaCore.Theme.smallestFont
                            width: parent ? parent.width / 2 : 0
                            elide: Text.ElideNone // eliding and height: implicitHeight causes loops
                        }
                    }
                ]
            }
        }
    }

    Column {
        width: parent.width
        spacing: PlasmaCore.Units.smallSpacing

        PlasmaCore.ToolTipArea {
            width: parent.width
            height: infoRow.height
            active: !detailsLoader.active
            z: 2

            mainItem: Row {
                id: batteryItemToolTip

                Layout.minimumWidth: implicitWidth + PlasmaCore.Units.smallSpacing * 2
                Layout.minimumHeight: implicitHeight + PlasmaCore.Units.gridUnit
                Layout.maximumWidth: implicitWidth + PlasmaCore.Units.smallSpacing * 2
                Layout.maximumHeight: implicitHeight + PlasmaCore.Units.gridUnit
                width: implicitWidth + PlasmaCore.Units.smallSpacing * 2
                height: implicitHeight + PlasmaCore.Units.gridUnit

                spacing: PlasmaCore.Units.gridUnit

                BatteryIcon {
                    x: PlasmaCore.Units.gridUnit
                    y: PlasmaCore.Units.smallSpacing * 2
                    width: PlasmaCore.Units.iconSizes.desktop // looks weird and small but that's what DefaultTooltip uses
                    height: width
                    batteryType: batteryIcon.batteryType
                    percent: batteryIcon.percent
                    hasBattery: batteryIcon.hasBattery
                    pluggedIn: batteryIcon.pluggedIn
                    visible: !batteryItem.isBroken
                }

                Column {
                    id: mainColumn
                    x: PlasmaCore.Units.smallSpacing * 2
                    y: PlasmaCore.Units.smallSpacing * 2

                    PlasmaExtras.Heading {
                        level: 3
                        text: batteryNameLabel.text
                    }
                    Loader {
                        sourceComponent: BatteryDetails {
                            inListView: false
                        }
                    }
                }
            }

            RowLayout {
                id: infoRow
                width: parent.width
                spacing: PlasmaCore.Units.gridUnit

                BatteryIcon {
                    id: batteryIcon
                    Layout.alignment: Qt.AlignTop
                    width: PlasmaCore.Units.iconSizes.medium
                    height: width
                    batteryType: model.Type
                    percent: model.Percent
                    hasBattery: batteryItem.isPresent
                    pluggedIn: model.State === "Charging" && model["Is Power Supply"]
                }

                ColumnLayout {
                    Layout.fillWidth: true
                    Layout.alignment: batteryItem.isPresent ? Qt.AlignTop : Qt.AlignVCenter
                    spacing: 0

                    RowLayout {
                        spacing: PlasmaCore.Units.smallSpacing

                        PlasmaComponents3.Label {
                            id: batteryNameLabel
                            Layout.fillWidth: true
                            elide: Text.ElideRight
                            text: model["Pretty Name"]
                        }

                        PlasmaComponents3.Label {
                            text: Logic.stringForBatteryState(model)
                            visible: model["Is Power Supply"]
                            enabled: false
                        }

                        PlasmaComponents3.Label {
                            id: batteryPercent
                            horizontalAlignment: Text.AlignRight
                            visible: batteryItem.isPresent
                            text: i18nc("Placeholder is battery percentage", "%1%", model.Percent)
                        }
                    }

                    PlasmaComponents3.ProgressBar {
                        id: chargeBar

                        Layout.fillWidth: true
                        Layout.topMargin: batteryItem.extraMargin
                        Layout.bottomMargin: batteryItem.extraMargin

                        from: 0
                        to: 100
                        visible: batteryItem.isPresent
                        value: Number(model.Percent)
                    }
                }
            }
        }

        Loader {
            id: detailsLoader
            anchors {
                left: parent.left
                leftMargin: batteryIcon.width + PlasmaCore.Units.gridUnit
                right: parent.right
            }
            sourceComponent: BatteryDetails {
                inListView: true
            }
        }

        InhibitionHint {
            anchors {
                left: parent.left
                leftMargin: batteryIcon.width + PlasmaCore.Units.gridUnit
                right: parent.right
            }
            readonly property var chargeStopThreshold: pmSource.data["Battery"] ? pmSource.data["Battery"]["Charge Stop Threshold"] : undefined
            readonly property bool pluggedIn: pmSource.data["AC Adapter"] !== undefined && pmSource.data["AC Adapter"]["Plugged in"]
            visible: pluggedIn && typeof chargeStopThreshold === "number" && chargeStopThreshold > 0 && chargeStopThreshold < 100
            iconSource: "kt-speed-limits" // FIXME good icon
            text: i18n("Your battery is configured to only charge up to %1%.", chargeStopThreshold || 0)
        }
    }
}
