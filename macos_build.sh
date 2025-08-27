#!/usr/bin/env sh

set -e

FRAMEWORKS='
-framework AppKit
-framework AudioToolbox
-framework Metal
-framework QuartzCore
'
WARNING_FLAGS='-Wno-unused-variable -Wno-unused-function'

mkdir -p build
clang code/handmade.cpp -shared -Wall $WARNING_FLAGS -Werror -Wunused-label -o build/handmade_game
clang ${FRAMEWORKS} code/macos_handmade.mm -Wall $WARNING_FLAGS -Werror -Wunused-label -o build/macos_handmade

xcrun -sdk macosx metal -c code/macos_shader.metal -o build/macos_shader.air
xcrun -sdk macosx metallib build/macos_shader.air -o build/macos_shader.metallib
