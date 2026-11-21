// Rezept anlegen oder bearbeiten – mit laufender Nährwertberechnung
import QtQuick
import QtQuick.Controls
import QtQuick.Controls.Material
import QtQuick.Layouts
import QtQuick.Dialogs

ScrollPage {
    id: page
    property string recipeId
    property var data: ({})
    property url imageUrl
    property bool removeImage: false
    property bool dirty: false
    property var preview: ({})
    readonly property bool isNew: recipeId.length === 0

    Component.onCompleted: {
        data = app.recipeForEdit(recipeId)
        titleField.text = data.title || ""
        servingsField.text = Ui.servingsText(data.servings || 4)
        unitField.text = data.yieldUnit || ""
        minutesField.text = data.prepMinutes > 0 ? data.prepMinutes.toString() : ""
        tagsField.text = data.tags || ""
        ingredientsArea.text = data.ingredients || ""
        instructionsArea.text = data.instructions || ""
        dirty = false
        hintCard.visible = isNew && app.showEditorHint()
        updatePreview()
        if (isNew) titleField.forceActiveFocus()
    }

    function servingsValue() {
        const v = parseFloat(servingsField.text.replace(",", "."))
        return isNaN(v) || v <= 0 ? 1 : v
    }
    function updatePreview() { preview = app.previewNutrition(ingredientsArea.text, servingsValue()) }
    function save() {
        const id = app.saveRecipe({
            id: recipeId, title: titleField.text, servings: servingsValue(), yieldUnit: unitField.text,
            prepMinutes: parseInt(minutesField.text) || 0, tags: tagsField.text,
            ingredients: ingredientsArea.text, instructions: instructionsArea.text,
            imageUrl: imageUrl, removeImage: removeImage
        })
        if (!id) return
        dirty = false
        Ui.main.imageRevision++
        Ui.main.back()
        if (isNew) Ui.main.openRecipe(id)
        Ui.main.toast(qsTr("Rezept gespeichert."))
    }
    function leave() {
        if (!dirty) { Ui.main.back(); return }
        Ui.main.ask(qsTr("Änderungen verwerfen?"), qsTr("Das Rezept wurde geändert, aber noch nicht gespeichert."),
                    qsTr("Verwerfen"), function () { page.dirty = false; Ui.main.back() })
    }

    header: PageHeader {
        title: page.isNew ? qsTr("Neues Rezept") : qsTr("Rezept bearbeiten")
        HeaderButton { iconName: "save"; tip: qsTr("Speichern"); onClicked: page.save() }
    }

    Timer { id: previewDelay; interval: 500; onTriggered: page.updatePreview() }

    // Freundlicher Hinweis auf die große Version (höchstens jedes fünfte Mal)
    Card {
        id: hintCard
        visible: false
        Layout.fillWidth: true
        background: Rectangle {
            radius: Ui.radius
            color: Material.theme === Material.Dark ? "#1E3346" : "#EAF3FB"
            border.color: "#B9D6EE"
        }
        ColumnLayout {
            width: parent.width
            spacing: 6
            RowLayout {
                Icon { iconName: "computer"; size: 22; color: Ui.blue }
                Label {
                    Layout.fillWidth: true
                    text: qsTr("Viele Rezepte? Am Computer geht es schneller.")
                    font.weight: Font.DemiBold
                    wrapMode: Label.WordWrap
                }
            }
            Label {
                Layout.fillWidth: true
                wrapMode: Label.WordWrap
                text: qsTr("Mit ViviaMeals für Windows, Linux und Mac scannen Sie Rezepte aus Kochbüchern, übernehmen ganze Sammlungen und holen Rezepte von Webseiten. Per Abgleich landen sie automatisch hier.")
            }
            RowLayout {
                Button { text: qsTr("Mehr erfahren"); flat: true; onClicked: Ui.main.push("DesktopPage.qml") }
                Item { Layout.fillWidth: true }
                Button {
                    text: qsTr("Nicht mehr anzeigen")
                    flat: true
                    onClicked: { app.disableEditorHint(); hintCard.visible = false }
                }
            }
        }
    }

    TextField {
        id: titleField
        Layout.fillWidth: true
        placeholderText: qsTr("Titel des Rezepts")
        font.pixelSize: 18
        onTextEdited: page.dirty = true
        EnterKey.type: Qt.EnterKeyNext
        Keys.onReturnPressed: servingsField.forceActiveFocus()
    }

    RowLayout {
        Layout.fillWidth: true
        spacing: Ui.gap
        TextField {
            id: servingsField
            Layout.preferredWidth: 90
            placeholderText: qsTr("Menge")
            inputMethodHints: Qt.ImhFormattedNumbersOnly
            validator: RegularExpressionValidator { regularExpression: /^\d{0,3}([.,]\d?)?$/ }
            onTextEdited: { page.dirty = true; previewDelay.restart() }
            EnterKey.type: Qt.EnterKeyNext
            Keys.onReturnPressed: unitField.forceActiveFocus()
        }
        TextField {
            id: unitField
            Layout.fillWidth: true
            placeholderText: qsTr("Portionen (oder z. B. Stücke)")
            onTextEdited: page.dirty = true
            EnterKey.type: Qt.EnterKeyNext
            Keys.onReturnPressed: minutesField.forceActiveFocus()
        }
        TextField {
            id: minutesField
            Layout.preferredWidth: 110
            placeholderText: qsTr("Minuten")
            inputMethodHints: Qt.ImhDigitsOnly
            validator: IntValidator { bottom: 0; top: 2000 }
            onTextEdited: page.dirty = true
            EnterKey.type: Qt.EnterKeyNext
            Keys.onReturnPressed: tagsField.forceActiveFocus()
        }
    }

    TextField {
        id: tagsField
        Layout.fillWidth: true
        placeholderText: qsTr("Themen, durch Komma getrennt (z. B. vegetarisch, schnell)")
        onTextEdited: page.dirty = true
        EnterKey.type: Qt.EnterKeyNext
        Keys.onReturnPressed: ingredientsArea.forceActiveFocus()
    }

    // Foto
    RowLayout {
        Layout.fillWidth: true
        spacing: Ui.gap
        RecipeThumb {
            Layout.preferredWidth: 72
            Layout.preferredHeight: 72
            recipeId: page.recipeId
            hasImage: page.data.hasImage === true && !page.removeImage && page.imageUrl.toString() === ""
            title: titleField.text
            tags: tagsField.text.split(",")
            Image {
                anchors.fill: parent
                visible: page.imageUrl.toString() !== ""
                source: page.imageUrl
                sourceSize: Qt.size(144, 144)
                fillMode: Image.PreserveAspectCrop
            }
        }
        Button {
            text: qsTr("Foto wählen")
            icon.source: Ui.icon("image-open")
            onClicked: imageDialog.open()
        }
        Button {
            visible: (page.data.hasImage && !page.removeImage) || page.imageUrl.toString() !== ""
            text: qsTr("Entfernen")
            flat: true
            onClicked: { page.removeImage = true; page.imageUrl = ""; page.dirty = true }
        }
        Item { Layout.fillWidth: true }
    }
    FileDialog {
        id: imageDialog
        title: qsTr("Foto wählen")
        nameFilters: [qsTr("Bilder (*.jpg *.jpeg *.png *.webp)")]
        onAccepted: { page.imageUrl = selectedFile; page.removeImage = false; page.dirty = true }
    }

    SectionLabel { text: qsTr("Zutaten – eine je Zeile, z. B. „200 g Mehl“") }
    TextArea {
        id: ingredientsArea
        Layout.fillWidth: true
        Layout.minimumHeight: 180
        wrapMode: TextArea.Wrap
        placeholderText: qsTr("500 g Kartoffeln\n1 Zwiebel\n2 EL Butter\n\nÜberschriften enden mit Doppelpunkt, z. B. „Für die Soße:“")
        onTextChanged: { page.dirty = true; previewDelay.restart() }
    }

    NutritionView {
        Layout.fillWidth: true
        nut: page.preview
        live: true
        perLabel: unitField.text.length ? unitField.text : qsTr("Portion")
    }

    SectionLabel { text: qsTr("Zubereitung") }
    TextArea {
        id: instructionsArea
        Layout.fillWidth: true
        Layout.minimumHeight: 220
        wrapMode: TextArea.Wrap
        placeholderText: qsTr("1. Kartoffeln schälen …\n2. …")
        onTextChanged: page.dirty = true
    }
    Label {
        Layout.fillWidth: true
        wrapMode: Label.WordWrap
        font.pixelSize: 13
        opacity: 0.7
        text: qsTr("Tipp: **fett** und *kursiv* werden in der Ansicht hervorgehoben. Themen wie „vegan“, „Saison“ oder „Säure-Basen-Fasten“ ergänzt ViviaMeals beim Speichern automatisch.")
    }

    RowLayout {
        Layout.fillWidth: true
        Button { text: qsTr("Abbrechen"); flat: true; onClicked: page.leave() }
        Item { Layout.fillWidth: true }
        Button {
            text: qsTr("Speichern")
            highlighted: true
            icon.source: Ui.icon("save")
            onClicked: page.save()
        }
    }

    // Zurück-Geste mit ungespeicherten Änderungen
    property bool guardBack: dirty
}
