@echo off

set CompilerFlags=-MT -nologo -EHa- -Oi -WX -W4 -wd4100 -wd4201 -wd4505 -wd4189 -DHANDMADE_SLOW=1 -DHANDMADE_INTERNAL=1 /FC -Zi
set LinkerFlags=user32.lib Gdi32.lib Winmm.lib

IF NOT EXIST build mkdir build

pushd build
cl %CompilerFlags% ../code/handmade.cpp -Fmhandmade.map /link -subsystem:windows,6.0 /DLL /EXPORT:GameUpdateAndRender /EXPORT:GameGetSoundSamples /OUT:handmade.dll
cl %CompilerFlags% ../code/win32_handmade.cpp -Fmwin32_handmade.map /link -subsystem:windows,6.0 %LinkerFlags%
popd
:W4
