// Einfarbiges Symbol in der Textfarbe (hell/dunkel) oder einer eigenen Farbe
import QtQuick
import QtQuick.Controls.Material
import QtQuick.Controls.impl

IconImage {
    property string iconName
    property int size: 24
    source: Ui.icon(iconName)
    sourceSize: Qt.size(size, size)
    color: Material.foreground
}
