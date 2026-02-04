#!/usr/bin/env sh

set -e

# --- Config ---
_JAVA_OPTIONS="--enable-native-access=ALL-UNNAMED"
BUILD_DIR="./build/android"
MANIFEST="./resources/AndroidManifest.xml"
APK_NAME="handmade.apk"
MIN_SDK=24
COMPILE_SDK="$ANDROID_SDK_ROOT/platforms/android-36/android.jar"
BUILD_TOOLS="$ANDROID_SDK_ROOT/build-tools/36.1.0"  # adjust to installed build-tools version
ANDROID_NDK_ROOT=$ANDROID_SDK_ROOT/ndk/26.1.10909125

# --- Prepare directories ---
rm -rf "$BUILD_DIR"

mkdir -p \
    "$BUILD_DIR/classes" \
    "$BUILD_DIR/apk" \
    "$BUILD_DIR/generated" \
    "$BUILD_DIR/native/lib/arm64-v8a" \
    "$BUILD_DIR/native/lib/x86_64"

# --- Compile native C code ---

# Compile for ARM
$ANDROID_NDK_ROOT/toolchains/llvm/prebuilt/linux-x86_64/bin/aarch64-linux-android24-clang \
  -shared -fPIC \
  -I$ANDROID_NDK_ROOT/sources/android/native_app_glue \
  --sysroot="$ANDROID_NDK_ROOT/toolchains/llvm/prebuilt/linux-x86_64/sysroot" \
  code/android_handmade.c \
  code/handmade.c \
  "$ANDROID_NDK_ROOT/sources/android/native_app_glue/android_native_app_glue.c" \
  -landroid -llog -lEGL -lGLESv2 \
  -o "$BUILD_DIR/native/lib/arm64-v8a/libmain.so"

# Compile for x64 (for emulator)
$ANDROID_NDK_ROOT/toolchains/llvm/prebuilt/linux-x86_64/bin/x86_64-linux-android24-clang \
    --sysroot="$ANDROID_NDK_ROOT/toolchains/llvm/prebuilt/linux-x86_64/sysroot" \
    -shared -fPIC \
    -I$ANDROID_NDK_ROOT/sources/android/native_app_glue \
    code/android_handmade.c \
    code/handmade.c \
    "$ANDROID_NDK_ROOT/sources/android/native_app_glue/android_native_app_glue.c" \
    -landroid -llog -lEGL -lGLESv2 \
    -o "$BUILD_DIR/native/lib/x86_64/libmain.so"

# compile svg to png
magick convert -density 2400 -background none "resources/web/favicon.svg" "resources/android/res/mipmap-mdpi/ic_launcher.png"

# --- process resource files and manifest ---
$BUILD_TOOLS/aapt package -f -m \
    -M "$MANIFEST"\
    -S "resources/android/res" \
    -I "$ANDROID_SDK_ROOT/platforms/android-36/android.jar" \
    -J "$BUILD_DIR/generated"

# --- Compile resources ---
$BUILD_TOOLS/aapt package -f -m  \
    -F "$BUILD_DIR/resources.ap_" \
    -M "$MANIFEST" \
    -S "resources/android/res" \
    -I "$COMPILE_SDK"

# --- Compile Java sources ---
find "code/android/java" -name "*.java" > "$BUILD_DIR/sources.txt"
find "$BUILD_DIR/generated" -name "*.java" >> "$BUILD_DIR/sources.txt"
javac -d "$BUILD_DIR/classes" -classpath "$COMPILE_SDK" "@$BUILD_DIR/sources.txt"

# --- Convert to DEX ---
$BUILD_TOOLS/d8 \
--lib $ANDROID_SDK_ROOT/platforms/android-36/android.jar \
--output "$BUILD_DIR/apk" $(find "$BUILD_DIR/classes" -name "*.class")

# --- Merge classes.dex and resources into APK ---
cp "$BUILD_DIR/resources.ap_" "$BUILD_DIR/$APK_NAME"
pushd "$BUILD_DIR/native"
zip -r "../$APK_NAME" "lib"
popd
zip -j "$BUILD_DIR/$APK_NAME" "$BUILD_DIR/apk/classes.dex"

# --- Align APK ---
$BUILD_TOOLS/zipalign -v 4 "$BUILD_DIR/$APK_NAME" "$BUILD_DIR/${APK_NAME%.apk}-aligned.apk"

# --- Sign APK with default debug keystore ---
 $BUILD_TOOLS/apksigner sign \
    --ks "$ANDROID_SDK_ROOT/debug.keystore" \
    --ks-pass pass:android \
    --key-pass pass:android \
    --min-sdk-version 24 \
    --out "$BUILD_DIR/${APK_NAME%.apk}-signed.apk" \
    "$BUILD_DIR/${APK_NAME%.apk}-aligned.apk"

echo "APK built at $BUILD_DIR/${APK_NAME%.apk}-signed.apk"
