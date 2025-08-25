#!/usr/bin/env sh

set -e

WARNING_FLAGS='-Wno-unused-variable -Wno-unused-function'

mkdir -p build
clang code/handmade.cpp -shared -Wall $WARNING_FLAGS -Werror -Wunused-label -o build/handmade_game
clang -framework AppKit code/macos_handmade.mm -Wall $WARNING_FLAGS -Werror -Wunused-label -o build/macos_handmade
