// Was die große Version am Computer kann (für Begrüßung und Hinweisseite)
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

ColumnLayout {
    spacing: 8
    Repeater {
        model: [
            ["ocr", qsTr("Rezepte aus Kochbüchern scannen oder aus Fotos erkennen lassen")],
            ["import", qsTr("ganze Sammlungen aus RezKonv, MealMaster und Ambrosia übernehmen")],
            ["web", qsTr("Rezepte direkt von Webseiten holen")],
            ["plan", qsTr("Wochenpläne passend zu Ihrem Ziel zusammenstellen lassen")],
            ["tex", qsTr("eigene Rezept-Website und Kochbücher (LaTeX, Writer) erstellen")]
        ]
        delegate: RowLayout {
            required property var modelData
            Layout.fillWidth: true
            spacing: 12
            Rectangle {
                Layout.preferredWidth: 30; Layout.preferredHeight: 30
                radius: 15
                color: "#FCE9DC"
                Icon { anchors.centerIn: parent; iconName: "check"; size: 18; color: Ui.orange }
            }
            Label { Layout.fillWidth: true; text: modelData[1]; wrapMode: Label.WordWrap; font.pixelSize: 16 }
        }
    }
}
