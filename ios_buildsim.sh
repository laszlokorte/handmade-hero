#!/usr/bin/env sh

set -e

./ios_build.sh

open -a Simulator
xcrun simctl install booted build/ios/Build/Products/Debug-iphonesimulator/HandmadeHero.app
xcrun simctl launch booted de.laszlokorte.HandmadeHero
