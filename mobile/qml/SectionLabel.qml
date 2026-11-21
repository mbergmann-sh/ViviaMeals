// Überschrift eines Abschnitts
import QtQuick
import QtQuick.Controls
import QtQuick.Controls.Material

Label {
    font.pixelSize: 15
    font.weight: Font.DemiBold
    color: Material.theme === Material.Dark ? "#8CC4F0" : Ui.blue
    topPadding: 8
    wrapMode: Label.WordWrap
}
