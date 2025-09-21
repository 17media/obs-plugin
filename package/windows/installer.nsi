; 17Live OBS Plugin NSIS Installer Script
; Based on the configuration from GitHub Actions workflow

!define PRODUCT_NAME "17Live OBS Plugin"
!define PRODUCT_VERSION "1.0.0"  ; This will be replaced by build script
!define PRODUCT_PUBLISHER "17Live Limited"
!define PRODUCT_WEB_SITE "https://17.live"
!define PACKAGE_ID "OneSevenLive.ObsPlugin"
!define PACKAGE_NAME "OneSevenLiveOBSPlugin"
!define UPGRADE_CODE "b81d9705-e5b2-475f-8de8-4d02d297c073"
!define PRODUCT_DIR_REGKEY "Software\Microsoft\Windows\CurrentVersion\App Paths\obs-17live-plugin"
!define PRODUCT_UNINST_KEY "Software\Microsoft\Windows\CurrentVersion\Uninstall\17LiveOBSPlugin"
!define PRODUCT_UNINST_ROOT_KEY "HKLM"

; Modern UI
!include "MUI2.nsh"
!include "FileFunc.nsh"

; Compression
SetCompressor lzma

; General
Name "${PRODUCT_NAME}"
OutFile "17liveOBSPlugin-windows-v${PRODUCT_VERSION}.exe"
InstallDir "C:\Program Files\obs-studio"
InstallDirRegKey HKLM "Software\OBS Studio" ""
ShowInstDetails show
ShowUnInstDetails show
RequestExecutionLevel admin

; Interface Settings
!define MUI_ABORTWARNING
!define MUI_ICON "${NSISDIR}\Contrib\Graphics\Icons\modern-install.ico"
!define MUI_UNICON "${NSISDIR}\Contrib\Graphics\Icons\modern-uninstall.ico"

; Welcome page
!insertmacro MUI_PAGE_WELCOME

; License page
!insertmacro MUI_PAGE_LICENSE "..\..\LICENSE"

; Directory page
!insertmacro MUI_PAGE_DIRECTORY

; Instfiles page
!insertmacro MUI_PAGE_INSTFILES

; Finish page
; Removed automatic OBS launch to avoid issues with incorrect OBS paths
; !define MUI_FINISHPAGE_RUN_TEXT "Launch OBS Studio"
; !define MUI_FINISHPAGE_RUN "$INSTDIR\bin\64bit\obs64.exe"
!define MUI_FINISHPAGE_SHOWREADME_TEXT "View Release Notes"
!define MUI_FINISHPAGE_SHOWREADME "$INSTDIR\data\obs-plugins\obs-17live\README.txt"
!insertmacro MUI_PAGE_FINISH

; Uninstaller pages
!insertmacro MUI_UNPAGE_INSTFILES

; Language files
!insertmacro MUI_LANGUAGE "English"
!insertmacro MUI_LANGUAGE "Japanese"
!insertmacro MUI_LANGUAGE "TradChinese"

; Version Information
VIProductVersion "${PRODUCT_VERSION}.0"
VIAddVersionKey /LANG=${LANG_ENGLISH} "ProductName" "${PRODUCT_NAME}"
VIAddVersionKey /LANG=${LANG_ENGLISH} "Comments" "17Live OBS Plugin for streaming integration"
VIAddVersionKey /LANG=${LANG_ENGLISH} "CompanyName" "${PRODUCT_PUBLISHER}"
VIAddVersionKey /LANG=${LANG_ENGLISH} "LegalCopyright" "© 2024 ${PRODUCT_PUBLISHER}"
VIAddVersionKey /LANG=${LANG_ENGLISH} "FileDescription" "${PRODUCT_NAME} Installer"
VIAddVersionKey /LANG=${LANG_ENGLISH} "FileVersion" "${PRODUCT_VERSION}"
VIAddVersionKey /LANG=${LANG_ENGLISH} "ProductVersion" "${PRODUCT_VERSION}"

; Reserve Files
!insertmacro MUI_RESERVEFILE_LANGDLL

