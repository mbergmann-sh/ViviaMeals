// VitaMeals – Steuerskript für den Qt-Installer-Framework-Installer
//
// Aktualisieren mit einem neuen Offline-Installer: Ist VitaMeals schon im Zielordner installiert,
// wird die alte Version vor der Installation über ihr Wartungsprogramm entfernt ("purge") und
// die neue in denselben Ordner installiert. Rezeptdatenbanken (eigene Ordner), Profile, eigene
// Nährwerte und Einstellungen liegen außerhalb des Programmordners und bleiben erhalten.

var kTool = "VitaMeals-Wartung.exe";

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
    var tool = previousTool();
    if (tool === "")
        return;
    var dir = installer.value("TargetDir");
    var text = "VitaMeals ist bereits in " + dir + " installiert.\n"
             + "Die vorhandene Version wird jetzt entfernt und anschließend die neue installiert. "
             + "Ihre Rezeptdatenbanken, Profile und Einstellungen bleiben erhalten.";
    var answer = QMessageBox.question("vitameals.update", "VitaMeals", text,
                                      QMessageBox.Ok | QMessageBox.Cancel);
    if (answer !== QMessageBox.Ok) {
        installer.setCanceled();
        return;
    }
    installer.execute(tool, ["purge", "-c"]);
    installer.setValue("TargetDir", dir);
}
