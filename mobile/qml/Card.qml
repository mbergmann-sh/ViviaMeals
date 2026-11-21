// Karte mit Rand und Innenabstand
import QtQuick
import QtQuick.Controls
import QtQuick.Controls.Material

Pane {
    padding: 14
    Material.elevation: 1
    background: Rectangle {
        radius: Ui.radius
        color: Material.theme === Material.Dark ? "#2A2D31" : "white"
        border.color: Material.theme === Material.Dark ? "#3A3E44" : "#DDE3EA"
    }
}
