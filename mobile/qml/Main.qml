// ViviaMeals Mobil – Hauptfenster
//
// Unter 600 dp Breite: Navigation unten, Seiten übereinander (Liste → Rezept).
// Ab 600 dp: Navigation am linken Rand; ab 720 dp Liste und Rezept nebeneinander.
// Die Zurück-Geste führt erst durch die eigenen Seiten, dann zur Rezeptliste.
// SPDX-License-Identifier: PolyForm-Noncommercial-1.0.0 (siehe LICENSE)
import QtQuick
import QtQuick.Controls
import QtQuick.Controls.Material
import QtQuick.Layouts
import QtQuick.Window

ApplicationWindow {
    id: win
    width: startWidth > 0 ? startWidth : 420
    height: startHeight > 0 ? startHeight : 860
    visible: true
    title: "ViviaMeals"

    Material.theme: app.themeMode === 1 ? Material.Light : app.themeMode === 2 ? Material.Dark : Material.System
    Material.primary: Ui.blue
    Material.accent: Ui.orange

    readonly property bool rail: width >= 600            // Navigation am Rand
    readonly property bool split: width >= 720           // Liste und Rezept nebeneinander
    readonly property bool canGoBack: stack.depth > 1
    readonly property bool fullScreen: stack.currentItem ? stack.currentItem.fullScreen === true : false
    property int tab: 0
    property int imageRevision: 0                         // nach dem Speichern Fotos neu laden

    readonly property var tabs: [
        { label: qsTr("Rezepte"), icon: "recipe", page: "RecipesPage.qml" },
        { label: qsTr("Einkauf"), icon: "cart", page: "ShoppingPage.qml" },
        { label: qsTr("Tagebuch"), icon: "diary", page: "DiaryPage.qml" },
        { label: qsTr("Ich"), icon: "user", page: "MePage.qml" },
        { label: qsTr("Mehr"), icon: "menu", page: "MorePage.qml" }
    ]

    // ------------------------------------------------------------------ Navigation
    function selectTab(i) {
        tab = i
        stack.clear(StackView.Immediate)
        stack.push(Qt.resolvedUrl(tabs[i].page), {}, StackView.Immediate)
    }
    function push(page, props) {
        return stack.push(Qt.resolvedUrl(page), props || {})
    }
    function back() {
        if (popup.opened) { popup.close(); return true }
        if (stack.depth > 1) {
            const cur = stack.currentItem
            if (cur && cur.guardBack === true && cur.leave) { cur.leave(); return true }
            stack.pop()
            return true
        }
        if (tab !== 0) { selectTab(0); return true }
        return false
    }
    function openRecipe(id) {
        if (!id) return
        if (split && tab === 0 && stack.depth === 1 && stack.currentItem.showRecipe) {
            stack.currentItem.showRecipe(id)
            return
        }
        push("RecipePage.qml", { recipeId: id })
    }
    function editRecipe(id) { push("EditorPage.qml", { recipeId: id || "" }) }
    function toast(text) { toastLabel.text = text; toastBox.open(); toastTimer.restart() }

    // Android: nicht abgefangene Zurück-Geste
    onClosing: (close) => { if (app.isAndroid && back()) close.accepted = false }

    // ------------------------------------------------------------------ Sicherer Bereich (Kameraloch, Leisten)
    // SafeArea gibt es ab Qt 6.9; mit älterem Qt (Test am PC) bleiben die Ränder 0.
    property var safe: null
    readonly property real safeTop: safe ? safe.top : 0
    readonly property real safeBottom: safe ? safe.bottom : 0
    readonly property real safeLeft: safe ? safe.left : 0
    readonly property real safeRight: safe ? safe.right : 0

    Item {
        id: root
        anchors.fill: parent

        Rectangle {   // Statusleiste in Vivia-Blau
            anchors { left: parent.left; right: parent.right; top: parent.top }
            height: win.safeTop
            color: Qt.darker(Ui.blue, 1.15)
        }

        // Navigationsleiste links (Tablet, Telefon quer)
        Rectangle {
            id: railBar
            visible: win.rail && !win.fullScreen
            x: win.safeLeft
            y: win.safeTop
            width: visible ? 84 : 0
            height: parent.height - win.safeTop - win.safeBottom
            color: Material.theme === Material.Dark ? "#202326" : "#F1F5F9"
            ColumnLayout {
                anchors.fill: parent
                anchors.topMargin: 8
                spacing: 2
                Image {
                    Layout.alignment: Qt.AlignHCenter
                    source: "qrc:/images/viviameals.svg"
                    sourceSize: Qt.size(40, 40)
                    Layout.bottomMargin: 8
                }
                Repeater {
                    model: win.tabs
                    delegate: ToolButton {
                        required property var modelData
                        required property int index
                        Layout.fillWidth: true
                        Layout.preferredHeight: 64
                        display: AbstractButton.TextUnderIcon
                        text: modelData.label
                        font.pixelSize: 12
                        font.capitalization: Font.MixedCase
                    leftPadding: 2
                    rightPadding: 2
                        icon.source: Ui.icon(modelData.icon)
                        icon.color: win.tab === index ? Ui.blue : Material.foreground
                        checked: win.tab === index
                        onClicked: win.selectTab(index)
                    }
                }
                Item { Layout.fillHeight: true }
            }
        }

        StackView {
            id: stack
            anchors {
                left: railBar.visible ? railBar.right : parent.left
                leftMargin: railBar.visible ? 0 : win.safeLeft
                right: parent.right
                rightMargin: win.safeRight
                top: parent.top
                topMargin: win.safeTop
                bottom: tabBar.visible ? tabBar.top : parent.bottom
                bottomMargin: tabBar.visible ? 0 : win.safeBottom
            }
            focus: true
            Keys.onBackPressed: (event) => { event.accepted = win.back() }
        }

        // Navigationsleiste unten (Telefon hochkant)
        TabBar {
            id: tabBar
            visible: !win.rail && !win.fullScreen
            anchors { left: parent.left; right: parent.right; bottom: parent.bottom }
            bottomPadding: win.safeBottom
            currentIndex: win.tab
            Repeater {
                model: win.tabs
                delegate: TabButton {
                    required property var modelData
                    required property int index
                    display: AbstractButton.TextUnderIcon
                    text: modelData.label
                    font.pixelSize: 12
                    font.capitalization: Font.MixedCase
                    leftPadding: 2
                    rightPadding: 2
                    icon.source: Ui.icon(modelData.icon)
                    icon.color: win.tab === index ? Ui.blue : Material.foreground
                    onClicked: win.selectTab(index)
                }
            }
        }
    }

    // ------------------------------------------------------------------ Meldungen und Fortschritt
    Popup {
        id: toastBox
        x: (win.width - width) / 2
        y: win.height - height - win.safeBottom - (tabBar.visible ? tabBar.height : 0) - 16
        width: Math.min(win.width - 32, 520)
        padding: 14
        closePolicy: Popup.CloseOnPressOutside
        background: Rectangle { radius: 8; color: "#323232" }
        contentItem: Label { id: toastLabel; color: "white"; wrapMode: Label.WordWrap }
        Timer { id: toastTimer; interval: 3500; onTriggered: toastBox.close() }
    }

    Popup {
        id: busyBox
        modal: true
        visible: app.busy
        closePolicy: Popup.NoAutoClose
        anchors.centerIn: Overlay.overlay
        width: Math.min(win.width - 48, 360)
        padding: 20
        ColumnLayout {
            width: parent.width
            spacing: 12
            BusyIndicator { Layout.alignment: Qt.AlignHCenter; running: app.busy }
            Label { text: app.busyText; Layout.fillWidth: true; wrapMode: Label.WordWrap; horizontalAlignment: Text.AlignHCenter }
            ProgressBar {
                Layout.fillWidth: true
                visible: app.busyTotal > 0
                from: 0; to: Math.max(1, app.busyTotal); value: app.busyDone
            }
        }
    }

    // Rückfrage- und Hinweisdialog für alle Seiten
    Dialog {
        id: popup
        property var onYes: null
        anchors.centerIn: Overlay.overlay
        width: Math.min(win.width - 32, 460)
        modal: true
        Label { id: popupText; width: parent.width; wrapMode: Label.WordWrap }
        onAccepted: if (onYes) onYes()
    }
    function ask(title, text, yesText, action) {
        popup.title = title
        popupText.text = text
        popup.onYes = action
        popup.standardButtons = Dialog.Cancel | Dialog.Ok
        popup.open()
        const ok = popup.standardButton(Dialog.Ok)
        if (ok) ok.text = yesText
        const no = popup.standardButton(Dialog.Cancel)
        if (no) no.text = qsTr("Abbrechen")
    }
    function inform(title, text) {
        popup.title = title
        popupText.text = text
        popup.onYes = null
        popup.standardButtons = Dialog.Ok
        popup.open()
    }

    Connections {
        target: app
        function onMessage(text) { win.toast(text) }
        function onImportFinished(ok, text) {
            if (ok) win.toast(text); else win.inform(qsTr("Import"), text)
        }
        function onSyncFinished(ok, text) {
            if (ok) win.toast(text); else win.inform(qsTr("Abgleich"), text)
        }
    }

    Component.onCompleted: {
        Ui.main = win
        try {
            safe = Qt.createQmlObject('import QtQuick; QtObject { property Item target; ' +
                                      'readonly property real top: (target && target.SafeArea) ? target.SafeArea.margins.top : 0; ' +
                                      'readonly property real bottom: (target && target.SafeArea) ? target.SafeArea.margins.bottom : 0; ' +
                                      'readonly property real left: (target && target.SafeArea) ? target.SafeArea.margins.left : 0; ' +
                                      'readonly property real right: (target && target.SafeArea) ? target.SafeArea.margins.right : 0 }', win, "safearea")
            safe.target = root
        } catch (e) {
            safe = null
        }
        selectTab(0)
        if (app.firstRun && !screenshotMode) push("WelcomePage.qml")
    }

    // ------------------------------------------------------------------ Bildschirmfotos (--screenshots)
    property bool demoReady: false
    function prepareDemo() {
        if (demoReady) return
        demoReady = true
        app.saveProfile({ name: "Erika", female: true, birthYear: 1972, heightCm: 168, pal: 1.5, goal: 0, targetKg: 68, ratePerWeek: 0.5 })
        const w = [74.8, 74.5, 74.1, 73.9, 73.6, 73.2]
        for (let i = 0; i < w.length; ++i) app.addWeight(Ui.addDays(Ui.todayIso(), -7 * (w.length - 1 - i)), w[i])
        app.setStock(["Kartoffeln", "Zwiebeln", "Möhren", "Eier", "Milch", "Butter", "Lauch", "Bergkäse"])
        app.recipes.query = "kartoffelsuppe"
        const soup = app.recipes.idAt(0)
        app.recipes.query = "linsen-dal"
        const dal = app.recipes.idAt(0)
        app.recipes.query = "bircher"
        const breakfast = app.recipes.idAt(0)
        app.recipes.query = ""
        if (soup) app.addRecipeToShopping(soup, 4, false)
        app.addShoppingText("Kaffee")
        app.addShoppingText("Spülmittel")
        if (breakfast) app.addDiaryRecipe(breakfast, 1, app.mealNames()[0], "")
        if (soup) app.addDiaryRecipe(soup, 1.5, app.mealNames()[2], "")
        app.addDiaryFree(qsTr("Apfel"), 75, app.mealNames()[1], "")
        if (dal) app.toggleFavorite(dal)
    }
    function demoRecipe() {
        app.recipes.query = "gemüsecurry"
        const id = app.recipes.idAt(0)
        app.recipes.query = ""
        return id
    }
    function showScreen(name) {
        prepareDemo()
        popup.close()
        const map = { recipes: 0, recipe: 0, editor: 0, shopping: 1, fridge: 1, diary: 2, me: 3, plan: 4,
                      more: 4, desktop: 4, sync: 4, glossary: 4, welcome: 0 }
        selectTab(map[name] !== undefined ? map[name] : 0)
        if (name === "recipe") openRecipe(demoRecipe())
        else if (name === "editor") editRecipe(demoRecipe())
        else if (name === "fridge") push("FridgePage.qml")
        else if (name === "plan") push("PlanPage.qml")
        else if (name === "desktop") push("DesktopPage.qml")
        else if (name === "sync") push("SyncPage.qml")
        else if (name === "glossary") push("GlossaryPage.qml", { query: "blanchieren" })
        else if (name === "welcome") push("WelcomePage.qml")
        else if (name === "recipes" && split) stack.currentItem.showRecipe(demoRecipe())
    }
}
