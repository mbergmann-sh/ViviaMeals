// Begrüßung beim ersten Start (einmalig)
import QtQuick
import QtQuick.Controls
import QtQuick.Controls.Material
import QtQuick.Layouts

Page {
    id: page
    property bool guardBack: true
    readonly property bool fullScreen: true
    function leave() { done() }
    function done() {
        app.finishFirstRun()
        guardBack = false
        Ui.main.back()
    }

    SwipeView {
        id: swipe
        anchors.fill: parent
        anchors.bottomMargin: bottomBar.height

        ScrollPage {
            ColumnLayout {
                Layout.fillWidth: true
                spacing: 16
                Image {
                    Layout.alignment: Qt.AlignHCenter
                    Layout.topMargin: 24
                    source: "qrc:/images/viviameals.svg"
                    sourceSize: Qt.size(120, 120)
                }
                Label {
                    Layout.fillWidth: true
                    horizontalAlignment: Text.AlignHCenter
                    text: qsTr("Willkommen bei ViviaMeals")
                    font.pixelSize: 26
                    font.weight: Font.Bold
                    color: Ui.blue
                    wrapMode: Label.WordWrap
                }
                Label {
                    Layout.fillWidth: true
                    horizontalAlignment: Text.AlignHCenter
                    wrapMode: Label.WordWrap
                    font.pixelSize: 16
                    text: qsTr("Ihre Rezepte mit Kalorien und Nährwerten – zum Kochen, Einkaufen und für Ihr Ernährungstagebuch.")
                }
                Card {
                    Layout.fillWidth: true
                    ColumnLayout {
                        width: parent.width
                        spacing: 8
                        Repeater {
                            model: [qsTr("%1 Beispielrezepte sind schon dabei.").arg(app.recipes.total),
                                    qsTr("Portionen umrechnen, Fehlendes auf die Einkaufsliste setzen."),
                                    qsTr("Der Kühlschrank schlägt Rezepte zu Ihren Vorräten vor."),
                                    qsTr("Kein Konto, keine Cloud, keine Werbung.")]
                            delegate: Label {
                                required property string modelData
                                Layout.fillWidth: true
                                text: "•  " + modelData
                                wrapMode: Label.WordWrap
                            }
                        }
                    }
                }
            }
        }

        ScrollPage {
            ColumnLayout {
                Layout.fillWidth: true
                spacing: 14
                Icon {
                    Layout.alignment: Qt.AlignHCenter
                    Layout.topMargin: 24
                    iconName: "computer"
                    size: 72
                    color: Ui.blue
                }
                Label {
                    Layout.fillWidth: true
                    horizontalAlignment: Text.AlignHCenter
                    text: qsTr("Rezepte bequem erfassen – mit ViviaMeals für Windows, Linux und Mac")
                    font.pixelSize: 20
                    font.weight: Font.DemiBold
                    wrapMode: Label.WordWrap
                }
                Label {
                    Layout.fillWidth: true
                    wrapMode: Label.WordWrap
                    text: qsTr("Hier in der App haben Sie Ihre Rezepte immer dabei. Am Computer wird das Erfassen richtig bequem:")
                }
                DesktopFeatures { Layout.fillWidth: true }
                Label {
                    Layout.fillWidth: true
                    wrapMode: Label.WordWrap
                    text: qsTr("Alles, was Sie dort erfassen, kommt per Abgleich automatisch hierher aufs Telefon.")
                }
                RowLayout {
                    Layout.alignment: Qt.AlignHCenter
                    Button {
                        text: qsTr("Mehr erfahren")
                        flat: true
                        onClicked: Qt.openUrlExternally(app.productUrl)
                    }
                    Button { text: qsTr("Später"); flat: true; onClicked: page.done() }
                }
            }
        }
    }

    Rectangle {
        id: bottomBar
        anchors { left: parent.left; right: parent.right; bottom: parent.bottom }
        height: 64
        color: "transparent"
        PageIndicator {
            anchors.left: parent.left
            anchors.leftMargin: 16
            anchors.verticalCenter: parent.verticalCenter
            count: swipe.count
            currentIndex: swipe.currentIndex
        }
        Button {
            anchors.right: parent.right
            anchors.rightMargin: 16
            anchors.verticalCenter: parent.verticalCenter
            highlighted: true
            text: swipe.currentIndex < swipe.count - 1 ? qsTr("Weiter") : qsTr("Los geht’s")
            onClicked: swipe.currentIndex < swipe.count - 1 ? swipe.incrementCurrentIndex() : page.done()
        }
    }
}
