// Schaltfläche in der Kopfzeile
import QtQuick
import QtQuick.Controls

ToolButton {
    property string iconName
    property string tip
    icon.source: Ui.icon(iconName)
    icon.color: "white"
    Accessible.name: tip
    ToolTip.visible: hovered && tip.length > 0
    ToolTip.text: tip
    ToolTip.delay: 600
}
