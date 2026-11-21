// Abgleich mit ViviaMeals am Computer – über die eigene Website oder im Heimnetz
import QtQuick
import QtQuick.Controls
import QtQuick.Controls.Material
import QtQuick.Layouts

ScrollPage {
    id: page
    property var cfg: app.syncConfig()
    property var found: []
    property bool searching: false
    property var pairWith: null
    Connections {
        target: app
        function onSyncConfigChanged() { page.cfg = app.syncConfig() }
        function onLanFound(devices) { page.found = devices; page.searching = false }
        function onLanPaired(ok, text) { if (ok) Ui.main.toast(text); else Ui.main.inform(qsTr("Heimnetz"), text) }
    }

    header: PageHeader { title: qsTr("Abgleich") }

    Label {
        Layout.fillWidth: true
        wrapMode: Label.WordWrap
        text: qsTr("Der Abgleich bringt Ihre Rezepte, Personen, Wochenpläne und Tagebücher vom Computer hierher – und Änderungen vom Telefon zurück. Die Daten gehen nur zu Ihrem eigenen Computer oder Ihrer eigenen Website.")
    }

    // ------------------------------------------------ eingerichtet
    Card {
        visible: app.syncConfigured
        Layout.fillWidth: true
        ColumnLayout {
            width: parent.width
            spacing: 8
            RowLayout {
                Icon { iconName: page.cfg.kind === "website" ? "web" : "network"; color: Ui.blue }
                Label {
                    Layout.fillWidth: true
                    text: page.cfg.kind === "website" ? qsTr("Über die Website") : qsTr("Im Heimnetz mit „%1“").arg(page.cfg.device || page.cfg.url)
                    font.weight: Font.DemiBold
                    wrapMode: Label.WordWrap
                }
            }
            Label { Layout.fillWidth: true; text: page.cfg.url; font.pixelSize: 13; opacity: 0.7; elide: Label.ElideMiddle }
            Label { Layout.fillWidth: true; text: app.syncStatus; opacity: 0.8 }
            Switch {
                Layout.fillWidth: true
                text: qsTr("Automatisch beim Öffnen der App")
                checked: page.cfg.auto === true
                onToggled: app.setSyncAuto(checked)
            }
            CheckBox {
                id: dropSamples
                visible: app.sampleCount() > 0
                Layout.fillWidth: true
                checked: true
                text: qsTr("Mitgelieferte Beispielrezepte vorher entfernen (Favoriten bleiben)")
            }
            RowLayout {
                Layout.fillWidth: true
                Button {
                    text: qsTr("Jetzt abgleichen")
                    highlighted: true
                    icon.source: Ui.icon("sync")
                    onClicked: app.syncNow(dropSamples.visible && dropSamples.checked)
                }
                Item { Layout.fillWidth: true }
                Button {
                    text: qsTr("Trennen")
                    flat: true
                    onClicked: Ui.main.ask(qsTr("Abgleich trennen"),
                                           qsTr("Die Verbindung wird entfernt. Ihre Rezepte bleiben auf diesem Gerät."),
                                           qsTr("Trennen"), function () { app.removeSync() })
                }
            }
        }
    }

    // ------------------------------------------------ einrichten
    SectionLabel { visible: !app.syncConfigured; text: qsTr("1. Einfach: Einladungscode") }
    Card {
        visible: !app.syncConfigured
        Layout.fillWidth: true
        ColumnLayout {
            width: parent.width
            Label {
                Layout.fillWidth: true
                wrapMode: Label.WordWrap
                text: qsTr("Am Computer in ViviaMeals: Datei → Abgleich → Abgleich einrichten … → „Einladungscode kopieren“. Schicken Sie sich den Code (z. B. per E-Mail an sich selbst) und fügen Sie ihn hier ein.")
            }
            TextField {
                id: codeField
                Layout.fillWidth: true
                placeholderText: "vmsync1:…"
                inputMethodHints: Qt.ImhNoAutoUppercase | Qt.ImhNoPredictiveText
            }
            Button {
                text: qsTr("Übernehmen")
                enabled: codeField.text.trim().length > 0
                onClicked: {
                    const err = app.setSyncInvitation(codeField.text)
                    if (err) Ui.main.inform(qsTr("Einladungscode"), err)
                    else { codeField.text = ""; app.syncNow(app.sampleCount() > 0) }
                }
            }
        }
    }

    SectionLabel { visible: !app.syncConfigured; text: qsTr("2. Im Heimnetz") }
    Card {
        visible: !app.syncConfigured
        Layout.fillWidth: true
        ColumnLayout {
            width: parent.width
            Label {
                Layout.fillWidth: true
                wrapMode: Label.WordWrap
                text: qsTr("Computer und Telefon im selben WLAN: Am Computer unter Datei → Abgleich → Abgleich einrichten … die Datenbank im lokalen Netz freigeben. Dann hier suchen und den sechsstelligen Kopplungscode eingeben, den der Computer anzeigt.")
            }
            RowLayout {
                Button {
                    text: page.searching ? qsTr("Suche läuft …") : qsTr("Computer suchen")
                    enabled: !page.searching
                    icon.source: Ui.icon("search")
                    onClicked: { page.searching = true; page.found = []; app.discoverLan() }
                }
                BusyIndicator { running: page.searching; visible: running; implicitWidth: 32; implicitHeight: 32 }
            }
            Repeater {
                model: page.found
                delegate: ItemDelegate {
                    required property var modelData
                    Layout.fillWidth: true
                    text: modelData.name + (modelData.device ? " – " + modelData.device : "")
                    icon.source: Ui.icon("computer")
                    onClicked: { page.pairWith = modelData; pairCode.text = ""; pairDialog.open() }
                }
            }
            Label {
                visible: !page.searching && page.found.length === 0 && searchedOnce.value
                Layout.fillWidth: true
                wrapMode: Label.WordWrap
                opacity: 0.75
                text: qsTr("Kein Computer gefunden. Läuft ViviaMeals dort, und ist die Datenbank im lokalen Netz freigegeben?")
            }
            QtObject { id: searchedOnce; property bool value: false }
            Connections { target: page; function onSearchingChanged() { if (!page.searching) searchedOnce.value = true } }
        }
    }

    SectionLabel { visible: !app.syncConfigured; text: qsTr("3. Über Ihre Website") }
    Card {
        visible: !app.syncConfigured
        Layout.fillWidth: true
        ColumnLayout {
            width: parent.width
            Label {
                Layout.fillWidth: true
                wrapMode: Label.WordWrap
                text: qsTr("Wenn Sie den Abgleich am Computer über Ihre Website eingerichtet haben: Adresse und Zugangsschlüssel stehen dort unter Datei → Abgleich → Abgleich einrichten …")
            }
            TextField {
                id: urlField
                Layout.fillWidth: true
                placeholderText: qsTr("Adresse, z. B. https://example.org/rezepte/")
                inputMethodHints: Qt.ImhUrlCharactersOnly | Qt.ImhNoAutoUppercase | Qt.ImhNoPredictiveText
                EnterKey.type: Qt.EnterKeyNext
                Keys.onReturnPressed: keyField.forceActiveFocus()
            }
            TextField {
                id: keyField
                Layout.fillWidth: true
                placeholderText: qsTr("Zugangsschlüssel")
                inputMethodHints: Qt.ImhNoAutoUppercase | Qt.ImhNoPredictiveText | Qt.ImhSensitiveData
                EnterKey.type: Qt.EnterKeyDone
                Keys.onReturnPressed: Qt.inputMethod.hide()
            }
            Button {
                text: qsTr("Übernehmen")
                enabled: urlField.text.trim().length > 0 && keyField.text.trim().length > 0
                onClicked: {
                    const err = app.setSyncWebsite(urlField.text, keyField.text)
                    if (err) Ui.main.inform(qsTr("Website"), err)
                    else app.syncNow(app.sampleCount() > 0)
                }
            }
        }
    }

    Label {
        Layout.fillWidth: true
        wrapMode: Label.WordWrap
        font.pixelSize: 13
        opacity: 0.7
        text: qsTr("Beim ersten Abgleich werden die mitgelieferten Beispielrezepte entfernt, damit sie nicht in Ihre Sammlung am Computer wandern. Rezepte, die Sie als Favorit markiert haben, bleiben.")
        visible: !app.syncConfigured && app.sampleCount() > 0
    }

    Dialog {
        id: pairDialog
        title: qsTr("Koppeln")
        modal: true
        anchors.centerIn: Overlay.overlay
        width: Math.min(page.width - 32, 420)
        standardButtons: Dialog.Ok | Dialog.Cancel
        ColumnLayout {
            width: parent.width
            Label {
                Layout.fillWidth: true
                wrapMode: Label.WordWrap
                text: qsTr("Geben Sie den Kopplungscode ein, den ViviaMeals am Computer „%1“ anzeigt.").arg(page.pairWith ? page.pairWith.name : "")
            }
            TextField {
                id: pairCode
                Layout.fillWidth: true
                inputMethodHints: Qt.ImhDigitsOnly
                placeholderText: qsTr("Kopplungscode")
            }
        }
        onAccepted: app.pairLan(page.pairWith.url, pairCode.text, page.pairWith.name)
    }
}
