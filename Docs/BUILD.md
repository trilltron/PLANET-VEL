# Building PLANET VEL

C++17 / JUCE 8 / CMake >= 3.22. JUCE is vendored in `third_party/JUCE` (or fetched automatically at the tag given by
`PV_JUCE_TAG` when that folder is absent).

| Option | Default | Meaning |
|---|---|---|
| `PV_VST2_SDK=<path>` | empty | Builds a VST2 too. Requires a Steinberg VST2 SDK **and** a valid VST2 license (not shipped). |
| `PV_BUILD_CLAP=ON` | OFF | Adds a CLAP via clap-juce-extensions (FL 2024+/modern hosts only). **Untested**; fetches third-party code, pin `GIT_TAG` first. |
| `PV_VST3_MIDI_CC_PARAMS=ON` | OFF | Exposes JUCE's 2048 "MIDI CC" emulation parameters. Needed for pitch-bend/mod-wheel in Cubase/Studio One; leave OFF for FL Studio (keeps *Browse parameters* clean). |
| `PV_BUILD_TESTS` | ON | Headless harness `PlanetVelTests`. |
| `PV_JUCE_TAG` | 8.0.8 | JUCE version when fetching. |

> **Windows 7 / 8.1 note.** JUCE 8 requires Windows 10+. FL Studio 20 still runs on Windows 7/8.1, so for those users build with
> `-DPV_JUCE_DIR=<path to JUCE 7.0.12>` (the source uses no JUCE-8-only API except `juce::FontOptions`, which needs a one-line
> shim to `juce::Font (name, height, style)` on 7.x). This path is **not tested here**.

## Windows (Visual Studio 2022)
```bat
cmake -S . -B build -G "Visual Studio 17 2022" -A x64
cmake --build build --config Release --target PlanetVel_VST3
```
* The CMake file sets `CMAKE_MSVC_RUNTIME_LIBRARY=MultiThreaded` (`/MT`): no VC++ redistributable required.
* No `/arch:AVX*` flags are set; baseline is SSE2. Any AVX path must be chosen at runtime.
* Output: `build\PlanetVel_artefacts\Release\VST3\Planet Vel.vst3` -> copy to `C:\Program Files\Common Files\VST3`.
* VST2: add `-DPV_VST2_SDK=C:\SDKs\vst2` and build target `PlanetVel_VST`; copy the `.dll` to the folder FL scans.

## macOS (Xcode)
```bash
cmake -S . -B build -G Xcode            # universal x86_64+arm64, deployment target 10.13 are set in CMakeLists.txt
cmake --build build --config Release --target PlanetVel_VST3 PlanetVel_AU
```
Sign + notarize (Developer ID, hardened runtime):
```bash
for f in build/PlanetVel_artefacts/Release/VST3/*.vst3 build/PlanetVel_artefacts/Release/AU/*.component; do
  codesign --force --deep --options runtime --timestamp --sign "Developer ID Application: YOUR NAME (TEAMID)" "$f"
done
Installer/macos/build_pkg.sh      # builds, signs and notarizes the .pkg (see script header for the env vars)
```
FL Studio 20 on Apple Silicon runs under Rosetta and loads the x86_64 slice; the universal binary covers it.

## Linux (used for development/CI here)
```bash
sudo apt install libasound2-dev libx11-dev libxrandr-dev libxinerama-dev libxcursor-dev libfreetype-dev libfontconfig1-dev libgl1-mesa-dev
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release && cmake --build build -j
```

## Headless test harness
```bash
build/PlanetVelTests_artefacts/Release/PlanetVelTests export Resources/Presets/Factory   # 620 .pvpreset files + round-trip checks
build/PlanetVelTests_artefacts/Release/PlanetVelTests render 8 [wavDir]                  # audio sanity per category (NaN/silence/clip/CPU)
build/PlanetVelTests_artefacts/Release/PlanetVelTests stress                             # 44.1-192 kHz, 16-8192 blocks, ECO/NORMAL/HQ, 64-voice, state, arp timing
build/PlanetVelTests_artefacts/Release/PlanetVelTests vst3 "path/to/Planet Vel.vst3"     # scans + loads the built bundle like a host
xvfb-run -a build/PlanetVelTests_artefacts/Release/PlanetVelTests snapshot ui.png <page 0-5> <program> <scale%>
build/PlanetVelTests_artefacts/Release/PlanetVelTests calibrate Source/FactoryCalibration.inc   # re-measure factory loudness
```
Run **pluginval** (https://github.com/Tracktion/pluginval) at strictness 5 on every platform before release; the CI workflow does.
