// Hinweis, wenn eine Liste leer ist
import QtQuick
import QtQuick.Controls
import QtQuick.Controls.Material
import QtQuick.Layouts

ColumnLayout {
    property string iconName: "info"
    property string title
    property string text
    spacing: 10
    Icon {
        Layout.alignment: Qt.AlignHCenter
        iconName: parent.iconName
        size: 56
        opacity: 0.35
    }
    Label {
        Layout.fillWidth: true
        horizontalAlignment: Text.AlignHCenter
        text: parent.title
        font.pixelSize: 18
        font.weight: Font.Medium
        wrapMode: Label.WordWrap
    }
    Label {
        Layout.fillWidth: true
        horizontalAlignment: Text.AlignHCenter
        text: parent.text
        wrapMode: Label.WordWrap
        opacity: 0.75
        visible: text.length > 0
    }
}
