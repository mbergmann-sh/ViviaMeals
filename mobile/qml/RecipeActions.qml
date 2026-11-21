// Favorit, Bearbeiten, Löschen – in der Kopfzeile der Rezeptansicht
import QtQuick
import QtQuick.Controls

Row {
    id: acts
    property string recipeId
    property bool favorite: false
    signal favoriteToggled(bool on)
    property color tint: "white"

    ToolButton {
        icon.source: Ui.icon(acts.favorite ? "star-filled" : "star")
        icon.color: acts.favorite ? "#FFC107" : acts.tint
        Accessible.name: qsTr("Favorit")
        onClicked: {
            const on = app.toggleFavorite(acts.recipeId)
            acts.favoriteToggled(on)
            app.recipes.refresh()
        }
    }
    ToolButton {
        icon.source: Ui.icon("edit")
        icon.color: acts.tint
        Accessible.name: qsTr("Bearbeiten")
        onClicked: Ui.main.editRecipe(acts.recipeId)
    }
    ToolButton {
        icon.source: Ui.icon("more")
        icon.color: acts.tint
        Accessible.name: qsTr("Weitere")
        onClicked: menu.open()
        Menu {
            id: menu
            y: parent.height
            MenuItem {
                text: qsTr("Rezept löschen …")
                onTriggered: Ui.main.ask(qsTr("Rezept löschen"),
                                         qsTr("Das Rezept wird auf diesem Gerät gelöscht. Ist ein Abgleich eingerichtet, verschwindet es beim nächsten Abgleich auch am PC."),
                                         qsTr("Löschen"), function () { app.deleteRecipe(acts.recipeId) })
            }
        }
    }
}
