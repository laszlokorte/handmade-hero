#!/usr/bin/env sh

set -e

./macos_build.sh "$@"

if [ "$1" != gameonly ]; then
rm -rf 'build/Handmade.app'
fi;

mkdir -p 'build/Handmade.app/Contents/MacOS'
mkdir -p 'build/Handmade.app/Contents/Resources'
cp build/handmade_game 'build/Handmade.app/Contents/MacOS/handmade_game'

if [ "$1" != gameonly ]; then
cp build/macos_handmade 'build/Handmade.app/Contents/MacOS/macos_handmade'
cp build/macos_shader.metallib 'build/Handmade.app/Contents/MacOS/macos_shader.metallib'
cp resources/Info.plist 'build/Handmade.app/Contents/Info.plist'
cp resources/PkgInfo 'build/Handmade.app/Contents/PkgInfo'
cp resources/AppIcon.icns 'build/Handmade.app/Contents/Resources/AppIcon.icns'
fi;
