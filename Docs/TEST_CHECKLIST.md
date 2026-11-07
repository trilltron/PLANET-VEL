# PLANET VEL - release test checklist

Legend: **[A]** automated in `PlanetVelTests` / CI, **[M]** manual in a real DAW. Nothing marked [M] has been run yet
(no FL Studio, Windows or macOS machine was available when this was written) - fill in the tables as you go.

## 1. Automated (Linux, every commit)  - status at hand-over
| Check | Result |
|---|---|
| [A] 620 presets export/re-import byte-exact, every preset maps M1-M8 + mod wheel | pass |
| [A] Render every 8th preset: no NaN/Inf, none silent, none > 0 dBFS, CPU 1-6 % (4-note chord, 48 kHz/512) | pass |
| [A] 44.1/48/88.2/96/192 kHz x ECO/NORMAL/HQ x blocks 16..8192 with random automation + 12-note chords | pass (no NaN; CPU at 192 kHz HQ with worst-case random settings is high) |
| [A] Live sample-rate changes on one instance (22.05-192 kHz) | pass |
| [A] 64-voice limit + voice stealing + sustain pedal + all-notes-off | pass |
| [A] State save/restore across instances and sample rates (params, routes, arp, preset meta) | pass |
| [A] Arp timing (1/8 @120 bpm = 12000 samples) | exact |
| [A] VST3 bundle scan, 12x create/destroy, 5 stereo outputs, state, editor open/close/resize x5 | pass |
| [A] pluginval strictness 5 | **not run here** (CI job included) |

## 2. FL Studio matrix  ([M], fill with date + build)
Hosts: FL 20.8 or 20.9 (Windows 10), FL 20.9 (macOS Intel), FL 21, FL 2024, FL 2025.

| Check | FL20.8 Win | FL20.9 macOS | FL21 | FL2024 | FL2025 |
|---|---|---|---|---|---|
| Plugin scan (*Verify plugins*) finishes fast, no dialogs | | | | | |
| Loads as generator; presets via wrapper arrows (620 programs) | | | | | |
| Knob tweak registers in *Last tweaked*; automation clip works | | | | | |
| *Link to controller* / Multilink | | | | | |
| *Browse parameters*: 177 entries in stable order (+Bypass/Program) | | | | | |
| Resize: drag bottom-right handle | | | | | |
| Resize: drag wrapper window edge | | | | | |
| UI scale menu 75/100/125/150/200 % | | | | | |
| DPI 100 / 150 / 200 % (no blur, no double scaling; FL "Scale plugin window" on/off) | | | | | |
| Save/reload .flp; open FL20-saved project in newer FL and back | | | | | |
| Multi-out: enable Layer A-D outputs, route to mixer tracks | | | | | |
| 10+ instances; clone channel; *Make unique*; Patcher; smart disable | | | | | |
| Freeze / render to audio: tail renders (Nebula reverb), no clicks | | | | | |
| Fixed-size buffers ON/OFF; buffers 16 ... 4096 | | | | | |
| Tempo automation + Song/Pattern switch: arp, delay sync, glitch stay locked | | | | | |
| Arp MIDI drag-out into piano roll | | | | | |
| Drag audio file from FL browser onto the UI -> granular user sample | | | | | |

## 3. Platform/CPU/renderer matrix  ([M])
* Windows 10 and 11 (and 7/8.1 only with the JUCE 7 build): VST3 loads without VC++ redistributable installed.
* macOS 10.13 / 12 Intel and an Apple Silicon machine (native + Rosetta): signed + notarized bundle passes Gatekeeper.
* CPU without AVX2 (SSE2/SSE4.1 only): loads and plays. (Source uses no AVX intrinsics; verify with a VM that masks AVX.)
* Software renderer and Direct2D (Windows): no flicker while resizing.
* Other hosts: Ableton Live, Logic (AU), Cubase (build with `PV_VST3_MIDI_CC_PARAMS=ON`), Studio One, Reaper, Bitwig.
