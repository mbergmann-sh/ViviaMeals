// Rezeptansicht: Foto, Titel, Portionen umrechnen, Nährwerte, Zutaten, Zubereitung
import QtQuick
import QtQuick.Controls
import QtQuick.Controls.Material
import QtQuick.Layouts

ScrollPage {
    id: view
    property string recipeId
    property real servings: 0
    property var r: ({})
    maxContentWidth: 820

    function reload() {
        r = recipeId ? app.recipe(recipeId, servings) : ({})
    }
    onRecipeIdChanged: { servings = 0; reload(); flickable.contentY = 0 }
    onServingsChanged: reload()
    Component.onCompleted: { reload(); app.setCooking(true) }
    Component.onDestruction: app.setCooking(false)
    Connections {
        target: app
        function onDatabaseChanged() { view.reload() }
    }

    function stepServings(dir) {
        const s = r.servings || 1
        const step = s < 2 ? 0.5 : 1
        servings = Math.max(0.5, Math.min(99, s + dir * step))
    }

    // Foto oder farbiger Kopf
    Rectangle {
        Layout.fillWidth: true
        Layout.preferredHeight: view.r.hasImage ? Math.min(view.width * 0.56, 320) : 110
        radius: Ui.radius
        clip: true
        color: Ui.themeColor((view.r.tags || []).map(t => t.tag))
        Image {
            anchors.fill: parent
            visible: view.r.hasImage === true
            source: view.r.hasImage ? "image://recipe/" + view.recipeId + "?v=" + (Ui.main ? Ui.main.imageRevision : 0) : ""
            sourceSize.width: Math.min(1200, parent.width * 2)
            fillMode: Image.PreserveAspectCrop
            asynchronous: true
            cache: false
        }
        Icon {
            visible: !view.r.hasImage
            anchors.centerIn: parent
            iconName: "recipe"
            size: 64
            color: "white"
            opacity: 0.45
        }
    }

    Label {
        Layout.fillWidth: true
        text: view.r.title || ""
        font.pixelSize: 24
        font.weight: Font.Bold
        wrapMode: Label.WordWrap
    }

    Flow {
        Layout.fillWidth: true
        spacing: 6
        visible: (view.r.tags || []).length > 0
        Repeater {
            model: view.r.tags || []
            delegate: Chip {
                required property var modelData
                text: modelData.label
                implicitHeight: 28
                onClicked: {
                    app.recipes.theme = modelData.tag
                    if (Ui.main.canGoBack) Ui.main.back()
                }
            }
        }
    }

    Label {
        Layout.fillWidth: true
        wrapMode: Label.WordWrap
        opacity: 0.75
        text: {
            const parts = []
            if (view.r.prepMinutes > 0) parts.push(qsTr("%1 Minuten").arg(view.r.prepMinutes))
            if (view.r.season) parts.push(qsTr("Saison: %1").arg(view.r.season))
            if (view.r.source) parts.push(view.r.source)
            return parts.join(" · ")
        }
        visible: text.length > 0
    }

    // Portionen
    RowLayout {
        Layout.fillWidth: true
        spacing: 4
        RoundButton {
            icon.source: Ui.icon("minus")
            flat: true
            onClicked: view.stepServings(-1)
            Accessible.name: qsTr("Weniger Portionen")
        }
        Label {
            text: Ui.servingsText(view.r.servings || 0) + " " + (view.r.servingsLabel || "")
            font.pixelSize: 18
            font.weight: Font.Medium
        }
        RoundButton {
            icon.source: Ui.icon("plus")
            flat: true
            onClicked: view.stepServings(1)
            Accessible.name: qsTr("Mehr Portionen")
        }
        Button {
            visible: view.r.servings !== view.r.baseServings
            text: qsTr("Original")
            flat: true
            onClicked: view.servings = 0
        }
        Item { Layout.fillWidth: true }
    }

    // Aktionen
    Flow {
        Layout.fillWidth: true
        spacing: 8
        Button {
            text: qsTr("Einkaufsliste")
            icon.source: Ui.icon("cart")
            onClicked: shopMenu.open()
            Menu {
                id: shopMenu
                MenuItem {
                    text: qsTr("Alle Zutaten")
                    onTriggered: {
                        const n = app.addRecipeToShopping(view.recipeId, view.r.servings, false)
                        Ui.main.toast(qsTr("%1 Zutaten auf der Einkaufsliste.").arg(n))
                    }
                }
                MenuItem {
                    text: qsTr("Nur was im Kühlschrank fehlt")
                    onTriggered: {
                        const n = app.addRecipeToShopping(view.recipeId, view.r.servings, true)
                        Ui.main.toast(n > 0 ? qsTr("%1 fehlende Zutaten auf der Einkaufsliste.").arg(n)
                                            : qsTr("Alles da – nichts hinzugefügt."))
                    }
                }
            }
        }
        Button {
            text: qsTr("Gegessen")
            icon.source: Ui.icon("diary")
            onClicked: Ui.main.push("DiaryAddPage.qml", { recipeId: view.recipeId, recipeTitle: view.r.title })
        }
        Button {
            text: qsTr("Teilen")
            icon.source: Ui.icon("share")
            flat: true
            onClicked: app.share(view.r.title, app.recipeText(view.recipeId))
        }
    }

    NutritionView {
        Layout.fillWidth: true
        nut: view.r.nutrition || ({})
        perLabel: view.r.perLabel || qsTr("Portion")
    }

    SectionLabel { text: qsTr("Zutaten") }
    Card {
        Layout.fillWidth: true
        ColumnLayout {
            width: parent.width
            spacing: 6
            Repeater {
                model: view.r.ingredients || []
                delegate: Label {
                    required property var modelData
                    Layout.fillWidth: true
                    Layout.topMargin: modelData.heading ? 6 : 0
                    text: modelData.heading ? modelData.text : "•  " + modelData.text
                    font.weight: modelData.heading ? Font.DemiBold : Font.Normal
                    font.pixelSize: 16
                    wrapMode: Label.WordWrap
                }
            }
        }
    }

    SectionLabel { text: qsTr("Zubereitung") }
    Card {
        Layout.fillWidth: true
        Label {
            width: parent.width
            text: view.r.instructionsHtml || ""
            textFormat: Text.RichText
            wrapMode: Label.WordWrap
            font.pixelSize: 16
            lineHeight: 1.15
            onLinkActivated: (link) => Qt.openUrlExternally(link)
        }
    }
}
