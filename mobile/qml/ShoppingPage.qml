// Einkaufsliste, nach Abteilungen sortiert
import QtQuick
import QtQuick.Controls
import QtQuick.Controls.Material
import QtQuick.Layouts

Page {
    id: page
    ListModel { id: items }

    function reload() {
        const y = list.contentY
        items.clear()
        const all = app.shoppingItems()
        for (let i = 0; i < all.length; ++i) items.append(all[i])
        list.contentY = y
    }
    Component.onCompleted: reload()
    Connections {
        target: app
        function onShoppingChanged() { page.reload() }
    }
    StackView.onActivated: reload()

    header: PageHeader {
        title: app.shoppingOpen > 0 ? qsTr("Einkauf (%1)").arg(app.shoppingOpen) : qsTr("Einkauf")
        HeaderButton {
            iconName: "fridge"
            tip: qsTr("Kühlschrank")
            onClicked: Ui.main.push("FridgePage.qml")
        }
        HeaderButton {
            iconName: "share"
            tip: qsTr("Liste teilen")
            enabled: items.count > 0
            onClicked: app.share(qsTr("Einkaufsliste"), app.shoppingText())
        }
        HeaderButton {
            iconName: "more"
            tip: qsTr("Weitere")
            onClicked: menu.open()
            Menu {
                id: menu
                y: parent.height
                MenuItem {
                    text: qsTr("Aus dem Wochenplan übernehmen")
                    enabled: app.profile().hasPlan === true
                    onTriggered: Ui.main.toast(qsTr("%1 Zutaten aus dem Wochenplan übernommen.").arg(app.planToShopping()))
                }
                MenuItem {
                    text: qsTr("Erledigte entfernen")
                    onTriggered: app.removeCheckedShopping()
                }
                MenuItem {
                    text: qsTr("Ganze Liste löschen …")
                    onTriggered: Ui.main.ask(qsTr("Einkaufsliste löschen"), qsTr("Alle Einträge werden entfernt."),
                                             qsTr("Löschen"), function () { app.clearShopping() })
                }
            }
        }
    }

    ColumnLayout {
        anchors.fill: parent
        spacing: 0

        RowLayout {
            Layout.fillWidth: true
            Layout.margins: Ui.gap
            Layout.maximumWidth: 760
            Layout.alignment: Qt.AlignHCenter
            TextField {
                id: addField
                Layout.minimumWidth: 80
                Layout.fillWidth: true
                placeholderText: qsTr("Hinzufügen, z. B. „2 Liter Milch“")
                EnterKey.type: Qt.EnterKeyDone
                onAccepted: add()
                function add() {
                    if (text.trim().length === 0) return
                    app.addShoppingText(text)
                    text = ""
                }
            }
            RoundButton {
                icon.source: Ui.icon("plus")
                highlighted: true
                onClicked: addField.add()
                Accessible.name: qsTr("Hinzufügen")
            }
        }

        ListView {
            id: list
            Layout.fillWidth: true
            Layout.fillHeight: true
            clip: true
            model: items
            ScrollIndicator.vertical: ScrollIndicator {}
            section.property: "category"
            section.delegate: Label {
                required property string section
                width: ListView.view.width
                leftPadding: 16
                topPadding: 10
                bottomPadding: 2
                text: section.length ? section : qsTr("Sonstiges")
                font.weight: Font.Bold
                color: Ui.blue
            }
            delegate: ItemDelegate {
                id: row
                required property int index
                required property var model
                readonly property string itemText: model.text
                readonly property string recipes: model.recipes
                readonly property bool done: model.checked
                width: ListView.view.width
                onClicked: app.setShoppingChecked(index, !done)
                contentItem: RowLayout {
                    spacing: 8
                    CheckBox {
                        checked: row.done
                        onToggled: app.setShoppingChecked(row.index, checked)
                    }
                    ColumnLayout {
                        Layout.fillWidth: true
                        spacing: 0
                        Label {
                            Layout.fillWidth: true
                            text: row.itemText
                            font.pixelSize: 16
                            font.strikeout: row.done
                            opacity: row.done ? 0.5 : 1
                            wrapMode: Label.WordWrap
                        }
                        Label {
                            Layout.fillWidth: true
                            visible: row.recipes.length > 0
                            text: row.recipes
                            font.pixelSize: 12
                            opacity: 0.6
                            elide: Label.ElideRight
                        }
                    }
                    ToolButton {
                        icon.source: Ui.icon("delete")
                        opacity: 0.6
                        onClicked: app.removeShoppingItem(row.index)
                        Accessible.name: qsTr("Entfernen")
                    }
                }
            }
            EmptyState {
                anchors.centerIn: parent
                width: parent.width - 48
                visible: items.count === 0
                iconName: "cart"
                title: qsTr("Die Einkaufsliste ist leer")
                text: qsTr("Tippen Sie in einem Rezept auf „Einkaufsliste“ oder tragen Sie oben etwas ein. Im Kühlschrank finden Sie Rezepte zu dem, was schon da ist.")
            }
        }
    }
}
