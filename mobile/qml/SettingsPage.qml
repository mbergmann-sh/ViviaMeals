// Einstellungen
import QtQuick
import QtQuick.Controls
import QtQuick.Controls.Material
import QtQuick.Layouts

ScrollPage {
    header: PageHeader { title: qsTr("Einstellungen") }

    SectionLabel { text: qsTr("Erscheinungsbild") }
    Card {
        Layout.fillWidth: true
        ColumnLayout {
            width: parent.width
            RadioButton { text: qsTr("Wie im System eingestellt"); checked: app.themeMode === 0; onClicked: app.themeMode = 0 }
            RadioButton { text: qsTr("Hell"); checked: app.themeMode === 1; onClicked: app.themeMode = 1 }
            RadioButton { text: qsTr("Dunkel"); checked: app.themeMode === 2; onClicked: app.themeMode = 2 }
        }
    }

    SectionLabel { text: qsTr("Beim Kochen") }
    Card {
        Layout.fillWidth: true
        ColumnLayout {
            width: parent.width
            Switch {
                Layout.fillWidth: true
                text: qsTr("Bildschirm anlassen, solange ein Rezept angezeigt wird")
                checked: app.keepScreenOn
                onToggled: app.keepScreenOn = checked
            }
            Label {
                Layout.fillWidth: true
                wrapMode: Label.WordWrap
                font.pixelSize: 13
                opacity: 0.7
                text: qsTr("So geht das Display nicht aus, während Sie mit Teig an den Fingern lesen.")
            }
        }
    }

    SectionLabel { text: qsTr("Daten") }
    Card {
        Layout.fillWidth: true
        Label {
            width: parent.width
            wrapMode: Label.WordWrap
            text: qsTr("Alle Rezepte, das Tagebuch und die Einkaufsliste liegen nur auf diesem Gerät – und, wenn Sie den Abgleich einrichten, auf Ihrem eigenen Computer oder Ihrer eigenen Website. ViviaMeals braucht kein Konto, nutzt keine fremde Cloud und zeigt keine Werbung.")
        }
    }
}
