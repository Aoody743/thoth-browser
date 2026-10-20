; Thoth Browser (Windows x64) installer.
; Built with NSIS 3.x (zlib/libpng license). No UPX or other packers are used,
; so the payload stays a plain, inspectable executable layout.

Unicode true

!include "MUI2.nsh"
!include "FileFunc.nsh"

!ifndef SOURCE_DIR
  !define SOURCE_DIR "..\ThothBrowser-win64"
!endif
!ifndef OUT_FILE
  !define OUT_FILE "ThothBrowser-0.9.1-win64-setup.exe"
!endif
!ifndef ICON_FILE
  !define ICON_FILE "..\installer_icon.ico"
!endif

!define APP_NAME "Thoth Browser"
!define APP_VERSION "0.9.1"
!define APP_DISPLAY_VERSION "0.9.1"
!define APP_PUBLISHER "Thoth Browser contributors"
!define APP_URL "https://github.com/Aoody743/thoth-browser"
!define UNINST_KEY "Software\Microsoft\Windows\CurrentVersion\Uninstall\ThothBrowser"

Name "${APP_NAME} ${APP_DISPLAY_VERSION}"
BrandingText "${APP_NAME} ${APP_DISPLAY_VERSION}"
OutFile "${OUT_FILE}"
InstallDir "$LOCALAPPDATA\Programs\Thoth Browser"
InstallDirRegKey HKCU "Software\ThothBrowser" "InstallDir"
RequestExecutionLevel user
SetCompressor /SOLID lzma
SetCompressorDictSize 16

VIProductVersion "0.9.1.0"
VIAddVersionKey "ProductName" "${APP_NAME}"
VIAddVersionKey "ProductVersion" "${APP_DISPLAY_VERSION}"
VIAddVersionKey "FileDescription" "${APP_NAME} installer"
VIAddVersionKey "FileVersion" "${APP_VERSION}"
VIAddVersionKey "CompanyName" "${APP_PUBLISHER}"
VIAddVersionKey "LegalCopyright" "BSD 2-Clause License"

!define MUI_ICON "${ICON_FILE}"
!define MUI_UNICON "${ICON_FILE}"
!define MUI_ABORTWARNING

!insertmacro MUI_PAGE_WELCOME
!insertmacro MUI_PAGE_LICENSE "..\..\LICENSE"
!insertmacro MUI_PAGE_DIRECTORY
!insertmacro MUI_PAGE_INSTFILES

!define MUI_FINISHPAGE_RUN "$INSTDIR\bin\Thoth.exe"
!define MUI_FINISHPAGE_RUN_TEXT "Launch ${APP_NAME}"
!insertmacro MUI_PAGE_FINISH

!insertmacro MUI_UNPAGE_CONFIRM
!insertmacro MUI_UNPAGE_INSTFILES

!insertmacro MUI_LANGUAGE "English"
!insertmacro MUI_LANGUAGE "SimpChinese"

Section "Thoth Browser" SEC_MAIN
  SectionIn RO
  SetOutPath "$INSTDIR"
  File /r "${SOURCE_DIR}\*.*"

  CreateDirectory "$SMPROGRAMS\Thoth Browser"
  CreateShortcut "$SMPROGRAMS\Thoth Browser\Thoth Browser.lnk" "$INSTDIR\bin\Thoth.exe"
  CreateShortcut "$SMPROGRAMS\Thoth Browser\Uninstall Thoth Browser.lnk" "$INSTDIR\Uninstall.exe"

  WriteRegStr HKCU "Software\ThothBrowser" "InstallDir" "$INSTDIR"
  WriteUninstaller "$INSTDIR\Uninstall.exe"

  WriteRegStr HKCU "${UNINST_KEY}" "DisplayName" "${APP_NAME} ${APP_DISPLAY_VERSION}"
  WriteRegStr HKCU "${UNINST_KEY}" "DisplayVersion" "${APP_VERSION}"
  WriteRegStr HKCU "${UNINST_KEY}" "Publisher" "${APP_PUBLISHER}"
  WriteRegStr HKCU "${UNINST_KEY}" "URLInfoAbout" "${APP_URL}"
  WriteRegStr HKCU "${UNINST_KEY}" "DisplayIcon" "$INSTDIR\bin\Thoth.exe"
  WriteRegStr HKCU "${UNINST_KEY}" "UninstallString" '"$INSTDIR\Uninstall.exe"'
  WriteRegStr HKCU "${UNINST_KEY}" "QuietUninstallString" '"$INSTDIR\Uninstall.exe" /S'
  WriteRegDWORD HKCU "${UNINST_KEY}" "NoModify" 1
  WriteRegDWORD HKCU "${UNINST_KEY}" "NoRepair" 1
  ${GetSize} "$INSTDIR" "/S=0K" $0 $1 $2
  IntFmt $0 "0x%08X" $0
  WriteRegDWORD HKCU "${UNINST_KEY}" "EstimatedSize" "$0"
SectionEnd

Section "Desktop shortcut" SEC_DESKTOP
  CreateShortcut "$DESKTOP\Thoth Browser.lnk" "$INSTDIR\bin\Thoth.exe"
SectionEnd

Section "Uninstall"
  Delete "$DESKTOP\Thoth Browser.lnk"
  Delete "$SMPROGRAMS\Thoth Browser\Thoth Browser.lnk"
  Delete "$SMPROGRAMS\Thoth Browser\Uninstall Thoth Browser.lnk"
  RMDir "$SMPROGRAMS\Thoth Browser"

  RMDir /r "$INSTDIR"
  DeleteRegKey HKCU "${UNINST_KEY}"
  DeleteRegKey HKCU "Software\ThothBrowser"
SectionEnd


