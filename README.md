# Handmade Hero

My attempt at [Handmade Hero](https://guide.handmadehero.org/).

My focus is mostly implementing the various platform layers and not so much the game mechanics.

The goal is to implement as many platform layers as possible: Win32, Linux Wayland, Linux X11, MacOS, iOS, Android, Web (WASM).

For each platform I want to provide as many rendering API implementations as available and reasonable: OpenGL (Immediate and Shader), Vulkan, Metal, DirectX, X11, WebGL, WebCanvas

## Cross Platform Status

The Win32 Platform layer is is most advanced implemention.
Sound output, FPS Counter, Recording/Playpack and Hot Reload is supported.
The rendering is done via OpenGL.

On MacOS rendering is done via Metal, sound playback and game code hot reloading are implemented, but gameplay recording/playback is not implemented.

On Linux the rendering is done via OpenGL 3.3 on wayland. ALSA is used for audio. Hotreload for game code is implemented but as with MacOS gameplay recording/playback is not.

An basic X11 platform implementation is also available but it is the most barebone one. Due to missing double buffering it may flicker.

On iOS the rendering is working and simple touch controls for moving the camera are implemented. But audio is still missing and a virtual gamepad is not implemented yet but would be nice.

Android is not yet implemeted yet.

A WASM based web platform layer is not implemented yet.

## TODO

**Generic Interface for Renderer**:
Currently the rendering code is hardcoded into each platform layer. I would be nice to have a simple interface that allows the renderers to the swapped at runtime and matched wich each compatible platform.

## Preview

![Screenshot](./SCREENSHOT.png)
