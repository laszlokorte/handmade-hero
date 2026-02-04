#!/usr/bin/env sh

set -e

./android_build.sh
unzip -l build/android/handmade-signed.apk
adb install build/android/handmade-signed.apk
