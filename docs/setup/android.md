# Setting up android SDK

## Download latest Android `commandlinetools`

```sh
mkdir -p ~/Android/Sdk/cmdline-tools ~/Android/downloads
wget -O ~/Android/downloads/cmdline-tools-latest.zip \
    https://dl.google.com/android/repository/commandlinetools-linux-14742923_latest.zip
unzip ~/Android/downloads/cmdline-tools-latest.zip -d ~/Android/Sdk/cmdline-tools
mv ~/Android/Sdk/cmdline-tools/cmdline-tools \
   ~/Android/Sdk/cmdline-tools/latest
```

## Setup Env Vars

```sh
export ANDROID_HOME="$HOME/Android/Sdk"
export ANDROID_SDK_ROOT="$ANDROID_HOME"
export PATH="$PATH:$ANDROID_HOME/cmdline-tools/latest/bin"
export PATH="$PATH:$ANDROID_HOME/platform-tools"
```

# Accept all android SDK licences

```sh
yes | sdkmanager --licenses
```

# Install latest android SDK versions

```sh
sdkmanager \
  "platform-tools" \
  "platforms;android-34" \
  "build-tools;34.0.0" \
  "emulator" \
  "system-images;android-34;google_apis;x86_64" \
  "ndk;26.1.10909125"
```
