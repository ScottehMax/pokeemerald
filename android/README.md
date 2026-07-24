# Android port

The Android build runs the SDL frontend in the main app process and the game
core in a private `:core` service process. The core is loaded below 4 GiB with
`android_dlopen_ext`, preserving Emerald's 32-bit encoded script addresses on
64-bit devices without limiting the APK to one CPU architecture. The APK
contains `arm64-v8a`, `armeabi-v7a`, `x86_64`, and `x86` builds.

## Prerequisites

- JDK 17
- Android SDK platform 35 and build-tools 35.0.0
- Android NDK 27.2.12479018
- CMake 3.22.1 from the Android SDK
- An SDL 2.32.x source tree
- The normal generated game assets in `build/assets`

Build the ROM assets once if they are absent:

```sh
make modern
```

Copy `local.properties.example` to `local.properties`, set `sdk.dir` and
`sdl2.dir`, then build:

```sh
cd android
./gradlew :app:assembleDebug
```

The APK is written to `android/app/build/outputs/apk/debug/`. Saves, profiles,
expanded `.ek3` storage, and the remembered-profile preference live in the
app's private files directory. Uninstalling the app removes those files unless
Android backup restores them.

If the game core crashes or cannot start, the frontend writes the same
diagnostic text report and last-frame PPM used by the desktop port under the
active profile's `crash-reports` directory. Android's share sheet opens with
the text report so it can be saved or sent without ADB.

## Controls

The SDL surface accepts multitouch, Android game controllers, and the desktop
keyboard mapping. Back acts as B. Touch controls are overlaid at the edges of
the game image: D-pad on the left, A/B on the right, L/R at the upper corners,
and Select/Start along the lower center.

Hold L, R, and Select for 1.5 seconds to share an on-device performance report.
It separates game callback, VBlank/audio, software PPU, and Android presentation
timing so stalls can be attributed without ADB.

No Android-native menu is used at present; profile selection remains in the
game. If native settings are added later, use Jetpack Compose for that UI.
