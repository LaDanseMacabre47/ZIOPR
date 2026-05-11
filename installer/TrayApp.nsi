; ================================================================
;  TrayApp NSIS Installer
; ================================================================

!include "MUI2.nsh"
!include "LogicLib.nsh"

Name            "TrayApp Antivirus"
OutFile         "TrayApp-Setup.exe"
InstallDir      "$PROGRAMFILES64\TrayApp"
InstallDirRegKey HKLM "Software\TrayApp" "InstallDir"
RequestExecutionLevel admin
Unicode True

VIProductVersion "1.0.0.0"
VIAddVersionKey "ProductName"    "TrayApp Antivirus"
VIAddVersionKey "FileVersion"    "1.0.0.0"
VIAddVersionKey "LegalCopyright" "2025"

!define MUI_ABORTWARNING
!insertmacro MUI_PAGE_WELCOME
!insertmacro MUI_PAGE_DIRECTORY
!insertmacro MUI_PAGE_INSTFILES
!insertmacro MUI_PAGE_FINISH

!insertmacro MUI_UNPAGE_CONFIRM
!insertmacro MUI_UNPAGE_INSTFILES

!insertmacro MUI_LANGUAGE "Russian"

; ================================================================
;  УСТАНОВКА
; ================================================================
Section "Main" SecMain

    SetOutPath "$INSTDIR"

    ; Копируем исполняемые файлы
    File "bin\TrayService.exe"
    File "bin\TrayApp.exe"

    ; MSVC Runtime — устанавливаем только если не установлен
    ReadRegDword $0 HKLM \
        "SOFTWARE\Microsoft\VisualStudio\14.0\VC\Runtimes\x64" "Installed"
    ${If} $0 != 1
        SetOutPath "$INSTDIR\redist"
        File "redist\VC_redist.x64.exe"
        ExecWait '"$INSTDIR\redist\VC_redist.x64.exe" /install /quiet /norestart'
        ; Помечаем что мы его установили
        WriteRegDWORD HKLM \
            "Software\TrayApp" "RedistInstalledByUs" 1
        SetOutPath "$INSTDIR"
    ${EndIf}

    ; Записываем путь установки
    WriteRegStr HKLM "Software\TrayApp" "InstallDir" "$INSTDIR"

    ; Создаём деинсталлятор
    WriteUninstaller "$INSTDIR\Uninstall.exe"

    ; Добавляем в "Программы и компоненты"
    WriteRegStr HKLM \
        "Software\Microsoft\Windows\CurrentVersion\Uninstall\TrayApp" \
        "DisplayName" "TrayApp Antivirus"
    WriteRegStr HKLM \
        "Software\Microsoft\Windows\CurrentVersion\Uninstall\TrayApp" \
        "UninstallString" '"$INSTDIR\Uninstall.exe"'
    WriteRegStr HKLM \
        "Software\Microsoft\Windows\CurrentVersion\Uninstall\TrayApp" \
        "DisplayVersion" "1.0.0.0"
    WriteRegStr HKLM \
        "Software\Microsoft\Windows\CurrentVersion\Uninstall\TrayApp" \
        "Publisher" "TrayApp"
    WriteRegDWORD HKLM \
        "Software\Microsoft\Windows\CurrentVersion\Uninstall\TrayApp" \
        "NoModify" 1
    WriteRegDWORD HKLM \
        "Software\Microsoft\Windows\CurrentVersion\Uninstall\TrayApp" \
        "NoRepair" 1

    ; Останавливаем старую службу если есть
    ExecWait 'sc.exe stop TrayService'
    ExecWait 'sc.exe delete TrayService'
    Sleep 1000

    ; Регистрируем службу с автозапуском (тип auto)
    ExecWait 'sc.exe create TrayService binPath= "$INSTDIR\TrayService.exe" start= auto DisplayName= "TrayApp Service"'
    ExecWait 'sc.exe description TrayService "TrayApp Antivirus background service"'
    ExecWait 'sc.exe start TrayService'

SectionEnd

; ================================================================
;  УДАЛЕНИЕ
; ================================================================
Section "Uninstall"

    ; Останавливаем и удаляем службу
    ExecWait 'sc.exe stop TrayService'
    Sleep 2000
    ExecWait 'sc.exe delete TrayService'

    ; Завершаем процессы
    ExecWait 'taskkill /F /IM TrayApp.exe'
    ExecWait 'taskkill /F /IM TrayService.exe'
    Sleep 1000

    ; Удаляем MSVC Runtime только если мы его устанавливали
    ReadRegDword $0 HKLM "Software\TrayApp" "RedistInstalledByUs"
    ${If} $0 == 1
        IfFileExists "$INSTDIR\redist\VC_redist.x64.exe" 0 SkipRedist
            ExecWait '"$INSTDIR\redist\VC_redist.x64.exe" /uninstall /quiet /norestart'
        SkipRedist:
    ${EndIf}

    ; Удаляем файлы
    Delete "$INSTDIR\TrayService.exe"
    Delete "$INSTDIR\TrayApp.exe"
    Delete "$INSTDIR\redist\VC_redist.x64.exe"
    RMDir  "$INSTDIR\redist"
    Delete "$INSTDIR\Uninstall.exe"
    RMDir  "$INSTDIR"

    ; Удаляем записи реестра
    DeleteRegKey HKLM "Software\TrayApp"
    DeleteRegKey HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\TrayApp"

SectionEnd