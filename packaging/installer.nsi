; MasterLens Windows installer (NSIS)
Unicode true
!include "MUI2.nsh"

!ifndef VERSION
  !define VERSION "1.2.0"
!endif
!ifndef ARCH
  !define ARCH "x64"
!endif
!ifndef STAGE
  !define STAGE "stage-x64"
!endif

Name "MasterLens ${VERSION} (${ARCH})"
OutFile "MasterLens-${VERSION}-Windows-${ARCH}.exe"
InstallDir "$PROGRAMFILES64\MasterLens"
RequestExecutionLevel admin

!insertmacro MUI_PAGE_WELCOME
!insertmacro MUI_PAGE_DIRECTORY
!insertmacro MUI_PAGE_INSTFILES
!insertmacro MUI_PAGE_FINISH

!insertmacro MUI_LANGUAGE "English"
!insertmacro MUI_LANGUAGE "SimpChinese"

Section "VST3 + Standalone"
  SetOutPath "$COMMONPROGRAMFILES64\VST3\MasterLens.vst3"
  File /r "${STAGE}\MasterLens.vst3\*.*"

  SetOutPath "$INSTDIR"
  File "${STAGE}\MasterLens.exe"
SectionEnd

Section "Uninstall"
  Delete "$COMMONPROGRAMFILES64\VST3\MasterLens.vst3\*.*"
  RMDir /r "$COMMONPROGRAMFILES64\VST3\MasterLens.vst3"
  Delete "$INSTDIR\MasterLens.exe"
  RMDir "$INSTDIR"
SectionEnd