; Installer Sections
Section "MainSection" SEC01
  ; Check if OBS Studio exists in the selected installation directory
  ; This supports both installed and portable versions of OBS Studio
  
  ; Check for OBS Studio executable in the selected directory
  IfFileExists "$INSTDIR\bin\64bit\obs64.exe" obs_found_in_instdir check_registry
  
  check_registry:
    ; If not found in selected directory, try to find from registry (for auto-detection)
    ReadRegStr $0 HKLM "Software\OBS Studio" ""
    StrCmp $0 "" try_wow6432 registry_found
    
    try_wow6432:
      ; Try to read from WOW6432Node (32-bit apps on 64-bit system)
      ReadRegStr $0 HKLM "Software\WOW6432Node\OBS Studio" ""
      StrCmp $0 "" obs_not_found registry_found
    
    registry_found:
      ; Update INSTDIR to the path found in registry
      StrCpy $INSTDIR $0
      IfFileExists "$INSTDIR\bin\64bit\obs64.exe" obs_found_in_registry obs_not_found
    
    obs_not_found:
      MessageBox MB_YESNO|MB_ICONQUESTION "OBS Studio not found in the selected directory ($INSTDIR).$\n$\nThis plugin requires OBS Studio to be installed.$\nDo you want to continue anyway?" IDYES continue_install
      Abort
    
    continue_install:
    obs_found_in_instdir:
    obs_found_in_registry:
  
  SetOutPath "$INSTDIR"
  SetOverwrite ifnewer
  
  ; Install plugin DLL and PDB files to obs-plugins\64bit
  SetOutPath "$INSTDIR\obs-plugins\64bit"
  File "..\..\build_x64\rundir\Release\obs-17live.dll"
  File "..\..\build_x64\rundir\Release\obs-17live.pdb"
  
  ; Install data directory to data\obs-plugins
  SetOutPath "$INSTDIR\data\obs-plugins"
  File /r "..\..\build_x64\rundir\Release\obs-17live"
  
  ; Create README file
  SetOutPath "$INSTDIR\data\obs-plugins\obs-17live"
  FileOpen $0 "$INSTDIR\data\obs-plugins\obs-17live\README.txt" w
  FileWrite $0 "17Live OBS Plugin v${PRODUCT_VERSION}$\r$\n"
  FileWrite $0 "================================$\r$\n$\r$\n"
  FileWrite $0 "Thank you for installing the 17Live OBS Plugin!$\r$\n$\r$\n"
  FileWrite $0 "This plugin enables seamless integration between OBS Studio and 17Live streaming platform.$\r$\n$\r$\n"
  FileWrite $0 "For support and documentation, visit: ${PRODUCT_WEB_SITE}$\r$\n$\r$\n"
  FileWrite $0 "Installation completed successfully.$\r$\n"
  FileClose $0
  
  ; Create start menu shortcuts
  CreateDirectory "$SMPROGRAMS\17Live OBS Plugin"
  CreateShortCut "$SMPROGRAMS\17Live OBS Plugin\17Live OBS Plugin.lnk" "$INSTDIR\bin\64bit\obs64.exe"
  
  ; Write registry entries
  WriteRegStr HKLM "Software\17Live\OBSPlugin" "InstallPath" "$INSTDIR"
  WriteRegStr HKLM "Software\17Live\OBSPlugin" "Version" "${PRODUCT_VERSION}"
  WriteRegStr HKLM "Software\17Live\OBSPlugin" "PackageId" "${PACKAGE_ID}"
  WriteRegStr HKLM "Software\17Live\OBSPlugin" "PackageName" "${PACKAGE_NAME}"
  WriteRegStr HKLM "Software\17Live\OBSPlugin" "Manufacturer" "${PRODUCT_PUBLISHER}"
  WriteRegStr HKLM "Software\17Live\OBSPlugin" "UpgradeCode" "${UPGRADE_CODE}"
SectionEnd

Section -AdditionalIcons
  WriteIniStr "$INSTDIR\17live.url" "InternetShortcut" "URL" "${PRODUCT_WEB_SITE}"
  CreateShortCut "$SMPROGRAMS\17Live OBS Plugin\Website.lnk" "$INSTDIR\17live.url"
  CreateShortCut "$SMPROGRAMS\17Live OBS Plugin\Uninstall.lnk" "$INSTDIR\uninst.exe"
SectionEnd

