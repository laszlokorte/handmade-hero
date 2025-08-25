@echo off

call win32_build.bat
if errorlevel 1 exit /b %errorlevel%
start /wait ./build/win32_handmade.exe
if errorlevel 1 exit /b %errorlevel%
