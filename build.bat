@echo off

set CompilerFlags=-MT -nologo -EHa- -Oi -WX -W4 -wd4100 -wd4201 -wd4505 -wd4189 -DHANDMADE_SLOW=1 -DHANDMADE_INTERNAL=1 /FC -Zi
set LinkerFlags=-incremental:no user32.lib Gdi32.lib Winmm.lib


IF NOT EXIST build mkdir build
pushd build
IF NOT EXIST hotreload.lock type NUL > hotreload.lock
del handmade_*.PDB > NUL 2> NUL
cl %CompilerFlags% ../code/handmade.cpp -Fmhandmade.map /LD /Fdhandmade_%random%.PDB /link -subsystem:windows,6.0 /EXPORT:GameUpdateAndRender /EXPORT:GameGetSoundSamples
if errorlevel 1 exit /b %errorlevel%
del hotreload.lock
if not '%1'=='gameonly' cl %CompilerFlags% ../code/win32_handmade.cpp -Fmwin32_handmade.map /link -subsystem:windows,6.0 %LinkerFlags%
if errorlevel 1 exit /b %errorlevel%
popd
