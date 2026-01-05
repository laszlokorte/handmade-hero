#!/usr/bin/env sh

set -e

mkdir -p build/web

cp -r resources/web/* build/web/

clang \
  --target=wasm32 \
  -O3 \
  -nostdlib \
  -matomics \
  -Wl,--no-entry \
  -Wl,--export-all \
  -Wl,--import-memory \
  -Wl,--shared-memory \
  -Wl,--max-memory=2097152\
  -o build/web/handmade.wasm \
  code/handmade.cpp \
  code/wasm_handmade.c
