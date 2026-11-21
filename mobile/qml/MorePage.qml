// Mehr: weitere Funktionen und Einstellungen
import QtQuick
import QtQuick.Controls
import QtQuick.Controls.Material
import QtQuick.Layouts
import QtQuick.Dialogs

Page {
    id: page
    header: PageHeader { title: qsTr("Mehr") }

    readonly property var entries: [
        { icon: "plan", text: qsTr("Wochenplan"), sub: qsTr("vom Computer, mit Einkaufsliste"), page: "PlanPage.qml" },
        { icon: "fridge", text: qsTr("Kühlschrank"), sub: qsTr("Was kann ich mit meinen Vorräten kochen?"), page: "FridgePage.qml" },
        { icon: "glossary", text: qsTr("Küchen-Glossar"), sub: qsTr("Fachbegriffe kurz erklärt"), page: "GlossaryPage.qml" },
        { icon: "import", text: qsTr("Rezepte importieren"), sub: qsTr("Rezeptpaket (.vmpack) oder Rezept (.vmr)"), action: "import" },
        { icon: "sync", text: qsTr("Abgleich mit dem Computer"), sub: app.syncStatus, page: "SyncPage.qml" },
        { icon: "computer", text: qsTr("ViviaMeals für Ihren Computer"), sub: qsTr("Rezepte bequem erfassen, scannen und übernehmen"), page: "DesktopPage.qml", accent: true },
        { icon: "settings", text: qsTr("Einstellungen"), sub: qsTr("Erscheinungsbild, Bildschirm beim Kochen"), page: "SettingsPage.qml" },
        { icon: "info", text: qsTr("Über ViviaMeals"), sub: qsTr("Version %1, Lizenzen").arg(app.version), page: "AboutPage.qml" }
    ]

    ListView {
        anchors.fill: parent
        model: page.entries
        ScrollIndicator.vertical: ScrollIndicator {}
        delegate: ItemDelegate {
            required property var modelData
            width: ListView.view.width
            height: 68
            onClicked: modelData.action === "import" ? importDialog.open() : Ui.main.push(modelData.page)
            contentItem: RowLayout {
                spacing: 16
                Icon {
                    iconName: modelData.icon
                    size: 26
                    color: modelData.accent ? Ui.orange : Material.foreground
                    opacity: modelData.accent ? 1 : 0.75
                }
                ColumnLayout {
                    Layout.fillWidth: true
                    spacing: 1
                    Label {
                        text: modelData.text
                        font.pixelSize: 16
                        font.weight: modelData.accent ? Font.DemiBold : Font.Normal
                        color: modelData.accent ? Ui.orange : Material.foreground
                    }
                    Label { Layout.fillWidth: true; text: modelData.sub; font.pixelSize: 13; opacity: 0.65; elide: Label.ElideRight }
                }
                Icon { iconName: "chevron"; size: 18; opacity: 0.4 }
            }
        }
    }

    FileDialog {
        id: importDialog
        title: qsTr("Rezepte importieren")
        nameFilters: [qsTr("ViviaMeals-Rezepte (*.vmpack *.vmr)"), qsTr("Alle Dateien (*)")]
        onAccepted: app.importFile(selectedFile)
    }
}
