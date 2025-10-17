
#!/usr/bin/env sh

set -e

WARNING_FLAGS='-Wno-unused-variable -Wno-unused-function'
WAYLAND_PROT='/usr/share/wayland-protocols/stable/xdg-shell/xdg-shell.xml'
WAYLAND_PROT_C='build/xdg-shell-client-protocol.c'
WAYLAND_PROT_H='build/xdg-shell-client-protocol.h'

mkdir -p build

wayland-scanner private-code "$WAYLAND_PROT" "$WAYLAND_PROT_C"
wayland-scanner client-header "$WAYLAND_PROT" "$WAYLAND_PROT_H"

gcc code/handmade.cpp -g -shared -fPIC -Wall $WARNING_FLAGS -Werror -Wunused-label -o build/handmade_game
if [ "$1" != gameonly ]; then
    gcc code/wayland_handmade.cpp "$WAYLAND_PROT_C" -include "$WAYLAND_PROT_H" -lasound -lxkbcommon -lwayland-client -lwayland-egl -lEGL -lGLESv2 -lm -g -Wall $WARNING_FLAGS -Werror -Wunused-label -o build/wayland_handmade
fi;
