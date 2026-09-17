; Windows installer for Thermavip.
;
; Driven entirely by the three defines the release workflow passes, so the
; script holds no version and no path of its own:
;
;   makensis -DVERSION=5.4.1 -DSRC=<absolute install prefix> \
;            -DOUT=<absolute output .exe> packaging/thermavip.nsi
;
; SRC and OUT are absolute on purpose: makensis resolves a relative File path
; against its current directory, which is not the directory of this script, and
; the two differ whenever it is invoked from the repository root.
;
; What is packaged is SRC\thermavip, the directory CMake installs the
; application into (THERMAVIP_APPLICATION_DIR), after windeployqt has copied the
; Qt runtime next to the executable. The SDK headers and import libraries under
; SRC\include and SRC\lib are not shipped: this is the application installer,
; not the SDK.

Unicode true

!ifndef VERSION
  !error "VERSION is not defined. Pass -DVERSION=<x.y.z>."
!endif
!ifndef SRC
  !error "SRC is not defined. Pass -DSRC=<install prefix>."
!endif
!ifndef OUT
  !error "OUT is not defined. Pass -DOUT=<output .exe>."
!endif

!include "MUI2.nsh"
!include "x64.nsh"
!include "FileFunc.nsh"

!define APPNAME    "Thermavip"
!define PUBLISHER  "CEA-IRFM"
!define HOMEPAGE   "https://github.com/IRFM/thermavip"
!define REGKEY     "Software\Microsoft\Windows\CurrentVersion\Uninstall\${APPNAME}"

Name              "${APPNAME} ${VERSION}"
OutFile           "${OUT}"
InstallDir        "$PROGRAMFILES64\${APPNAME}"
InstallDirRegKey  HKLM "Software\${PUBLISHER}\${APPNAME}" "InstallDir"
; Per-machine install under Program Files needs the elevation prompt; without
; it the copy fails silently on a standard account.
RequestExecutionLevel admin
SetCompressor /SOLID lzma

VIProductVersion "${VERSION}.0"
VIAddVersionKey  "ProductName"     "${APPNAME}"
VIAddVersionKey  "ProductVersion"  "${VERSION}"
VIAddVersionKey  "CompanyName"     "${PUBLISHER}"
VIAddVersionKey  "FileDescription" "${APPNAME} installer"
VIAddVersionKey  "FileVersion"     "${VERSION}"
VIAddVersionKey  "LegalCopyright"  "${PUBLISHER}"

!define MUI_ABORTWARNING
!define MUI_ICON   "${SRC}\thermavip\icons\thermavip.ico"
!define MUI_UNICON "${SRC}\thermavip\icons\thermavip.ico"

!insertmacro MUI_PAGE_LICENSE "${SRC}\share\thermavip\THIRD_PARTY_NOTICES.md"
!insertmacro MUI_PAGE_DIRECTORY
!insertmacro MUI_PAGE_INSTFILES
!define MUI_FINISHPAGE_RUN "$INSTDIR\Thermavip.exe"
!insertmacro MUI_PAGE_FINISH

!insertmacro MUI_UNPAGE_CONFIRM
!insertmacro MUI_UNPAGE_INSTFILES

!insertmacro MUI_LANGUAGE "English"

Function .onInit
	${IfNot} ${RunningX64}
		MessageBox MB_ICONSTOP "${APPNAME} is 64-bit only."
		Abort
	${EndIf}
FunctionEnd

Section "Thermavip" SecMain
	SectionIn RO
	SetOutPath "$INSTDIR"
	File /r "${SRC}\thermavip\*.*"

	CreateDirectory "$SMPROGRAMS\${APPNAME}"
	CreateShortCut  "$SMPROGRAMS\${APPNAME}\${APPNAME}.lnk" "$INSTDIR\Thermavip.exe"
	CreateShortCut  "$SMPROGRAMS\${APPNAME}\Uninstall.lnk"  "$INSTDIR\Uninstall.exe"

	WriteUninstaller "$INSTDIR\Uninstall.exe"
	WriteRegStr HKLM "Software\${PUBLISHER}\${APPNAME}" "InstallDir" "$INSTDIR"
	WriteRegStr HKLM "Software\${PUBLISHER}\${APPNAME}" "Version"    "${VERSION}"

	; Add/Remove Programs. EstimatedSize is read from what was actually copied,
	; so it stays right when the payload changes.
	WriteRegStr  HKLM "${REGKEY}" "DisplayName"     "${APPNAME}"
	WriteRegStr  HKLM "${REGKEY}" "DisplayVersion"  "${VERSION}"
	WriteRegStr  HKLM "${REGKEY}" "DisplayIcon"     "$INSTDIR\Thermavip.exe"
	WriteRegStr  HKLM "${REGKEY}" "Publisher"       "${PUBLISHER}"
	WriteRegStr  HKLM "${REGKEY}" "URLInfoAbout"    "${HOMEPAGE}"
	WriteRegStr  HKLM "${REGKEY}" "InstallLocation" "$INSTDIR"
	WriteRegStr  HKLM "${REGKEY}" "UninstallString" '"$INSTDIR\Uninstall.exe"'
	WriteRegDWORD HKLM "${REGKEY}" "NoModify" 1
	WriteRegDWORD HKLM "${REGKEY}" "NoRepair" 1
	${GetSize} "$INSTDIR" "/S=0K" $0 $1 $2
	IntFmt $0 "0x%08X" $0
	WriteRegDWORD HKLM "${REGKEY}" "EstimatedSize" "$0"
SectionEnd

Section "Uninstall"
	; RMDir /r on $INSTDIR only: never on a directory the user could have
	; redirected the install to and filled with something else.
	Delete "$SMPROGRAMS\${APPNAME}\${APPNAME}.lnk"
	Delete "$SMPROGRAMS\${APPNAME}\Uninstall.lnk"
	RMDir  "$SMPROGRAMS\${APPNAME}"

	Delete "$INSTDIR\Uninstall.exe"
	RMDir /r "$INSTDIR"

	DeleteRegKey HKLM "${REGKEY}"
	DeleteRegKey HKLM "Software\${PUBLISHER}\${APPNAME}"
SectionEnd
