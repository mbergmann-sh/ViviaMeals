// ViviaMeals – Steuerskript für den Qt-Installer-Framework-Installer
//
// Aktualisieren mit einem neuen Offline-Installer: Ist ViviaMeals schon im Zielordner installiert,
// wird die alte Version vor der Installation über ihr Wartungsprogramm entfernt ("purge") und
// die neue in denselben Ordner installiert. Rezeptdatenbanken (eigene Ordner), Profile, eigene
// Nährwerte und Einstellungen liegen außerhalb des Programmordners und bleiben erhalten.

var kTool = "ViviaMeals-Wartung.exe";

function Controller()
{
}

// Vorhandene Installation im (Vorgabe-)Zielordner suchen. Den Uninstall-Schlüssel in der
// Registrierung kann das Skript nicht nutzen: IFW benennt ihn nach einer zufälligen UUID.
function previousTool()
{
    var dir = installer.value("TargetDir");
    if (installer.fileExists(dir + "/" + kTool))
        return dir + "/" + kTool;
    return "";
}

Controller.prototype.IntroductionPageCallback = function()
{
    if (!installer.isInstaller())
        return;
    // Vorgänger unter altem Namen (bis 0.5: „VitaMeals“ in C:\VitaMeals) – anbieten, ihn zu entfernen
    var legacy = "C:/VitaMeals/VitaMeals-Wartung.exe";
    if (installer.fileExists(legacy)) {
        var ltext = "Das Programm hieß bisher „VitaMeals“ und ist noch in C:\\VitaMeals installiert.\n"
                  + "Soll die alte Version jetzt entfernt werden? Ihre Rezeptdatenbanken, Profile, Tagebücher und "
                  + "Einstellungen bleiben erhalten und werden von ViviaMeals beim ersten Start übernommen.";
        var lanswer = QMessageBox.question("viviameals.legacy", "ViviaMeals", ltext, QMessageBox.Yes | QMessageBox.No);
        if (lanswer === QMessageBox.Yes)
            installer.execute(legacy, ["purge", "-c"]);
    }
    var tool = previousTool();
    if (tool === "")
        return;
    var dir = installer.value("TargetDir");
    var text = "ViviaMeals ist bereits in " + dir + " installiert.\n"
             + "Die vorhandene Version wird jetzt entfernt und anschließend die neue installiert. "
             + "Ihre Rezeptdatenbanken, Profile und Einstellungen bleiben erhalten.";
    var answer = QMessageBox.question("viviameals.update", "ViviaMeals", text,
                                      QMessageBox.Ok | QMessageBox.Cancel);
    if (answer !== QMessageBox.Ok) {
        installer.setCanceled();
        return;
    }
    installer.execute(tool, ["purge", "-c"]);
    installer.setValue("TargetDir", dir);
}
