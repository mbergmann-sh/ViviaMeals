// VitaMeals – Verknüpfungen und Dateizuordnung
//
//  Startmenü:  VitaMeals, VitaMeals – Demo (--demo), „VitaMeals aktualisieren oder entfernen“
//  Desktop:    VitaMeals
//  Dateien:    .vmr (VitaMeals-Rezept) öffnet VitaMeals und übernimmt das Rezept in die geöffnete
//              Rezeptdatenbank; .vmdb (Kennungsdatei einer Rezeptdatenbank) öffnet die Datenbank.
//              Die Zuordnungen werden bei der Deinstallation wieder entfernt.
function Component()
{
}

Component.prototype.createOperations = function()
{
    component.createOperations();
    if (systemInfo.productType !== "windows")
        return;

    var exe = "@TargetDir@/VitaMeals.exe";
    component.addOperation("CreateShortcut", exe, "@StartMenuDir@/VitaMeals.lnk",
        "workingDirectory=@TargetDir@", "iconPath=" + exe, "iconId=0",
        "description=Rezeptverwaltung mit Nährwertanalyse und Gewichtskontrolle");
    component.addOperation("CreateShortcut", exe, "@StartMenuDir@/VitaMeals - Demo.lnk", "--demo",
        "workingDirectory=@TargetDir@", "iconPath=" + exe, "iconId=0",
        "description=Demo mit Beispielrezepten (berührt die eigenen Daten nicht)");
    component.addOperation("CreateShortcut", "@TargetDir@/VitaMeals-Wartung.exe",
        "@StartMenuDir@/VitaMeals aktualisieren oder entfernen.lnk",
        "workingDirectory=@TargetDir@", "description=VitaMeals aktualisieren oder deinstallieren");
    component.addOperation("CreateShortcut", exe, "@DesktopDir@/VitaMeals.lnk",
        "workingDirectory=@TargetDir@", "iconPath=" + exe, "iconId=0");

    // Dateizuordnungen (Befehl in Windows-Schreibweise, Datei in Anführungszeichen)
    var cmd = "\"@TargetDir@\\VitaMeals.exe\" \"%1\"";
    component.addOperation("RegisterFileType", "vmr", cmd, "VitaMeals-Rezept",
        "application/x-vitameals-recipe", "@TargetDir@\\VitaMeals.exe,0", "ProgId=mbergmann.VitaMeals.vmr");
    component.addOperation("RegisterFileType", "vmdb", cmd, "VitaMeals-Rezeptdatenbank",
        "application/x-vitameals-database", "@TargetDir@\\VitaMeals.exe,0", "ProgId=mbergmann.VitaMeals.vmdb");
}
