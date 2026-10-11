@echo off
rem Builds DefaultSettings.SA.asi (32-bit, static CRT) into .\out with the Visual Studio C++ compiler,
rem then builds and runs the host-side self-test (tools\selftest.cpp), which also regenerates
rem out\DefaultSettings.SA.ini from src\actions.h. The checked-in data\DefaultSettings.SA.ini is that file.
setlocal
call tools\vsenv.bat || exit /b 1
if not exist out mkdir out

cl /nologo /LD /MT /O2 /W4 /EHsc /std:c++17 src\main.cpp /Fo:out\ /Fe:out\DefaultSettings.SA.asi /link /MACHINE:X86 user32.lib shell32.lib || exit /b 1

cl /nologo /MT /W4 /EHsc /std:c++17 tools\selftest.cpp /Fo:out\ /Fe:out\selftest.exe /link /MACHINE:X86 || exit /b 1
out\selftest.exe || exit /b 1
out\selftest.exe --emit-ini out\DefaultSettings.SA.ini || exit /b 1

echo.
echo Built out\DefaultSettings.SA.asi  (ini: out\DefaultSettings.SA.ini)
