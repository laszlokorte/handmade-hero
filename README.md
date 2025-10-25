# Handmade Hero

My attempt at [Handmade Hero](https://guide.handmadehero.org/).

My focus is mostly implementing the various platform layers and not so much the game mechanics.

## Cross Platform Satus

The Win32 Platform layer is is most advanced implemention.
Sound output, FPS Counter, Recording/Playpack and Hot Reload is supported.
The rendering is done via OpenGL.

On MacOS rendering is done via Metal, sound playback and game code hot reloading are implemented, but gameplay recording/playback is not implemented.

On Linux the rendering is done via OpenGL 3.3 on wayland. ALSA is used for audio. Hotreload for game code is implemented but as with MacOS gameplay recording/playback is not.

An X11 implemention is still missing.

## Preview

![Screenshot](./SCREENSHOT.png)
