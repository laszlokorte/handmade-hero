#!/usr/bin/env sh

set -e

mkdir -p build/web

cp -r resources/web/* build/web/

clang \
  --target=wasm32 \
  -O3 \
  -x c\
  -std=c11 \
  -nostdlib \
  -matomics \
  -pthread \
  -mthread-model posix \
  -mbulk-memory \
  -Wl,--no-entry \
  -Wl,--export-all \
  -Wl,--import-memory \
  -Wl,--shared-memory \
  -Wl,--max-memory=2097152\
  -o build/web/handmade.wasm \
  code/handmade.c \
  code/wasm_handmade.c
