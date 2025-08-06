@echo off

mkdir build

pushd build
cl ../code/win32_handmade.cpp /LINKER user32.lib /LINKER Gdi32.lib
popd
