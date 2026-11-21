// Vorschaubild: Foto oder farbige Fläche mit Anfangsbuchstabe
import QtQuick
import QtQuick.Controls

Rectangle {
    id: t
    property string recipeId
    property bool hasImage: false
    property var tags: []
    property string title
    property int revision: 0
    radius: 8
    clip: true
    color: Ui.themeColor(tags)
    Label {
        anchors.centerIn: parent
        visible: !img.visible
        text: t.title.length ? t.title.charAt(0).toUpperCase() : "?"
        color: "white"
        font.pixelSize: t.height * 0.45
        font.weight: Font.Bold
    }
    Image {
        id: img
        anchors.fill: parent
        visible: t.hasImage && status === Image.Ready
        source: t.hasImage ? "image://recipe/" + t.recipeId + "?v=" + t.revision : ""
        sourceSize: Qt.size(t.width * 2, t.height * 2)
        fillMode: Image.PreserveAspectCrop
        asynchronous: true
        cache: false
    }
}
