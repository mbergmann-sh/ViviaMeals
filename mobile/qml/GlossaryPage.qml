// Küchen-Glossar
import QtQuick
import QtQuick.Controls
import QtQuick.Controls.Material
import QtQuick.Layouts

Page {
    id: page
    property string query
    property var hits: app.glossary(query)
    header: PageHeader { title: qsTr("Küchen-Glossar") }

    ColumnLayout {
        anchors.fill: parent
        spacing: 0
        TextField {
            Layout.fillWidth: true
            Layout.margins: Ui.gap
            placeholderText: qsTr("Begriff suchen, z. B. blanchieren")
            text: page.query
            inputMethodHints: Qt.ImhNoPredictiveText
            onTextChanged: delay.restart()
            Timer { id: delay; interval: 250; onTriggered: page.query = parent.text }
        }
        ListView {
            Layout.fillWidth: true
            Layout.fillHeight: true
            clip: true
            model: page.hits
            spacing: 8
            ScrollIndicator.vertical: ScrollIndicator {}
            delegate: Card {
                required property var modelData
                x: Ui.gap
                width: ListView.view.width - 2 * Ui.gap
                ColumnLayout {
                    width: parent.width
                    spacing: 4
                    RowLayout {
                        Label { text: modelData.term; font.pixelSize: 17; font.weight: Font.DemiBold; Layout.fillWidth: true; wrapMode: Label.WordWrap }
                        Label { text: modelData.category; font.pixelSize: 12; opacity: 0.6 }
                    }
                    Label { Layout.fillWidth: true; text: modelData.text; wrapMode: Label.WordWrap }
                    Label {
                        visible: modelData.seeAlso.length > 0
                        Layout.fillWidth: true
                        text: qsTr("Siehe auch: %1").arg(modelData.seeAlso)
                        font.pixelSize: 13
                        opacity: 0.7
                        wrapMode: Label.WordWrap
                    }
                }
            }
            EmptyState {
                anchors.centerIn: parent
                width: parent.width - 48
                visible: page.hits.length === 0
                iconName: "glossary"
                title: qsTr("Kein Eintrag gefunden")
            }
        }
    }
}
