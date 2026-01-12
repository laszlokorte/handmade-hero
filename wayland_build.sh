#!/usr/bin/env sh

set -e

WARNING_FLAGS='-Wno-unused-variable -Wno-unused-function'
WAYLAND_PROT='/usr/share/wayland-protocols/stable/xdg-shell/xdg-shell.xml'
WAYLAND_PROT_C='build/xdg-shell-client-protocol.c'
WAYLAND_PROT_H='build/xdg-shell-client-protocol.h'
WAYLAND_PROT_O='build/xdg-shell-client-protocol.o'

mkdir -p build

wayland-scanner private-code "$WAYLAND_PROT" "$WAYLAND_PROT_C"
wayland-scanner client-header "$WAYLAND_PROT" "$WAYLAND_PROT_H"

gcc -std=c11 code/handmade.c -g -shared -fPIC -Wall $WARNING_FLAGS -Werror -Wunused-label -o build/handmade_game
if [ "$1" != gameonly ]; then
    gcc "$WAYLAND_PROT_C" -c -o "$WAYLAND_PROT_O"
    g++ -std=c++11 -pthread code/wayland_handmade.cpp "$WAYLAND_PROT_O" -include "$WAYLAND_PROT_H" -lasound -lxkbcommon -lwayland-client -lwayland-egl -lEGL -lGL -lm -g -Wall $WARNING_FLAGS -Werror -Wunused-label -o build/wayland_handmade
fi;
