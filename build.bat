@echo off

IF NOT EXIST build mkdir build

pushd build
cl -MT -nologo -EHa- -Oi -WX -W4 -wd4100 -Fmwin32_handmade.map -wd4201 -wd4505 -wd4189 -DHANDMADE_SLOW=1 -DHANDMADE_INTERNAL=1 /FC -Zi ../code/win32_handmade.cpp /link -subsystem:windows,6.0 user32.lib Gdi32.lib Winmm.lib
popd
