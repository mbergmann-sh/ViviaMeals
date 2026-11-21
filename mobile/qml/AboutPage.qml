// Über ViviaMeals
import QtQuick
import QtQuick.Controls
import QtQuick.Controls.Material
import QtQuick.Layouts

ScrollPage {
    header: PageHeader { title: qsTr("Über ViviaMeals") }

    RowLayout {
        Layout.fillWidth: true
        spacing: 16
        Image { source: "qrc:/images/viviameals.svg"; sourceSize: Qt.size(72, 72) }
        ColumnLayout {
            Label { text: "ViviaMeals"; font.pixelSize: 24; font.weight: Font.Bold; color: Ui.blue }
            Label { text: qsTr("Rezepte & Nährwerte · Version %1").arg(app.version); opacity: 0.8 }
            Label { text: "© 2026 Michael Bergmann"; opacity: 0.8 }
        }
    }
    Label {
        Layout.fillWidth: true
        wrapMode: Label.WordWrap
        text: qsTr("ViviaMeals verwaltet Rezepte, rechnet Kalorien und Nährwerte aus den Zutaten und hilft beim Planen, Einkaufen und Führen eines Ernährungstagebuchs.")
    }
    Card {
        Layout.fillWidth: true
        Label {
            width: parent.width
            wrapMode: Label.WordWrap
            text: qsTr("ViviaMeals ist kein Medizinprodukt. Alle Werte sind Richtwerte aus einer allgemeinen Nährwerttabelle und ersetzen keine ärztliche oder ernährungsfachliche Beratung.")
        }
    }
    Label {
        Layout.fillWidth: true
        wrapMode: Label.WordWrap
        textFormat: Text.StyledText
        text: qsTr("Rezepte bequem erfassen, scannen und ganze Sammlungen übernehmen: <a href=\"%1\">ViviaMeals für Windows, Linux und Mac</a>.").arg(app.productUrl)
        onLinkActivated: (link) => Qt.openUrlExternally(link)
        linkColor: Ui.blue
    }

    SectionLabel { text: qsTr("Lizenzen") }
    Label {
        Layout.fillWidth: true
        wrapMode: Label.WordWrap
        textFormat: Text.StyledText
        linkColor: Ui.blue
        onLinkActivated: (link) => Qt.openUrlExternally(link)
        text: qsTr("ViviaMeals steht unter der PolyForm Noncommercial License 1.0.0. Die mitgelieferten Beispielrezepte sind eigens für ViviaMeals geschrieben.<br><br>"
                 + "Diese App verwendet das Qt-Framework (Qt 6) unter der GNU Lesser General Public License v3. "
                 + "Quelltext und Lizenz von Qt: <a href=\"https://www.qt.io/licensing/open-source-lgpl-obligations\">qt.io</a>. "
                 + "Für die verschlüsselte Verbindung beim Abgleich wird OpenSSL (Apache License 2.0) verwendet.")
    }
    Button {
        text: qsTr("Lizenztext von ViviaMeals anzeigen")
        flat: true
        onClicked: Ui.main.push("LicensePage.qml")
    }
}