Section -Post
  WriteUninstaller "$INSTDIR\uninst.exe"
  WriteRegStr HKLM "${PRODUCT_DIR_REGKEY}" "" "$INSTDIR\obs-17live-plugin.exe"
  WriteRegStr ${PRODUCT_UNINST_ROOT_KEY} "${PRODUCT_UNINST_KEY}" "DisplayName" "${PRODUCT_NAME}"
  WriteRegStr ${PRODUCT_UNINST_ROOT_KEY} "${PRODUCT_UNINST_KEY}" "UninstallString" "$INSTDIR\uninst.exe"
  WriteRegStr ${PRODUCT_UNINST_ROOT_KEY} "${PRODUCT_UNINST_KEY}" "DisplayVersion" "${PRODUCT_VERSION}"
  WriteRegStr ${PRODUCT_UNINST_ROOT_KEY} "${PRODUCT_UNINST_KEY}" "URLInfoAbout" "${PRODUCT_WEB_SITE}"
  WriteRegStr ${PRODUCT_UNINST_ROOT_KEY} "${PRODUCT_UNINST_KEY}" "Publisher" "${PRODUCT_PUBLISHER}"
  WriteRegStr ${PRODUCT_UNINST_ROOT_KEY} "${PRODUCT_UNINST_KEY}" "PackageId" "${PACKAGE_ID}"
  WriteRegStr ${PRODUCT_UNINST_ROOT_KEY} "${PRODUCT_UNINST_KEY}" "PackageName" "${PACKAGE_NAME}"
  WriteRegStr ${PRODUCT_UNINST_ROOT_KEY} "${PRODUCT_UNINST_KEY}" "UpgradeCode" "${UPGRADE_CODE}"
  
  ; Calculate and write install size
  ${GetSize} "$INSTDIR\obs-plugins\64bit" "/S=0K" $0 $1 $2
  ${GetSize} "$INSTDIR\data\obs-plugins\obs-17live" "/S=0K" $3 $1 $2
  IntOp $0 $0 + $3
  WriteRegDWORD ${PRODUCT_UNINST_ROOT_KEY} "${PRODUCT_UNINST_KEY}" "EstimatedSize" $0
SectionEnd

; Uninstaller Section
Section Uninstall
  ; Remove plugin files
  Delete "$INSTDIR\obs-plugins\64bit\obs-17live.dll"
  Delete "$INSTDIR\obs-plugins\64bit\obs-17live.pdb"
  RMDir /r "$INSTDIR\data\obs-plugins\obs-17live"
  
  ; Remove shortcuts and registry entries
  Delete "$INSTDIR\17live.url"
  Delete "$INSTDIR\uninst.exe"
  Delete "$SMPROGRAMS\17Live OBS Plugin\17Live OBS Plugin.lnk"
  Delete "$SMPROGRAMS\17Live OBS Plugin\Uninstall.lnk"
  Delete "$SMPROGRAMS\17Live OBS Plugin\Website.lnk"
  RMDir "$SMPROGRAMS\17Live OBS Plugin"
  
  DeleteRegKey ${PRODUCT_UNINST_ROOT_KEY} "${PRODUCT_UNINST_KEY}"
  DeleteRegKey HKLM "${PRODUCT_DIR_REGKEY}"
  DeleteRegKey HKLM "Software\17Live\OBSPlugin"
  
  SetAutoClose true
SectionEnd

; Installer Functions
Function .onInit
  !insertmacro MUI_LANGDLL_DISPLAY
  
  ; Check for existing installation
  ReadRegStr $R0 ${PRODUCT_UNINST_ROOT_KEY} "${PRODUCT_UNINST_KEY}" "UninstallString"
  StrCmp $R0 "" done
  
  MessageBox MB_OKCANCEL|MB_ICONEXCLAMATION \
  "${PRODUCT_NAME} is already installed. $\n$\nClick `OK` to remove the \
  previous version or `Cancel` to cancel this upgrade." \
  IDOK uninst
  Abort
  
  uninst:
    ClearErrors
    ExecWait '$R0 _?=$INSTDIR'
    
    IfErrors no_remove_uninstaller done
    no_remove_uninstaller:
  
  done:
FunctionEnd

Function un.onInit
  !insertmacro MUI_UNGETLANGUAGE
  MessageBox MB_ICONQUESTION|MB_YESNO|MB_DEFBUTTON2 "Are you sure you want to completely remove $(^Name) and all of its components?" IDYES +2
  Abort
FunctionEnd

Function un.onUninstSuccess
  HideWindow
  MessageBox MB_ICONINFORMATION|MB_OK "$(^Name) was successfully removed from your computer."
FunctionEnd