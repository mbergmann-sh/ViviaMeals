// Seite mit rollbarem Inhalt. Hält das Eingabefeld mit dem Fokus über der Bildschirmtastatur:
// Android verkleinert das Fenster (adjustResize); zusätzlich wird hier das aktive Feld bzw. die
// Schreibmarke in den sichtbaren Bereich gerollt – auch beim Drehen mit offener Tastatur.
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

Page {
    id: page
    default property alias content: column.data
    property alias flickable: flick
    property int maxContentWidth: 760
    property int bottomSpace: 24

    Flickable {
        id: flick
        anchors.fill: parent
        contentWidth: width
        contentHeight: column.implicitHeight + 2 * Ui.gap + page.bottomSpace + keyboardOverlap
        boundsBehavior: Flickable.StopAtBounds
        ScrollIndicator.vertical: ScrollIndicator {}

        // Teil des Flickable, den die Tastatur trotz adjustResize verdeckt (z. B. schwebende Tastaturen)
        property real keyboardOverlap: {
            if (!Qt.inputMethod.visible) return 0
            const kb = Qt.inputMethod.keyboardRectangle
            if (kb.height <= 0 || !page.Window.window) return 0
            const bottom = flick.mapToItem(null, 0, flick.height).y
            const kbTop = kb.y / Screen.devicePixelRatio
            return Math.max(0, bottom - kbTop)
        }

        ColumnLayout {
            id: column
            x: Math.max(Ui.gap, (flick.width - width) / 2)
            y: Ui.gap
            width: Math.min(flick.width - 2 * Ui.gap, page.maxContentWidth)
            spacing: Ui.gap
        }
    }

    function ensureVisible(item) {
        if (!item || !page.visible) return
        let p = item
        while (p && p !== column) p = p.parent
        if (!p) return   // Feld gehört nicht zu dieser Seite
        let r = Qt.rect(0, 0, item.width, item.height)
        if (item.cursorRectangle !== undefined && item.height > 120) {
            const c = item.cursorRectangle
            r = Qt.rect(0, Math.max(0, c.y - 40), item.width, c.height + 80)
        }
        const top = item.mapToItem(flick.contentItem, r.x, r.y).y
        const visibleHeight = flick.height - flick.keyboardOverlap
        const margin = 16
        if (top - margin < flick.contentY)
            flick.contentY = Math.max(0, top - margin)
        else if (top + r.height + margin > flick.contentY + visibleHeight)
            flick.contentY = Math.min(Math.max(0, flick.contentHeight - flick.height),
                                      top + r.height + margin - visibleHeight)
    }

    Timer {
        id: settle
        interval: 250   // Tastatur und Fenstergröße abwarten
        onTriggered: page.ensureVisible(page.Window.activeFocusItem)
    }
    Connections {
        target: page.Window.window
        function onActiveFocusItemChanged() { settle.restart() }
    }
    Connections {
        target: Qt.inputMethod
        function onKeyboardRectangleChanged() { settle.restart() }
        function onVisibleChanged() { settle.restart() }
    }
    onHeightChanged: settle.restart()
    Connections {
        target: page.Window.activeFocusItem && page.Window.activeFocusItem.cursorRectangle !== undefined
                ? page.Window.activeFocusItem : null
        ignoreUnknownSignals: true
        function onCursorRectangleChanged() { settle.restart() }
    }
}
