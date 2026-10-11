@echo off
rem Builds the emulated-game harness and two TEST builds of the plugin (settings file forced "missing" /
rem "existing" via DA_TEST_SETTINGS_FILE), then runs the scenarios. Output goes to out\hooktest\.
setlocal
call tools\vsenv.bat || exit /b 1

set "T=out\hooktest"
if exist %T% rmdir /s /q %T%
mkdir %T%\nofile %T%\file %T%\azerty

rem harness: its own PE image IS the fake game image (base 0x400000, section .fake covers up to 0xC1D000+)
cl /nologo /MT /W4 /EHsc /std:c++17 tools\hooktest\hooktest.cpp /Fo:%T%\ /Fe:%T%\hooktest.exe /link user32.lib /MACHINE:X86 /BASE:0x400000 /FIXED /DYNAMICBASE:NO || exit /b 1

rem test builds of the plugin
cl /nologo /LD /MT /O2 /W4 /EHsc /std:c++17 /DDA_TEST_SETTINGS_FILE=\"C:\\__definitely_missing__\\gta_sa.set\" src\main.cpp /Fo:%T%\nofile\ /Fe:%T%\nofile\DefaultSettings.SA.asi /link /MACHINE:X86 user32.lib shell32.lib >nul || exit /b 1
cl /nologo /LD /MT /O2 /W4 /EHsc /std:c++17 /DDA_TEST_SETTINGS_FILE=\"C:\\Windows\\win.ini\" src\main.cpp /Fo:%T%\file\ /Fe:%T%\file\DefaultSettings.SA.asi /link /MACHINE:X86 user32.lib shell32.lib >nul || exit /b 1

cl /nologo /LD /MT /O2 /W4 /EHsc /std:c++17 /DDA_TEST_LAYOUT=4 /DDA_TEST_SETTINGS_FILE=\"C:\\__definitely_missing__\\gta_sa.set\" src\main.cpp /Fo:%T%\azerty\ /Fe:%T%\azerty\DefaultSettings.SA.asi /link /MACHINE:X86 user32.lib shell32.lib >nul || exit /b 1

set RC=0
rem 1. shipped INI, file missing  2. shipped INI, file exists  3. custom INI, file missing
copy /y data\DefaultSettings.SA.ini %T%\nofile\DefaultSettings.SA.ini >nul
copy /y data\DefaultSettings.SA.ini %T%\file\DefaultSettings.SA.ini >nul
%T%\hooktest.exe %T%\nofile\DefaultSettings.SA.asi shipped 0 || set RC=1
%T%\hooktest.exe %T%\file\DefaultSettings.SA.asi shipped 1 || set RC=1
copy /y tools\hooktest\custom.ini %T%\nofile\DefaultSettings.SA.ini >nul
%T%\hooktest.exe %T%\nofile\DefaultSettings.SA.asi custom 0 || set RC=1
copy /y data\DefaultSettings.SA.ini %T%\nofile\DefaultSettings.SA.ini >nul
%T%\hooktest.exe %T%\nofile\DefaultSettings.SA.asi shipped 0 chained || set RC=1
copy /y data\DefaultSettings.SA.ini %T%\azerty\DefaultSettings.SA.ini >nul
%T%\hooktest.exe %T%\azerty\DefaultSettings.SA.asi shipped 0 azerty || set RC=1
echo.
if %RC%==0 (echo hooktest: ALL SCENARIOS PASSED) else (echo hooktest: FAILURES)
exit /b %RC%
