// Kopfzeile einer Seite: Zurück, Titel, eigene Schaltflächen rechts
import QtQuick
import QtQuick.Controls
import QtQuick.Controls.Material
import QtQuick.Layouts

ToolBar {
    id: bar
    property string title
    property bool showBack: Ui.main ? Ui.main.canGoBack : false
    default property alias actions: actionRow.data
    Material.foreground: "white"
    Material.background: Ui.blue

    RowLayout {
        anchors.fill: parent
        anchors.leftMargin: bar.showBack ? 0 : 16
        spacing: 0
        ToolButton {
            visible: bar.showBack
            icon.source: Ui.icon("back")
            icon.color: "white"
            onClicked: Ui.main.back()
            Accessible.name: qsTr("Zurück")
        }
        Label {
            text: bar.title
            font.pixelSize: 20
            font.weight: Font.Medium
            elide: Label.ElideRight
            Layout.fillWidth: true
        }
        Row { id: actionRow }
    }
}
