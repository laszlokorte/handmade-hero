#!/usr/bin/env sh

set -e

WARNING_FLAGS='-Wno-unused-variable -Wno-unused-function'

mkdir -p build

gcc code/handmade.cpp -g -shared -fPIC -Wall $WARNING_FLAGS -Werror -Wunused-label -o build/handmade_game
if [ "$1" != gameonly ]; then
gcc code/x11_handmade.cpp -g -Wall $WARNING_FLAGS -Werror -Wunused-label -o build/x11_handmade
fi;
