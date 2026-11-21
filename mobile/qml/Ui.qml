// ViviaMeals Mobil – gemeinsame Farben, Maße und Hilfsfunktionen
// SPDX-License-Identifier: PolyForm-Noncommercial-1.0.0 (siehe LICENSE)
pragma Singleton
import QtQuick

QtObject {
    property var main: null                       // Hauptfenster, setzt Main.qml

    readonly property color blue: "#1474BC"       // Vivia-Blau
    readonly property color orange: "#E0620D"     // Vivia-Orange
    readonly property color green: "#2E9E6B"
    readonly property color red: "#C0392B"
    readonly property int gap: 12
    readonly property int radius: 10

    function icon(name) { return "qrc:/icons/" + name + ".svg" }

    // Farbe für Rezepte ohne Foto, nach dem ersten passenden Thema
    function themeColor(tags) {
        const t = (tags || []).join(" ")
        const map = [["desserts", "#C2185B"], ["backen", "#8D6E63"], ["fisch", "#0277BD"],
                     ["fleischgerichte", "#B23A26"], ["vegan", "#2E7D32"], ["salate", "#7CB342"],
                     ["suppen", "#EF6C00"], ["eintöpfe", "#EF6C00"], ["frühstück", "#F9A825"],
                     ["beilagen", "#6D4C41"], ["vegetarisch", "#558B2F"]]
        for (let i = 0; i < map.length; ++i)
            if (t.indexOf(map[i][0]) >= 0) return map[i][1]
        return blue
    }

    function num(x, digits) {
        return Number(x).toLocaleString(Qt.locale("de_DE"), 'f', digits === undefined ? 0 : digits)
    }
    function servingsText(x) {
        return Math.abs(x - Math.round(x)) < 0.01 ? Math.round(x).toString() : num(x, 1)
    }
    function todayIso() {
        const d = new Date()
        return Qt.formatDate(d, "yyyy-MM-dd")
    }
    function addDays(iso, n) {
        const p = iso.split("-")
        const d = new Date(+p[0], +p[1] - 1, +p[2] + n)
        return Qt.formatDate(d, "yyyy-MM-dd")
    }
}
