// ViviaMeals – Verknüpfungen und Dateizuordnung
//
//  Startmenü:  ViviaMeals, ViviaMeals – Demo (--demo), „ViviaMeals aktualisieren oder entfernen“
//  Desktop:    ViviaMeals
//  Dateien:    .vmr (ViviaMeals-Rezept) öffnet ViviaMeals und übernimmt das Rezept in die geöffnete
//              Rezeptdatenbank; .vmdb (Kennungsdatei einer Rezeptdatenbank) öffnet die Datenbank;
//              .vmpack (Rezeptpaket) bietet das Übernehmen bzw. Anlegen als neue Datenbank an.
//              Die Zuordnungen werden bei der Deinstallation wieder entfernt.
function Component()
{
}

Component.prototype.createOperations = function()
{
    component.createOperations();
    if (systemInfo.productType !== "windows")
        return;

    var exe = "@TargetDir@/ViviaMeals.exe";
    component.addOperation("CreateShortcut", exe, "@StartMenuDir@/ViviaMeals.lnk",
        "workingDirectory=@TargetDir@", "iconPath=" + exe, "iconId=0",
        "description=Rezeptverwaltung mit Nährwertanalyse und Gewichtskontrolle");
    component.addOperation("CreateShortcut", exe, "@StartMenuDir@/ViviaMeals - Demo.lnk", "--demo",
        "workingDirectory=@TargetDir@", "iconPath=" + exe, "iconId=0",
        "description=Demo mit Beispielrezepten (berührt die eigenen Daten nicht)");
    component.addOperation("CreateShortcut", "@TargetDir@/ViviaMeals-Wartung.exe",
        "@StartMenuDir@/ViviaMeals aktualisieren oder entfernen.lnk",
        "workingDirectory=@TargetDir@", "description=ViviaMeals aktualisieren oder deinstallieren");
    component.addOperation("CreateShortcut", exe, "@DesktopDir@/ViviaMeals.lnk",
        "workingDirectory=@TargetDir@", "iconPath=" + exe, "iconId=0");

    // Dateizuordnungen (Befehl in Windows-Schreibweise, Datei in Anführungszeichen)
    var cmd = "\"@TargetDir@\\ViviaMeals.exe\" \"%1\"";
    component.addOperation("RegisterFileType", "vmr", cmd, "ViviaMeals-Rezept",
        "application/x-viviameals-recipe", "@TargetDir@\\ViviaMeals.exe,0", "ProgId=mbergmann.ViviaMeals.vmr");
    component.addOperation("RegisterFileType", "vmdb", cmd, "ViviaMeals-Rezeptdatenbank",
        "application/x-viviameals-database", "@TargetDir@\\ViviaMeals.exe,0", "ProgId=mbergmann.ViviaMeals.vmdb");
    component.addOperation("RegisterFileType", "vmpack", cmd, "ViviaMeals-Rezeptpaket",
        "application/x-viviameals-package", "@TargetDir@\\ViviaMeals.exe,0", "ProgId=mbergmann.ViviaMeals.vmpack");
}
