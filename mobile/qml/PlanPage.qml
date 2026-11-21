// Wochenplan – wird am Computer erstellt und kommt über den Abgleich
import QtQuick
import QtQuick.Controls
import QtQuick.Controls.Material
import QtQuick.Layouts

ScrollPage {
    id: page
    property var plan: app.plan()
    readonly property bool empty: !plan || !plan.days || plan.days.length === 0
    readonly property bool wide: width >= 900
    maxContentWidth: wide ? 1200 : 760
    Connections {
        target: app
        function onDatabaseChanged() { page.plan = app.plan() }
        function onProfileChanged() { page.plan = app.plan() }
    }

    header: PageHeader {
        title: qsTr("Wochenplan")
        HeaderButton {
            visible: !page.empty
            iconName: "cart"
            tip: qsTr("Einkaufsliste erstellen")
            onClicked: Ui.main.toast(qsTr("%1 Zutaten auf der Einkaufsliste.").arg(app.planToShopping()))
        }
    }

    Label {
        visible: !page.empty
        Layout.fillWidth: true
        wrapMode: Label.WordWrap
        text: page.empty ? "" : qsTr("Plan für %1 ab %2 · Ziel etwa %3 kcal am Tag")
                                    .arg(page.plan.person).arg(page.plan.start).arg(Ui.num(page.plan.target))
        opacity: 0.8
    }

    GridLayout {
        visible: !page.empty
        Layout.fillWidth: true
        columns: page.wide ? 2 : 1
        columnSpacing: Ui.gap
        rowSpacing: Ui.gap
        Repeater {
            model: page.empty ? [] : page.plan.days
            delegate: Card {
                required property var modelData
                Layout.fillWidth: true
                Layout.alignment: Qt.AlignTop
                background: Rectangle {
                    radius: Ui.radius
                    color: Material.theme === Material.Dark ? "#2A2D31" : "white"
                    border.color: modelData.today ? Ui.orange : (Material.theme === Material.Dark ? "#3A3E44" : "#DDE3EA")
                    border.width: modelData.today ? 2 : 1
                }
                ColumnLayout {
                    width: parent.width
                    spacing: 4
                    RowLayout {
                        Layout.fillWidth: true
                        Label {
                            Layout.fillWidth: true
                            text: modelData.label + (modelData.today ? "  · " + qsTr("heute") : "")
                            font.weight: Font.DemiBold
                            color: Ui.blue
                        }
                        Label { text: Ui.num(modelData.kcal) + " kcal"; opacity: 0.75 }
                    }
                    Repeater {
                        model: modelData.meals
                        delegate: ItemDelegate {
                            required property var modelData
                            Layout.fillWidth: true
                            padding: 4
                            enabled: modelData.id.length > 0
                            onClicked: Ui.main.openRecipe(modelData.id)
                            contentItem: RowLayout {
                                Label { text: modelData.meal; Layout.preferredWidth: 110; opacity: 0.7; elide: Label.ElideRight }
                                Label {
                                    Layout.fillWidth: true
                                    text: modelData.title + (modelData.portions ? " (" + modelData.portions + ")" : "")
                                    wrapMode: Label.WordWrap
                                }
                                Label { text: Ui.num(modelData.kcal); opacity: 0.6 }
                            }
                        }
                    }
                }
            }
        }
    }

    EmptyState {
        visible: page.empty
        Layout.fillWidth: true
        Layout.topMargin: 32
        iconName: "plan"
        title: qsTr("Noch kein Wochenplan")
        text: qsTr("Den Wochenplan stellt der Assistent in ViviaMeals am Computer zusammen – passend zu Ihrem Ziel und Ihren Vorlieben. Nach dem Abgleich sehen Sie ihn hier und können die Einkaufsliste daraus erstellen.")
    }
    RowLayout {
        visible: page.empty
        Layout.alignment: Qt.AlignHCenter
        Button { text: qsTr("Abgleich einrichten"); onClicked: Ui.main.push("SyncPage.qml") }
        Button { text: qsTr("ViviaMeals für Ihren Computer"); flat: true; onClicked: Ui.main.push("DesktopPage.qml") }
    }
}
