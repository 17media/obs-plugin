; 17Live OBS Plugin NSIS Configuration
; This file contains common configuration and constants for the NSIS installer

; Product Information
!ifndef PRODUCT_NAME
  !define PRODUCT_NAME "17Live OBS Plugin"
!endif

!ifndef PRODUCT_VERSION
  !define PRODUCT_VERSION "1.0.0"
!endif

!ifndef PRODUCT_PUBLISHER
  !define PRODUCT_PUBLISHER "17Live Limited"
!endif

!ifndef PRODUCT_WEB_SITE
  !define PRODUCT_WEB_SITE "https://17.live"
!endif

; Package Identifiers (consistent with WiX configuration)
!ifndef PACKAGE_ID
  !define PACKAGE_ID "OneSevenLive.ObsPlugin"
!endif

!ifndef PACKAGE_NAME
  !define PACKAGE_NAME "OneSevenLiveOBSPlugin"
!endif

!ifndef UPGRADE_CODE
  !define UPGRADE_CODE "b81d9705-e5b2-475f-8de8-4d02d297c073"
!endif

; Registry Keys
!ifndef PRODUCT_DIR_REGKEY
  !define PRODUCT_DIR_REGKEY "Software\Microsoft\Windows\CurrentVersion\App Paths\obs-17live-plugin"
!endif

!ifndef PRODUCT_UNINST_KEY
  !define PRODUCT_UNINST_KEY "Software\Microsoft\Windows\CurrentVersion\Uninstall\17LiveOBSPlugin"
!endif

!ifndef PRODUCT_UNINST_ROOT_KEY
  !define PRODUCT_UNINST_ROOT_KEY "HKLM"
!endif

; Build Configuration
!ifndef BUILD_DIR
  !define BUILD_DIR "..\..\build_x64\rundir\Release"
!endif

!ifndef OUTPUT_NAME
  !define OUTPUT_NAME "17liveOBSPlugin-windows-v${PRODUCT_VERSION}.exe"
!endif

; OBS Studio Default Installation Paths
!ifndef OBS_DEFAULT_INSTALL_DIR
  !define OBS_DEFAULT_INSTALL_DIR "C:\Program Files\obs-studio"
!endif

!ifndef OBS_DEFAULT_INSTALL_DIR_X86
  !define OBS_DEFAULT_INSTALL_DIR_X86 "C:\Program Files (x86)\obs-studio"
!endif

; Plugin Installation Paths (relative to OBS installation)
!ifndef PLUGIN_DLL_PATH
  !define PLUGIN_DLL_PATH "obs-plugins\64bit"
!endif

!ifndef PLUGIN_DATA_PATH
  !define PLUGIN_DATA_PATH "data\obs-plugins"
!endif

; Installer Configuration
!ifndef INSTALLER_COMPRESSION
  !define INSTALLER_COMPRESSION "lzma"
!endif

!ifndef INSTALLER_ICON
  !define INSTALLER_ICON "${NSISDIR}\Contrib\Graphics\Icons\modern-install.ico"
!endif

!ifndef UNINSTALLER_ICON
  !define UNINSTALLER_ICON "${NSISDIR}\Contrib\Graphics\Icons\modern-uninstall.ico"
!endif

; License File
!ifndef LICENSE_FILE
  !define LICENSE_FILE "..\..\LICENSE"
!endif

; Supported Languages
!ifndef SUPPORTED_LANGUAGES
  !define SUPPORTED_LANGUAGES "English Japanese TradChinese"
!endif

; Version Information for Executable
!ifndef VI_PRODUCT_VERSION
  !define VI_PRODUCT_VERSION "${PRODUCT_VERSION}.0"
!endif

; Macros for common operations
!macro CheckOBSInstallation
  ReadRegStr $0 HKLM "Software\OBS Studio" "InstallPath"
  StrCmp $0 "" 0 +3
  ReadRegStr $0 HKLM "Software\Wow6432Node\OBS Studio" "InstallPath"
  StrCmp $0 "" obs_not_found obs_found
!macroend

!macro CreatePluginShortcuts
  CreateDirectory "$SMPROGRAMS\17Live OBS Plugin"
  CreateShortCut "$SMPROGRAMS\17Live OBS Plugin\17Live OBS Plugin.lnk" "$INSTDIR\bin\64bit\obs64.exe"
  CreateShortCut "$SMPROGRAMS\17Live OBS Plugin\Website.lnk" "${PRODUCT_WEB_SITE}"
  CreateShortCut "$SMPROGRAMS\17Live OBS Plugin\Uninstall.lnk" "$INSTDIR\uninst.exe"
!macroend

!macro RemovePluginShortcuts
  Delete "$SMPROGRAMS\17Live OBS Plugin\17Live OBS Plugin.lnk"
  Delete "$SMPROGRAMS\17Live OBS Plugin\Website.lnk"
  Delete "$SMPROGRAMS\17Live OBS Plugin\Uninstall.lnk"
  RMDir "$SMPROGRAMS\17Live OBS Plugin"
!macroend

!macro WriteRegistryEntries
  WriteRegStr HKLM "Software\17Live\OBSPlugin" "InstallPath" "$INSTDIR"
  WriteRegStr HKLM "Software\17Live\OBSPlugin" "Version" "${PRODUCT_VERSION}"
  WriteRegStr HKLM "Software\17Live\OBSPlugin" "PackageId" "${PACKAGE_ID}"
  WriteRegStr HKLM "Software\17Live\OBSPlugin" "PackageName" "${PACKAGE_NAME}"
  WriteRegStr HKLM "Software\17Live\OBSPlugin" "Manufacturer" "${PRODUCT_PUBLISHER}"
  WriteRegStr HKLM "Software\17Live\OBSPlugin" "UpgradeCode" "${UPGRADE_CODE}"
!macroend

!macro RemoveRegistryEntries
  DeleteRegKey HKLM "Software\17Live\OBSPlugin"
  DeleteRegKey ${PRODUCT_UNINST_ROOT_KEY} "${PRODUCT_UNINST_KEY}"
  DeleteRegKey HKLM "${PRODUCT_DIR_REGKEY}"
!macroend