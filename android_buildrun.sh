#!/usr/bin/env sh

set -e

# Build APK
./android_build.sh

# show APK content
unzip -l build/android/handmade-signed.apk

# copy onto device
adb install build/android/handmade-signed.apk

# Start APK on device
adb shell am start -n de.handmade.hero/android.app.NativeActivity

# print debug log
adb logcat *:E
