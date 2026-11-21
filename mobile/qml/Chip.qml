// kleines Schild (Thema, Zutat im Kühlschrank); optional mit Schließen-Knopf
import QtQuick
import QtQuick.Controls
import QtQuick.Controls.Material
import QtQuick.Layouts

Rectangle {
    id: chip
    property string text
    property bool selected: false
    property bool closable: false
    signal clicked()
    signal closeClicked()
    implicitHeight: 34
    implicitWidth: row.implicitWidth + 24
    radius: height / 2
    color: selected ? Ui.blue : (Material.theme === Material.Dark ? "#33373D" : "#EAF2FA")
    border.color: selected ? Ui.blue : (Material.theme === Material.Dark ? "#4A4F57" : "#C9DCEE")
    RowLayout {
        id: row
        anchors.centerIn: parent
        spacing: 4
        Label {
            text: chip.text
            color: chip.selected ? "white" : (Material.theme === Material.Dark ? "#E6E6E6" : "#1D3A55")
            font.pixelSize: 14
        }
        ToolButton {
            visible: chip.closable
            implicitWidth: 26; implicitHeight: 26
            padding: 2
            icon.source: Ui.icon("close")
            icon.width: 14; icon.height: 14
            icon.color: chip.selected ? "white" : Material.foreground
            onClicked: chip.closeClicked()
            Accessible.name: qsTr("Entfernen")
        }
    }
    MouseArea {
        anchors.fill: parent
        anchors.rightMargin: chip.closable ? 30 : 0
        onClicked: chip.clicked()
    }
}
