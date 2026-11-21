// Nährwerte je Portion
import QtQuick
import QtQuick.Controls
import QtQuick.Controls.Material
import QtQuick.Layouts

Card {
    id: nv
    property var nut: ({})
    property string perLabel: qsTr("Portion")
    property bool live: false     // im Editor: „wird berechnet“
    readonly property bool valid: nut && nut.kcal > 0

    ColumnLayout {
        width: parent.width
        spacing: 6
        RowLayout {
            Layout.fillWidth: true
            Icon { iconName: "nutrition"; size: 22; opacity: 0.7 }
            Label {
                Layout.fillWidth: true
                text: nv.live ? qsTr("Nährwerte je %1 (werden laufend berechnet)").arg(nv.perLabel)
                              : qsTr("Nährwerte je %1").arg(nv.perLabel)
                font.weight: Font.Medium
                wrapMode: Label.WordWrap
            }
        }
        RowLayout {
            visible: nv.valid
            spacing: 6
            Label { text: nv.valid ? Ui.num(nv.nut.kcal) : ""; font.pixelSize: 30; font.weight: Font.Bold; color: Ui.orange }
            Label { text: "kcal"; font.pixelSize: 16; Layout.alignment: Qt.AlignBaseline }
            Label { text: nv.valid ? "· " + Ui.num(nv.nut.kJ) + " kJ" : ""; opacity: 0.7; Layout.alignment: Qt.AlignBaseline }
        }
        GridLayout {
            visible: nv.valid
            Layout.fillWidth: true
            columns: nv.width > 420 ? 6 : 3
            columnSpacing: 8
            rowSpacing: 6
            Repeater {
                model: [[qsTr("Eiweiß"), "protein", "g"], [qsTr("Fett"), "fat", "g"], [qsTr("Kohlenh."), "carbs", "g"],
                        [qsTr("Zucker"), "sugar", "g"], [qsTr("Ballastst."), "fiber", "g"], [qsTr("Salz"), "salt", "g"]]
                delegate: ColumnLayout {
                    required property var modelData
                    Layout.fillWidth: true
                    spacing: 0
                    Label { text: (nv.nut[modelData[1]] || "0") + " " + modelData[2]; font.weight: Font.DemiBold }
                    Label { text: modelData[0]; font.pixelSize: 12; opacity: 0.7 }
                }
            }
        }
        Label {
            visible: !nv.valid
            Layout.fillWidth: true
            wrapMode: Label.WordWrap
            opacity: 0.75
            text: nv.live ? qsTr("Sobald Zutaten mit Mengen eingetragen sind, erscheinen hier Kalorien und Nährwerte.")
                          : qsTr("Für dieses Rezept liegen keine Nährwerte vor.")
        }
        Label {
            visible: nv.valid && nv.nut.coverage !== undefined && nv.nut.coverage < 100
            Layout.fillWidth: true
            wrapMode: Label.WordWrap
            font.pixelSize: 13
            color: Ui.orange
            text: qsTr("%1 % der Zutaten erkannt").arg(nv.nut.coverage)
                  + ((nv.nut.unmatched && nv.nut.unmatched.length) ? " – " + qsTr("ohne Nährwerte: %1").arg(nv.nut.unmatched.join(", ")) : "")
        }
    }
}
