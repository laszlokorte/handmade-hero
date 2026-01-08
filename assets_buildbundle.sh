#!/usr/bin/env sh

set -e

WARNING_FLAGS='-Wno-unused-variable -Wno-unused-function'

mkdir -p build

g++ -std=c++11 code/handmade_assets_bundler.cpp   -lm -g -Wall $WARNING_FLAGS -Werror -Wunused-label -o build/handmade_assets_bundler
./build/handmade_assets_bundler
