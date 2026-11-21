// Einzelnes Rezept (Telefon): Kopfzeile mit Favorit, Bearbeiten und Löschen
import QtQuick
import QtQuick.Controls
import QtQuick.Controls.Material

Page {
    id: page
    property string recipeId
    property bool favorite: false
    property string recipeTitle

    function refreshHeader() {
        const r = app.recipe(recipeId, 0)
        favorite = r.favorite === true
        recipeTitle = r.title || ""
    }
    Component.onCompleted: refreshHeader()
    Connections {
        target: app
        function onDatabaseChanged() {
            const r = app.recipe(page.recipeId, 0)
            if (!r.title) Ui.main.back(); else page.refreshHeader()
        }
    }

    header: PageHeader {
        title: page.recipeTitle
        RecipeActions {
            recipeId: page.recipeId
            favorite: page.favorite
            onFavoriteToggled: (on) => page.favorite = on
        }
    }

    RecipeView {
        anchors.fill: parent
        recipeId: page.recipeId
    }
}
