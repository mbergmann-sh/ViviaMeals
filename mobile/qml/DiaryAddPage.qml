// Eintrag ins Tagebuch: Rezept (mit Portionen) oder freier Eintrag mit Kalorien
import QtQuick
import QtQuick.Controls
import QtQuick.Controls.Material
import QtQuick.Layouts

ScrollPage {
    id: page
    property string date: Ui.todayIso()
    property string recipeId
    property string recipeTitle
    property bool free: false
    property real portions: 1
    property string savedQuery
    property string savedTheme

    Component.onCompleted: {
        const meals = app.mealNames()
        mealBox.model = meals
        mealBox.currentIndex = Math.max(0, meals.indexOf(app.mealForNow()))
        savedQuery = app.recipes.query
        savedTheme = app.recipes.theme
        if (!recipeId && !free) {
            app.recipes.theme = ""
            app.recipes.query = ""
        }
    }
    Component.onDestruction: {
        app.recipes.query = savedQuery
        app.recipes.theme = savedTheme
    }

    function save() {
        let ok
        if (free) ok = app.addDiaryFree(freeTitle.text, parseFloat(freeKcal.text.replace(",", ".")) || 0, mealBox.currentText, date)
        else ok = app.addDiaryRecipe(recipeId, portions, mealBox.currentText, date)
        if (!ok) { Ui.main.toast(qsTr("Bitte die Angaben prüfen.")); return }
        Ui.main.toast(qsTr("Im Tagebuch eingetragen."))
        Ui.main.back()
    }

    header: PageHeader {
        title: qsTr("Ins Tagebuch")
        HeaderButton {
            iconName: "check"
            tip: qsTr("Eintragen")
            enabled: page.free ? freeTitle.text.trim().length > 0 : page.recipeId.length > 0
            onClicked: page.save()
        }
    }

    // Auswahl, wenn noch kein Rezept feststeht
    ColumnLayout {
        visible: !page.free && !page.recipeId
        Layout.fillWidth: true
        spacing: 6
        TextField {
            id: search
            Layout.fillWidth: true
            placeholderText: qsTr("Rezept suchen")
            onTextChanged: app.recipes.query = text
        }
        ListView {
            id: pick
            Layout.fillWidth: true
            Layout.preferredHeight: Math.min(count, 30) * 52
            interactive: false
            model: page.recipeId || page.free ? null : app.recipes
            delegate: ItemDelegate {
                required property string recipeId
                required property string title
                required property string subtitle
                width: ListView.view.width
                height: 52
                text: title
                onClicked: { page.recipeTitle = title; page.recipeId = recipeId }
            }
        }
    }

    ColumnLayout {
        visible: page.free || page.recipeId.length > 0
        Layout.fillWidth: true
        spacing: Ui.gap

        Label {
            visible: !page.free
            Layout.fillWidth: true
            text: page.recipeTitle
            font.pixelSize: 20
            font.weight: Font.DemiBold
            wrapMode: Label.WordWrap
        }
        TextField {
            id: freeTitle
            visible: page.free
            Layout.fillWidth: true
            placeholderText: qsTr("Was? (z. B. Apfel, Brötchen mit Käse)")
            EnterKey.type: Qt.EnterKeyNext
            Keys.onReturnPressed: freeKcal.forceActiveFocus()
        }
        TextField {
            id: freeKcal
            visible: page.free
            Layout.fillWidth: true
            placeholderText: qsTr("Kalorien (kcal)")
            inputMethodHints: Qt.ImhFormattedNumbersOnly
            validator: RegularExpressionValidator { regularExpression: /^\d{0,4}([.,]\d?)?$/ }
            EnterKey.type: Qt.EnterKeyDone
            Keys.onReturnPressed: Qt.inputMethod.hide()
        }
        RowLayout {
            visible: !page.free
            Label { text: qsTr("Portionen:") }
            RoundButton { icon.source: Ui.icon("minus"); flat: true; onClicked: page.portions = Math.max(0.25, page.portions - 0.25) }
            Label { text: Ui.num(page.portions, page.portions % 1 === 0 ? 0 : 2); font.pixelSize: 18; font.weight: Font.Medium }
            RoundButton { icon.source: Ui.icon("plus"); flat: true; onClicked: page.portions = Math.min(20, page.portions + 0.25) }
        }
        RowLayout {
            Label { text: qsTr("Mahlzeit:") }
            ComboBox { id: mealBox; Layout.fillWidth: true }
        }
        Label {
            text: page.date === Ui.todayIso() ? qsTr("Für heute") : qsTr("Für den %1").arg(Qt.formatDate(new Date(page.date + "T12:00:00"), "d. M. yyyy"))
            opacity: 0.7
        }
        Button {
            Layout.alignment: Qt.AlignRight
            text: qsTr("Eintragen")
            highlighted: true
            enabled: page.free ? freeTitle.text.trim().length > 0 : page.recipeId.length > 0
            onClicked: page.save()
        }
    }
}
