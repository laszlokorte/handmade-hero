#!/usr/bin/env bash

xcodebuild \
  -project code/ios/HandmadeHero.xcodeproj \
  -scheme HandmadeHero\
  -sdk iphonesimulator \
  -configuration Debug \
  -derivedDataPath build/ios \
  build
