// ViviaMeals für Ihren Computer – freundlicher Hinweis auf die große Version (ohne Preis, ohne Kauf in der App)
import QtQuick
import QtQuick.Controls
import QtQuick.Controls.Material
import QtQuick.Layouts

ScrollPage {
    header: PageHeader { title: qsTr("ViviaMeals für Ihren Computer") }

    Rectangle {
        Layout.fillWidth: true
        Layout.preferredHeight: 150
        radius: Ui.radius
        gradient: Gradient {
            orientation: Gradient.Horizontal
            GradientStop { position: 0; color: Ui.blue }
            GradientStop { position: 1; color: Qt.lighter(Ui.blue, 1.35) }
        }
        RowLayout {
            anchors.fill: parent
            anchors.margins: 18
            spacing: 16
            Image { source: "qrc:/images/viviameals.svg"; sourceSize: Qt.size(84, 84) }
            Label {
                Layout.fillWidth: true
                text: qsTr("Rezepte bequem erfassen – mit ViviaMeals für Windows, Linux und Mac")
                color: "white"
                font.pixelSize: 20
                font.weight: Font.DemiBold
                wrapMode: Label.WordWrap
            }
        }
    }

    Label {
        Layout.fillWidth: true
        wrapMode: Label.WordWrap
        font.pixelSize: 16
        text: qsTr("Hier in der App haben Sie Ihre Rezepte immer dabei: ansehen, planen, einkaufen, kochen. Am Computer wird das Erfassen richtig bequem:")
    }
    DesktopFeatures { Layout.fillWidth: true }
    Label {
        Layout.fillWidth: true
        wrapMode: Label.WordWrap
        font.pixelSize: 16
        text: qsTr("Alles, was Sie dort erfassen, kommt per Abgleich automatisch hierher aufs Telefon – über Ihre eigene Website oder im Heimnetz, ohne fremde Cloud.")
    }
    Button {
        Layout.alignment: Qt.AlignHCenter
        Layout.topMargin: 8
        text: qsTr("Mehr erfahren")
        highlighted: true
        Material.accent: Ui.orange
        icon.source: Ui.icon("web")
        icon.color: "white"
        onClicked: Qt.openUrlExternally(app.productUrl)
    }
    Label {
        Layout.fillWidth: true
        horizontalAlignment: Text.AlignHCenter
        wrapMode: Label.WrapAnywhere
        font.pixelSize: 12
        opacity: 0.6
        text: app.productUrl.replace("https://", "")
    }
}
