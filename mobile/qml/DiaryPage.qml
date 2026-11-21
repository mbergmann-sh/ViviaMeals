// Ernährungstagebuch: was wurde heute gegessen, wie viel bleibt bis zum Tagesziel
import QtQuick
import QtQuick.Controls
import QtQuick.Controls.Material
import QtQuick.Layouts

ScrollPage {
    id: page
    property string date: Ui.todayIso()
    property var day: app.diaryDay(date)
    readonly property bool isToday: date === Ui.todayIso()

    function reload() { day = app.diaryDay(date) }
    onDateChanged: reload()
    Connections {
        target: app
        function onDiaryChanged() { page.reload() }
        function onProfileChanged() { page.reload() }
    }
    StackView.onActivated: reload()

    header: PageHeader {
        title: qsTr("Tagebuch")
        HeaderButton { iconName: "chevron-left"; tip: qsTr("Vortag"); onClicked: page.date = Ui.addDays(page.date, -1) }
        HeaderButton {
            iconName: "chevron"
            tip: qsTr("Folgetag")
            enabled: !page.isToday
            opacity: enabled ? 1 : 0.4
            onClicked: page.date = Ui.addDays(page.date, 1)
        }
    }

    Card {
        Layout.fillWidth: true
        ColumnLayout {
            width: parent.width
            spacing: 6
            Label { text: page.day.label; font.pixelSize: 18; font.weight: Font.DemiBold }
            RowLayout {
                Label { text: Ui.num(page.day.kcal); font.pixelSize: 30; font.weight: Font.Bold; color: Ui.orange }
                Label {
                    text: page.day.target > 0 ? qsTr("von %1 kcal").arg(Ui.num(page.day.target)) : "kcal"
                    Layout.alignment: Qt.AlignBaseline
                }
            }
            ProgressBar {
                visible: page.day.target > 0
                Layout.fillWidth: true
                from: 0; to: page.day.target
                value: Math.min(page.day.kcal, page.day.target)
                Material.accent: page.day.kcal > page.day.target * 1.05 ? Ui.red : Ui.green
            }
            Label {
                Layout.fillWidth: true
                wrapMode: Label.WordWrap
                opacity: 0.8
                text: page.day.target <= 0
                      ? qsTr("Ein Tagesziel erscheint, sobald unter „Ich“ Größe, Geburtsjahr und Gewicht eingetragen sind.")
                      : page.day.kcal <= page.day.target
                        ? qsTr("Noch %1 kcal bis zum Tagesziel.").arg(Ui.num(page.day.target - page.day.kcal))
                        : qsTr("%1 kcal über dem Tagesziel.").arg(Ui.num(page.day.kcal - page.day.target))
            }
            Label {
                Layout.fillWidth: true
                wrapMode: Label.WordWrap
                font.pixelSize: 13
                opacity: 0.7
                text: qsTr("Eiweiß %1 g · Fett %2 g · Kohlenhydrate %3 g").arg(page.day.protein).arg(page.day.fat).arg(page.day.carbs)
            }
        }
    }

    RowLayout {
        Layout.fillWidth: true
        Button {
            text: qsTr("Rezept eintragen")
            highlighted: true
            icon.source: Ui.icon("recipe")
            onClicked: Ui.main.push("DiaryAddPage.qml", { date: page.date })
        }
        Button {
            text: qsTr("Freier Eintrag")
            icon.source: Ui.icon("edit")
            onClicked: Ui.main.push("DiaryAddPage.qml", { date: page.date, free: true })
        }
    }

    Repeater {
        model: app.mealNames()
        delegate: ColumnLayout {
            id: mealBlock
            required property string modelData
            readonly property var entries: (page.day.entries || []).filter(e => e.meal === modelData)
            Layout.fillWidth: true
            visible: entries.length > 0
            spacing: 4
            SectionLabel {
                text: mealBlock.modelData + "  ·  " + Ui.num(mealBlock.entries.reduce((s, e) => s + e.kcal, 0)) + " kcal"
            }
            Repeater {
                model: mealBlock.entries
                delegate: ItemDelegate {
                    required property var modelData
                    Layout.fillWidth: true
                    onClicked: if (modelData.recipeId) Ui.main.openRecipe(modelData.recipeId)
                    contentItem: RowLayout {
                        Label { text: modelData.time; opacity: 0.6; Layout.preferredWidth: 48 }
                        ColumnLayout {
                            Layout.fillWidth: true
                            spacing: 0
                            Label { Layout.fillWidth: true; text: modelData.title; wrapMode: Label.WordWrap }
                            Label { text: modelData.amount; font.pixelSize: 12; opacity: 0.6; visible: text.length > 0 }
                        }
                        Label { text: Ui.num(modelData.kcal) + " kcal" }
                        ToolButton {
                            icon.source: Ui.icon("delete")
                            opacity: 0.6
                            onClicked: app.removeDiaryEntry(modelData.id)
                            Accessible.name: qsTr("Eintrag löschen")
                        }
                    }
                }
            }
        }
    }

    EmptyState {
        visible: (page.day.entries || []).length === 0
        Layout.fillWidth: true
        Layout.topMargin: 16
        iconName: "diary"
        title: page.isToday ? qsTr("Heute noch nichts eingetragen") : qsTr("Keine Einträge an diesem Tag")
        text: qsTr("Tragen Sie Rezepte direkt aus der Rezeptansicht mit „Gegessen“ ein.")
    }

    Label {
        Layout.fillWidth: true
        Layout.topMargin: 12
        wrapMode: Label.WordWrap
        font.pixelSize: 12
        opacity: 0.6
        text: qsTr("ViviaMeals ist kein Medizinprodukt. Die Werte sind Richtwerte und ersetzen keine ärztliche oder ernährungsfachliche Beratung.")
    }
}
