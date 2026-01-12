#!/usr/bin/env sh

set -e

WARNING_FLAGS='-Wno-unused-variable -Wno-unused-function'

mkdir -p build

gcc -std=c11 -pthread code/handmade.c -g -shared -fPIC -Wall $WARNING_FLAGS -Werror -Wunused-label -o build/handmade_game
if [ "$1" != gameonly ]; then
g++ -std=c++11 $(pkg-config x11 --cflags --libs) code/x11_handmade.cpp  -lasound -lxkbcommon -lXrender -lEGL -lGL -lm -g -Wall $WARNING_FLAGS -Werror -Wunused-label -o build/x11_handmade
fi;
