// ViviaMealsCheat – Steuerskript
// Die Standardseite „Zielordner“ ist ausgeblendet: Der Ordner von ViviaMeals wird auf der eigenen Seite
// „ViviaMeals-Ordner“ gewählt (Komponentenskript); TargetDir wird daraus abgeleitet.
// Ist ViviaMealsCheat dort schon installiert, wird die alte Fassung vorher entfernt.

var kTool = "ViviaMealsCheat-Deinstallation.exe";

function Controller()
{
    if (installer.isInstaller())
        installer.setDefaultPageVisible(QInstaller.TargetDirectory, false);
}

Controller.prototype.ReadyForInstallationPageCallback = function()
{
    if (!installer.isInstaller())
        return;
    var dir = installer.value("TargetDir");
    var tool = dir + "/" + kTool;
    if (!installer.fileExists(tool))
        return;
    var answer = QMessageBox.question("viviamealscheat.update", "ViviaMealsCheat",
                                      "ViviaMealsCheat ist hier bereits installiert. Die vorhandene Fassung wird jetzt entfernt "
                                      + "und anschließend die neue installiert.",
                                      QMessageBox.Ok | QMessageBox.Cancel);
    if (answer !== QMessageBox.Ok) {
        installer.setCanceled();
        return;
    }
    installer.execute(tool, ["purge", "-c"]);
}
