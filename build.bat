@echo off

mkdir build

pushd build
cl /FC -Zi ../code/win32_handmade.cpp user32.lib Gdi32.lib
popd
