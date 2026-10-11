@echo off
rem Sets up the 32-bit Visual Studio C++ environment (cl, link) in the calling cmd session.
rem Usage:  call tools\vsenv.bat || exit /b 1
set "VSWHERE=%ProgramFiles(x86)%\Microsoft Visual Studio\Installer\vswhere.exe"
if not exist "%VSWHERE%" (echo vswhere.exe not found - install Visual Studio with the C++ x86 tools & exit /b 1)
rem vswhere runs as a plain command (not inside for /f): its path contains parentheses, which cmd mangles there.
set "VSTMP=%TEMP%\DefaultSettings.SA.vsdir.txt"
"%VSWHERE%" -latest -prerelease -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath > "%VSTMP%"
set "VSDIR="
set /p VSDIR=<"%VSTMP%"
del "%VSTMP%" 2>nul
if not defined VSDIR (echo Visual Studio with the C++ x86 toolset not found & exit /b 1)
call "%VSDIR%\VC\Auxiliary\Build\vcvars32.bat" >nul || exit /b 1
exit /b 0
