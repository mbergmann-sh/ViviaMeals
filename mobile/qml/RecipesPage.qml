// Rezeptliste mit Suche und Themen. Ab 720 dp Breite steht das gewählte Rezept rechts daneben.
import QtQuick
import QtQuick.Controls
import QtQuick.Controls.Material
import QtQuick.Layouts

Page {
    id: page
    property string selectedId
    property bool selectedFavorite: false
    readonly property bool split: Ui.main ? Ui.main.split : false

    function showRecipe(id) {
        selectedId = id
        selectedFavorite = app.recipe(id, 0).favorite === true
        const row = app.recipes.indexOf(id)
        if (row >= 0) list.positionViewAtIndex(row, ListView.Contain)
    }
    function open(id) {
        if (split) showRecipe(id); else Ui.main.openRecipe(id)
    }
    Component.onCompleted: if (split) showRecipe(app.recipes.idAt(0) || app.randomRecipe())
    onSplitChanged: if (split && !selectedId) showRecipe(app.recipes.idAt(0) || app.randomRecipe())
    Connections {
        target: app
        function onDatabaseChanged() {
            if (page.selectedId && app.recipes.indexOf(page.selectedId) < 0 && !app.recipe(page.selectedId, 0).title)
                page.selectedId = app.recipes.idAt(0)
        }
    }

    header: ToolBar {
        Material.background: Ui.blue
        Material.foreground: "white"
        RowLayout {
            anchors.fill: parent
            anchors.leftMargin: 12
            spacing: 4
            TextField {
                id: search
                Layout.fillWidth: true
                placeholderText: qsTr("Rezepte oder Zutaten suchen")
                text: app.recipes.query
                inputMethodHints: Qt.ImhNoPredictiveText
                Material.accent: "white"
                color: "white"
                placeholderTextColor: "#D6E6F5"
                onTextChanged: searchDelay.restart()
                Keys.onReturnPressed: { searchDelay.stop(); app.recipes.query = text; Qt.inputMethod.hide() }
                Timer { id: searchDelay; interval: 250; onTriggered: app.recipes.query = search.text }
            }
            HeaderButton {
                visible: search.text.length > 0
                iconName: "close"
                tip: qsTr("Suche leeren")
                onClicked: { search.text = ""; app.recipes.query = "" }
            }
            HeaderButton {
                iconName: "dice"
                tip: qsTr("Zufälliges Rezept")
                onClicked: page.open(app.randomRecipe())
            }
            RecipeActions {
                visible: page.split && page.selectedId.length > 0
                recipeId: page.selectedId
                favorite: page.selectedFavorite
                onFavoriteToggled: (on) => page.selectedFavorite = on
            }
        }
    }

    RowLayout {
        anchors.fill: parent
        spacing: 0

        Item {
            Layout.fillHeight: true
            Layout.fillWidth: !page.split
            Layout.preferredWidth: page.split ? Math.min(400, Math.max(300, page.width * 0.36)) : -1

            ColumnLayout {
                anchors.fill: parent
                spacing: 0

                // Themen
                ListView {
                    id: themeList
                    Layout.fillWidth: true
                    Layout.preferredHeight: 52
                    orientation: ListView.Horizontal
                    spacing: 6
                    leftMargin: 10; rightMargin: 10
                    clip: true
                    model: app.themes()
                    Connections {
                        target: app
                        function onDatabaseChanged() { themeList.model = app.themes() }
                    }
                    delegate: Chip {
                        required property var modelData
                        anchors.verticalCenter: parent ? parent.verticalCenter : undefined
                        text: modelData.label + "  " + modelData.count
                        selected: app.recipes.theme === modelData.tag
                        visible: modelData.count > 0 || modelData.tag === "" || modelData.tag === ":fav"
                        width: visible ? implicitWidth : 0
                        onClicked: app.recipes.theme = modelData.tag
                    }
                }

                Label {
                    Layout.fillWidth: true
                    Layout.leftMargin: 16
                    text: app.recipes.count === app.recipes.total
                          ? qsTr("%1 Rezepte").arg(app.recipes.total)
                          : qsTr("%1 von %2 Rezepten").arg(app.recipes.count).arg(app.recipes.total)
                    font.pixelSize: 13
                    opacity: 0.7
                }

                ListView {
                    id: list
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    clip: true
                    model: app.recipes
                    reuseItems: true
                    cacheBuffer: 600
                    ScrollIndicator.vertical: ScrollIndicator {}
                    section.property: app.recipes.query.length === 0 ? "letter" : ""
                    section.delegate: Label {
                        required property string section
                        width: ListView.view.width
                        leftPadding: 16
                        topPadding: 8
                        text: section
                        font.weight: Font.Bold
                        color: Ui.blue
                    }
                    delegate: ItemDelegate {
                        id: row
                        required property string recipeId
                        required property string title
                        required property string subtitle
                        required property bool favorite
                        required property bool hasImage
                        required property var tags
                        width: ListView.view.width
                        height: 72
                        highlighted: page.split && page.selectedId === recipeId
                        onClicked: page.open(recipeId)
                        contentItem: RowLayout {
                            spacing: 12
                            RecipeThumb {
                                Layout.preferredWidth: 52
                                Layout.preferredHeight: 52
                                recipeId: row.recipeId
                                hasImage: row.hasImage
                                tags: row.tags
                                title: row.title
                                revision: Ui.main ? Ui.main.imageRevision : 0
                            }
                            ColumnLayout {
                                Layout.fillWidth: true
                                spacing: 2
                                Label {
                                    Layout.fillWidth: true
                                    text: row.title
                                    font.pixelSize: 16
                                    font.weight: Font.Medium
                                    elide: Label.ElideRight
                                }
                                Label {
                                    Layout.fillWidth: true
                                    text: row.subtitle
                                    font.pixelSize: 13
                                    opacity: 0.7
                                    elide: Label.ElideRight
                                }
                            }
                            Icon {
                                visible: row.favorite
                                iconName: "star-filled"
                                size: 18
                                color: "#F5A623"
                            }
                        }
                    }
                    EmptyState {
                        anchors.centerIn: parent
                        width: parent.width - 48
                        visible: list.count === 0
                        iconName: "search"
                        title: qsTr("Nichts gefunden")
                        text: app.recipes.theme === ":fav"
                              ? qsTr("Tippen Sie in einem Rezept auf den Stern, um es zu den Favoriten zu legen.")
                              : qsTr("Versuchen Sie einen anderen Suchbegriff oder wählen Sie „Alle Rezepte“.")
                    }
                }
            }

            RoundButton {
                anchors { right: parent.right; bottom: parent.bottom; margins: 16 }
                width: 60; height: 60
                highlighted: true
                Material.accent: Ui.orange
                icon.source: Ui.icon("plus")
                icon.color: "white"
                onClicked: Ui.main.editRecipe("")
                Accessible.name: qsTr("Neues Rezept")
                ToolTip.visible: hovered
                ToolTip.text: qsTr("Neues Rezept")
            }
        }

        Rectangle {
            visible: page.split
            Layout.fillHeight: true
            Layout.preferredWidth: 1
            color: Material.theme === Material.Dark ? "#3A3E44" : "#DDE3EA"
        }

        Loader {
            Layout.fillWidth: true
            Layout.fillHeight: true
            active: page.split
            visible: active
            sourceComponent: page.selectedId ? viewComp : emptyComp
        }
    }

    Component {
        id: viewComp
        RecipeView { recipeId: page.selectedId }
    }
    Component {
        id: emptyComp
        Item {
            EmptyState {
                anchors.centerIn: parent
                width: parent.width - 64
                iconName: "recipe"
                title: qsTr("Rezept wählen")
                text: qsTr("Tippen Sie links auf ein Rezept.")
            }
        }
    }
}
