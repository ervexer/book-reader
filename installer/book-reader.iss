; Скрипт Inno Setup для программного средства чтения электронных книг
; Открыть в Inno Setup Compiler (File -> Open) и нажать Build -> Compile (зелёная кнопка).
; Готовый установщик появится в installer\Output\BookReaderSetup.exe

#define MyAppName "Чтение книг"
#define MyAppVersion "1.0"
#define MyAppPublisher "Book Reader"
#define MyAppExeName "book project.exe"

[Setup]
AppId={{B8F3A2C1-9D4E-4F6A-8B2C-1E5D7A9C3F40}
AppName={#MyAppName}
AppVersion={#MyAppVersion}
AppPublisher={#MyAppPublisher}
; Ставим в пользовательскую папку (без прав администратора),
; чтобы программа могла создавать базу library.db рядом с собой.
PrivilegesRequired=lowest
DefaultDirName={autopf}\BookReader
DefaultGroupName={#MyAppName}
DisableProgramGroupPage=yes
OutputDir=Output
OutputBaseFilename=BookReaderSetup
Compression=lzma2
SolidCompression=yes
WizardStyle=modern

[Languages]
Name: "russian"; MessagesFile: "compiler:Languages\Russian.isl"

[Tasks]
Name: "desktopicon"; Description: "{cm:CreateDesktopIcon}"; GroupDescription: "{cm:AdditionalIcons}"; Flags: unchecked

[Files]
; Всё содержимое подготовленной папки deploy (exe, DLL Qt, плагины, image icon, рантайм)
Source: "..\deploy\*"; DestDir: "{app}"; Flags: recursesubdirs createallsubdirs ignoreversion

[Icons]
Name: "{group}\{#MyAppName}"; Filename: "{app}\{#MyAppExeName}"
Name: "{group}\{cm:UninstallProgram,{#MyAppName}}"; Filename: "{uninstallexe}"
Name: "{autodesktop}\{#MyAppName}"; Filename: "{app}\{#MyAppExeName}"; Tasks: desktopicon

[Run]
Filename: "{app}\{#MyAppExeName}"; Description: "{cm:LaunchProgram,{#MyAppName}}"; Flags: nowait postinstall skipifsilent

[UninstallDelete]
Type: files; Name: "{app}\library.db"
