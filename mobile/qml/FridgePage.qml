// Kühlschrank: was ist da? Daraus Rezeptvorschläge, fehlende Zutaten auf die Einkaufsliste
import QtQuick
import QtQuick.Controls
import QtQuick.Controls.Material
import QtQuick.Layouts

ScrollPage {
    id: page
    property var stock: []
    property var suggestions: []
    property int maxMissing: 2

    function reload() {
        stock = app.stock()
        suggestions = app.fridgeSuggestions(maxMissing)
    }
    Component.onCompleted: reload()
    onMaxMissingChanged: suggestions = app.fridgeSuggestions(maxMissing)

    header: PageHeader {
        title: qsTr("Kühlschrank")
        HeaderButton {
            iconName: "delete"
            tip: qsTr("Alles leeren")
            enabled: page.stock.length > 0
            onClicked: Ui.main.ask(qsTr("Kühlschrank leeren"), qsTr("Alle Einträge werden entfernt."), qsTr("Leeren"),
                                   function () { app.setStock([]); page.reload() })
        }
    }

    Label {
        Layout.fillWidth: true
        wrapMode: Label.WordWrap
        text: qsTr("Tragen Sie ein, was Sie im Haus haben. ViviaMeals sucht passende Rezepte.")
    }
    RowLayout {
        Layout.fillWidth: true
        TextField {
            id: addField
            Layout.minimumWidth: 80
            Layout.fillWidth: true
            placeholderText: qsTr("z. B. Kartoffeln, Lauch, Eier")
            EnterKey.type: Qt.EnterKeyDone
            onAccepted: add()
            function add() {
                const parts = text.split(",")
                for (let i = 0; i < parts.length; ++i)
                    if (parts[i].trim().length) app.addStock(parts[i].trim())
                text = ""
                page.reload()
            }
        }
        RoundButton {
            icon.source: Ui.icon("plus")
            highlighted: true
            onClicked: addField.add()
            Accessible.name: qsTr("Hinzufügen")
        }
    }
    Flow {
        Layout.fillWidth: true
        spacing: 6
        Repeater {
            model: page.stock
            delegate: Chip {
                required property string modelData
                text: modelData
                closable: true
                onCloseClicked: { app.removeStock(modelData); page.reload() }
            }
        }
    }
    Label {
        Layout.fillWidth: true
        wrapMode: Label.WordWrap
        font.pixelSize: 13
        opacity: 0.7
        text: qsTr("Immer im Haus angenommen: %1").arg(app.basics().join(", "))
    }

    SectionLabel { text: qsTr("Rezeptvorschläge") }
    Flow {
        Layout.fillWidth: true
        spacing: 8
        Label { text: qsTr("Höchstens fehlen dürfen:"); height: spin.height; verticalAlignment: Text.AlignVCenter }
        SpinBox {
            id: spin
            from: 0; to: 5
            value: page.maxMissing
            onValueModified: page.maxMissing = value
        }
        Label { text: page.maxMissing === 1 ? qsTr("Zutat") : qsTr("Zutaten"); height: spin.height; verticalAlignment: Text.AlignVCenter }
    }

    Repeater {
        model: page.suggestions
        delegate: Card {
            required property var modelData
            Layout.fillWidth: true
            ColumnLayout {
                width: parent.width
                spacing: 4
                Label {
                    Layout.fillWidth: true
                    text: modelData.title
                    font.pixelSize: 16
                    font.weight: Font.Medium
                    wrapMode: Label.WordWrap
                }
                Label {
                    Layout.fillWidth: true
                    wrapMode: Label.WordWrap
                    opacity: 0.75
                    text: modelData.missing.length
                          ? qsTr("%1 von %2 Zutaten da · es fehlt: %3").arg(modelData.have).arg(modelData.total).arg(modelData.missing)
                          : qsTr("Alle %1 Zutaten da").arg(modelData.total)
                    color: modelData.missing.length ? Material.foreground : Ui.green
                }
                RowLayout {
                    Button { text: qsTr("Ansehen"); flat: true; onClicked: Ui.main.openRecipe(modelData.id) }
                    Button {
                        visible: modelData.missing.length > 0
                        text: qsTr("Fehlendes einkaufen")
                        flat: true
                        icon.source: Ui.icon("cart")
                        onClicked: {
                            const n = app.addRecipeToShopping(modelData.id, 0, true)
                            Ui.main.toast(qsTr("%1 Zutaten auf der Einkaufsliste.").arg(n))
                        }
                    }
                }
            }
        }
    }
    EmptyState {
        Layout.fillWidth: true
        Layout.topMargin: 24
        visible: page.suggestions.length === 0
        iconName: "fridge"
        title: page.stock.length ? qsTr("Keine passenden Rezepte") : qsTr("Der Kühlschrank ist leer")
        text: page.stock.length ? qsTr("Erlauben Sie mehr fehlende Zutaten oder tragen Sie weitere Vorräte ein.")
                                : qsTr("Tragen Sie oben ein, was Sie im Haus haben.")
    }
}
