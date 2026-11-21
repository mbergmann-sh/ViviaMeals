// Ich: persönliche Angaben, Tagesbedarf, Ziel und Gewichtsverlauf (eine Person je Gerät)
import QtQuick
import QtQuick.Controls
import QtQuick.Controls.Material
import QtQuick.Layouts

ScrollPage {
    id: page
    property var p: app.profile()
    property var w: app.weights()
    property var pals: app.palLevels()
    property var choices: app.profileChoices()
    readonly property bool wide: width >= 900
    maxContentWidth: wide ? 1200 : 760

    function reload() {
        p = app.profile()
        w = app.weights()
        choices = app.profileChoices()
        fill()
    }
    function fill() {
        nameField.text = p.name || ""
        sexBox.currentIndex = p.female ? 0 : 1
        yearField.text = p.birthYear > 0 ? p.birthYear.toString() : ""
        heightField.text = p.heightCm > 0 ? Ui.num(p.heightCm) : ""
        let best = 0
        for (let i = 0; i < pals.length; ++i)
            if (Math.abs(pals[i].value - p.pal) < Math.abs(pals[best].value - p.pal)) best = i
        palBox.currentIndex = best
        goalBox.currentIndex = p.goal || 0
        targetField.text = p.targetKg > 0 ? Ui.num(p.targetKg, 1) : ""
        rateBox.currentIndex = Math.max(0, [0.25, 0.5, 0.75, 1.0].indexOf(p.ratePerWeek))
    }
    function n(t) { return parseFloat(t.replace(",", ".")) || 0 }
    function save() {
        app.saveProfile({ name: nameField.text, female: sexBox.currentIndex === 0, birthYear: parseInt(yearField.text) || 0,
                          heightCm: n(heightField.text), pal: pals[palBox.currentIndex].value, goal: goalBox.currentIndex,
                          targetKg: n(targetField.text), ratePerWeek: [0.25, 0.5, 0.75, 1.0][rateBox.currentIndex] })
        Ui.main.toast(qsTr("Gespeichert."))
    }
    Component.onCompleted: fill()
    Connections {
        target: app
        function onProfileChanged() { page.p = app.profile(); page.w = app.weights() }
        function onDatabaseChanged() { page.reload() }
    }

    header: PageHeader {
        title: page.p.name ? page.p.name : qsTr("Ich")
        HeaderButton { iconName: "save"; tip: qsTr("Speichern"); onClicked: page.save() }
    }

    // Nach dem Abgleich mit mehreren Personen vom PC: wer bin ich?
    Card {
        visible: page.choices.length > 1
        Layout.fillWidth: true
        ColumnLayout {
            width: parent.width
            Label {
                Layout.fillWidth: true
                wrapMode: Label.WordWrap
                text: qsTr("Vom Computer sind mehrere Personen übernommen worden. Wessen Tagebuch soll auf diesem Gerät geführt werden?")
            }
            Flow {
                Layout.fillWidth: true
                spacing: 6
                Repeater {
                    model: page.choices
                    delegate: Chip {
                        required property var modelData
                        text: modelData.name
                        selected: modelData.current
                        onClicked: { app.chooseProfile(modelData.id); page.reload() }
                    }
                }
            }
        }
    }

    GridLayout {
        Layout.fillWidth: true
        columns: page.wide ? 2 : 1
        columnSpacing: Ui.gap
        rowSpacing: Ui.gap

        // ---- Ergebnis und Gewicht
        ColumnLayout {
            Layout.fillWidth: true
            Layout.alignment: Qt.AlignTop
            spacing: Ui.gap

            Card {
                Layout.fillWidth: true
                ColumnLayout {
                    width: parent.width
                    spacing: 4
                    Label {
                        visible: page.p.targetKcal > 0
                        text: qsTr("Tagesziel")
                        opacity: 0.7
                    }
                    RowLayout {
                        visible: page.p.targetKcal > 0
                        Label { text: Ui.num(page.p.targetKcal); font.pixelSize: 32; font.weight: Font.Bold; color: Ui.orange }
                        Label { text: qsTr("kcal am Tag"); Layout.alignment: Qt.AlignBaseline }
                    }
                    Label {
                        visible: page.p.targetKcal > 0
                        Layout.fillWidth: true
                        wrapMode: Label.WordWrap
                        text: qsTr("Gesamtumsatz %1 kcal · BMI %2 (%3)").arg(Ui.num(page.p.tdee)).arg(page.p.bmi).arg(page.p.bmiClass)
                    }
                    Label {
                        visible: (page.p.hint || "").length > 0
                        Layout.fillWidth: true
                        wrapMode: Label.WordWrap
                        color: Ui.orange
                        text: page.p.hint || ""
                    }
                    Label {
                        visible: !(page.p.targetKcal > 0)
                        Layout.fillWidth: true
                        wrapMode: Label.WordWrap
                        text: qsTr("Tragen Sie Ihre Angaben und Ihr aktuelles Gewicht ein. Dann berechnet ViviaMeals Ihren Tagesbedarf und ein Tagesziel.")
                    }
                }
            }

            SectionLabel { text: qsTr("Gewicht") }
            RowLayout {
                Layout.fillWidth: true
                TextField {
                    id: kgField
                    Layout.minimumWidth: 80
                    Layout.fillWidth: true
                    placeholderText: page.p.currentKg > 0 ? qsTr("zuletzt %1 kg").arg(Ui.num(page.p.currentKg, 1)) : qsTr("Gewicht heute in kg")
                    inputMethodHints: Qt.ImhFormattedNumbersOnly
                    validator: RegularExpressionValidator { regularExpression: /^\d{0,3}([.,]\d{0,1})?$/ }
                    EnterKey.type: Qt.EnterKeyDone
                    onAccepted: addWeight()
                    function addWeight() {
                        const kg = page.n(text)
                        if (kg < 20 || kg > 400) { Ui.main.toast(qsTr("Bitte ein Gewicht zwischen 20 und 400 kg eingeben.")); return }
                        app.addWeight("", kg)
                        text = ""
                        Qt.inputMethod.hide()
                    }
                }
                Button { text: qsTr("Eintragen"); highlighted: true; onClicked: kgField.addWeight() }
            }

            // Verlauf
            Canvas {
                id: chart
                visible: page.w.length > 1
                Layout.fillWidth: true
                Layout.preferredHeight: 140
                onPaint: {
                    const ctx = getContext("2d")
                    ctx.reset()
                    const pts = page.w.slice().reverse()
                    if (pts.length < 2) return
                    let lo = 1e9, hi = -1e9
                    for (const q of pts) { lo = Math.min(lo, q.kg); hi = Math.max(hi, q.kg) }
                    if (page.p.targetKg > 0) { lo = Math.min(lo, page.p.targetKg); hi = Math.max(hi, page.p.targetKg) }
                    if (hi - lo < 1) { hi += 0.5; lo -= 0.5 }
                    const pad = 10
                    const x = i => pad + i * (width - 2 * pad) / (pts.length - 1)
                    const y = v => pad + (hi - v) * (height - 2 * pad) / (hi - lo)
                    if (page.p.targetKg > 0) {
                        ctx.strokeStyle = Ui.green
                        ctx.setLineDash([4, 4])
                        ctx.beginPath(); ctx.moveTo(pad, y(page.p.targetKg)); ctx.lineTo(width - pad, y(page.p.targetKg)); ctx.stroke()
                        ctx.setLineDash([])
                    }
                    ctx.strokeStyle = Ui.blue
                    ctx.lineWidth = 2.5
                    ctx.beginPath()
                    for (let i = 0; i < pts.length; ++i) i ? ctx.lineTo(x(i), y(pts[i].kg)) : ctx.moveTo(x(i), y(pts[i].kg))
                    ctx.stroke()
                    ctx.fillStyle = Ui.orange
                    for (let i = 0; i < pts.length; ++i) { ctx.beginPath(); ctx.arc(x(i), y(pts[i].kg), 3.5, 0, 2 * Math.PI); ctx.fill() }
                }
                Connections { target: page; function onWChanged() { chart.requestPaint() } }
                onWidthChanged: requestPaint()
            }

            Repeater {
                model: page.w.slice(0, 8)
                delegate: RowLayout {
                    required property var modelData
                    Layout.fillWidth: true
                    Label { text: modelData.label; Layout.fillWidth: true }
                    Label { text: modelData.text; font.weight: Font.Medium }
                    ToolButton {
                        icon.source: Ui.icon("delete")
                        opacity: 0.6
                        onClicked: app.removeWeight(modelData.date)
                        Accessible.name: qsTr("Eintrag löschen")
                    }
                }
            }
        }

        // ---- Angaben
        Card {
            Layout.fillWidth: true
            Layout.alignment: Qt.AlignTop
            GridLayout {
                width: parent.width
                columns: 2
                columnSpacing: Ui.gap
                rowSpacing: 4
                Label { text: qsTr("Name") }
                TextField { id: nameField; Layout.fillWidth: true; EnterKey.type: Qt.EnterKeyNext; Keys.onReturnPressed: yearField.forceActiveFocus() }
                Label { text: qsTr("Geschlecht") }
                ComboBox { id: sexBox; Layout.fillWidth: true; model: [qsTr("weiblich"), qsTr("männlich")] }
                Label { text: qsTr("Geburtsjahr") }
                TextField {
                    id: yearField
                    Layout.fillWidth: true
                    inputMethodHints: Qt.ImhDigitsOnly
                    validator: IntValidator { bottom: 1900; top: 2100 }
                    EnterKey.type: Qt.EnterKeyNext
                    Keys.onReturnPressed: heightField.forceActiveFocus()
                }
                Label { text: qsTr("Größe (cm)") }
                TextField {
                    id: heightField
                    Layout.fillWidth: true
                    inputMethodHints: Qt.ImhDigitsOnly
                    validator: IntValidator { bottom: 0; top: 250 }
                    EnterKey.type: Qt.EnterKeyNext
                    Keys.onReturnPressed: targetField.forceActiveFocus()
                }
                Label { text: qsTr("Alltag") }
                ComboBox {
                    id: palBox
                    Layout.fillWidth: true
                    model: page.pals
                    textRole: "text"
                    // lange Beschreibungen dürfen umbrechen
                    delegate: ItemDelegate {
                        required property var modelData
                        required property int index
                        width: ListView.view ? ListView.view.width : 300
                        contentItem: Label { text: modelData.text; wrapMode: Label.WordWrap }
                        highlighted: palBox.highlightedIndex === index
                    }
                }
                Label { text: qsTr("Ziel") }
                ComboBox { id: goalBox; Layout.fillWidth: true; model: [qsTr("abnehmen"), qsTr("Gewicht halten"), qsTr("zunehmen")] }
                Label { text: qsTr("Zielgewicht (kg)"); visible: goalBox.currentIndex !== 1 }
                TextField {
                    id: targetField
                    visible: goalBox.currentIndex !== 1
                    Layout.fillWidth: true
                    inputMethodHints: Qt.ImhFormattedNumbersOnly
                    validator: RegularExpressionValidator { regularExpression: /^\d{0,3}([.,]\d{0,1})?$/ }
                    EnterKey.type: Qt.EnterKeyDone
                    Keys.onReturnPressed: Qt.inputMethod.hide()
                }
                Label { text: qsTr("Tempo"); visible: goalBox.currentIndex !== 1 }
                ComboBox {
                    id: rateBox
                    visible: goalBox.currentIndex !== 1
                    Layout.fillWidth: true
                    model: [qsTr("0,25 kg pro Woche"), qsTr("0,5 kg pro Woche"), qsTr("0,75 kg pro Woche"), qsTr("1 kg pro Woche")]
                }
                Item { Layout.columnSpan: 1 }
                Button { text: qsTr("Speichern"); highlighted: true; Layout.alignment: Qt.AlignRight; onClicked: page.save() }
            }
        }
    }

    Label {
        Layout.fillWidth: true
        wrapMode: Label.WordWrap
        font.pixelSize: 12
        opacity: 0.6
        text: qsTr("ViviaMeals ist kein Medizinprodukt. Bei Vorerkrankungen, in der Schwangerschaft oder bei starkem Über- oder Untergewicht besprechen Sie Ihr Ziel bitte mit Ihrer Ärztin oder Ihrem Arzt.")
    }
}
