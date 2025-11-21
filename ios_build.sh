#!/usr/bin/env sh

set -e

xcodebuild \
  -project code/ios/HandmadeHero.xcodeproj \
  -scheme HandmadeHero\
  -sdk iphonesimulator \
  -configuration Debug \
  -derivedDataPath build/ios \
  build
