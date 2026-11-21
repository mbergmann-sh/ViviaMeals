// Lizenztext
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

ScrollPage {
    header: PageHeader { title: qsTr("Lizenz") }
    Label {
        Layout.fillWidth: true
        text: app.licenseText()
        wrapMode: Label.WordWrap
        font.family: "monospace"
        font.pixelSize: 12
    }
}
