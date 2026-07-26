# Android port

The Android build runs the SDL frontend in the main app process and the game
core in a private `:core` service process. The core is loaded below 4 GiB with
`android_dlopen_ext`, preserving Emerald's 32-bit encoded script addresses on
64-bit devices without limiting the port to one CPU architecture. The build
produces separate `arm64-v8a`, `armeabi-v7a`, `x86_64`, and `x86` APKs so a
device does not carry native libraries for three unused architectures.

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

The ABI-specific APKs are written to `android/app/build/outputs/apk/debug/`.
For example, use `app-arm64-v8a-debug.apk` on an arm64 device. Every active
profile can access the default and profile `.ek3` storage collections. The
settings screen can keep saves and storage in the private app folder or move
them to the user-accessible
`Android/data/org.pokeemerald.pc/files/Pokemon Emerald/` device folder. Location
changes are applied on the next cold start so the running save is never copied
while the core is writing it.

The overworld viewport follows the device aspect ratio, up to 400x160, so wider
screens reveal more of the map instead of adding side bars. Battles, menus, and
other fixed-layout scenes remain at their original 240x160 composition.

Use the settings button to the left of the camera-safe top gap to choose the
rendezvous server and game-data location. The server value accepts a host name
or IPv4 address with an optional UDP port; port 8765 is used when omitted. The
button to the right of the gap hides or restores the complete virtual button
overlay while retaining direct touch interactions. Link Play
sends game traffic through the configured server; direct peer-to-peer transport
is disabled in the standard Android build.

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

Profile selection remains in the game. The native settings screen uses Jetpack
Compose.
