# PLANET VEL  -  by Trill Tron
*Signals From a Red World.*  A hybrid cinematic-ambient instrument plugin (JUCE 8 / C++17), sister product to *ZIL - Xeno-Synth Transmitter*.
VST3 (primary) + Standalone on every OS, AU on macOS, optional VST2 (needs your SDK license) and CLAP.

## What is in the box
| Area | Status |
|---|---|
| **5 layer engines x 4 layers** (A-D, each with filter, ADSR, pan, spread, level, key range, reverb send, unison) | done |
| Wavetable: **208 band-limited tables** (8 families x 26 frames, 10 mip levels, built in 230 ms on a background thread), warp modes (bend/sync/mirror/FM/PWM), unison <= 16 | done |
| Analog: saw/square/tri/sine/supersaw/noise, PWM, hard sync, FM, sub, unison <= 16 | done |
| Granular: 4 procedural source clouds + **user audio (drag & drop onto the UI)**, size/density/spray/pitch-random/reverse/freeze/**tempo-synced scan** | done |
| Organic: modal-synthesis felt piano, bells, glass, kalimba, music box; **formant choir with Ah/Oh/Oo/Mm/Ee morph + voice size**; strings; user-sample player (start/reverse/loop) | done (synthesized, see limits) |
| Spectral/Resonator: bowed/struck modal resonator bank, structure (harmonic <-> metal), blur, decay | done (resonator bank, see limits) |
| **ORBIT** XY blend of the 4 layers + animated orbit path | done |
| 8 macros (TIDE MORPH SPACE GRIT WARP AIR DRIFT GLITCH), **32-slot mod matrix**, 2 LFOs (6 shapes), mod wheel / aftertouch sources | done |
| Arp: 7 patterns, 1-4 oct, 15 rates (triplet/dotted), **32 steps** with velocity / pitch / ratchet 1-8 / probability, PPQ-locked, **drag-out MIDI to the piano roll** | done |
| Glitch: stutter, reverse, tape-stop, bit-crush, pitch-jump, gate-chop (probability + tempo sync, PPQ-locked) | done |
| FX: master filter, tape drive, chorus, ping-pong delay (sync/free), **FDN reverb** (Hall/Plate/Cathedral/Nebula up to 60 s, infinite hold at max), 3-band EQ, width, mono, soft limiter | done |
| **620 factory presets**, 13 categories, tag search, engine filter, favourites, Explore, prev/next, A/B, user presets (`.pvpreset` JSON), exposed as host programs; all 8 macros + mod wheel mapped in every preset | done (procedurally authored, see limits) |
| UI matching the reference: brushed-metal glow knobs, LCD + animated planet orb, 61-key keyboard that lights for incoming MIDI, PB/MOD wheels, 6 tabs (PROGRAM COMBI LAYERS FX ARP GLOBAL), 3 themes | done |
| Resize: corner handle + host edge drag, 16:10 locked, 960x600 ... 3840x2400, 75-200 % menu, vector (AffineTransform) scaling, size saved per instance + globally | done |
| Multi-out: stereo main + 4 layer outputs | done |
| Knob handling: drag, **shift/ctrl fine**, wheel, double-click reset, right-click menu with **MIDI learn**, tooltips, automation via host params (registers with FL *Last tweaked*) | done |
| ECO/NORMAL/HQ, up to 64 voices, voice stealing, `.scl` tuning, versioned state, frozen IDs (`TRTN`/`Pvel`), parameters append-only | done |

## Honest limits (things the brief asked for that are **not** in v1.0)
* **No recorded sample library.** There is no licensed piano/choir/string multisample set in this repo, so "Organic" is modal / formant /
  oscillator synthesis. It sounds like a synth emulating those instruments, not like recordings. The sampler hooks (user-sample player,
  disk streaming, multi-velocity, round-robin, true-legato) are **not implemented**. Dropping in a real library is the largest remaining content task.
* Spectral engine is a modal resonator bank - no FFT resynthesis/blur/freeze yet.
* Layer filter types: LP / BP / HP / Notch (no comb / formant / ladder / vowel); filter envelope = amp envelope (amount per layer).
* Mod: **2** LFOs (spec: 4), no extra ADSR/DAHDSR slots, no step sequencers, no chaos generator, no drawable LFO shapes, no drag-and-drop assignment
  (matrix is edited by clicking), no MPE (the switch is reserved).
* FX: fixed order (not a reorderable 8-slot rack); no shimmer, granular/diffusion/tape delay, flanger/phaser/rotary, vinyl/wow-flutter,
  spectral FX, compressor/transient shaper/ducking, mono-bass.
* **Factory presets are algorithmic**, not hand-designed: names for the signature ones are from the brief, the rest are generated combinations.
  They are level-calibrated by measurement and are a solid starting point, but nobody has auditioned them by ear yet - expect to prune/tune.
* UI: no collapsible keyboard, no OpenGL renderer option (JUCE software / Direct2D is used), no separate arp "pattern editor" for pitch lanes beyond what is listed.
* **Only the Linux VST3 + Standalone were built and tested here.** Windows and macOS builds, the FL Studio matrix, pluginval, installers and
  signing/notarization were **not run** (no such machines). `.github/workflows/build.yml` and `Docs/TEST_CHECKLIST.md` are ready for that.
* JUCE 8 needs Windows 10+. Windows 7/8.1 (which FL 20 still supports) needs a JUCE 7.0.12 build - see `Docs/BUILD.md`.

## Layout
```
CMakeLists.txt         JUCE plugin target + headless test harness
Source/Params.*        parameter table (append-only!)          Source/Presets.*  preset model, JSON, 620 factory presets
Source/DSP/            Engines, Synth, Fx, Arp, Library, Dsp   Source/PluginProcessor.*  audio thread, state, programs, MIDI learn
Source/UI/             editor, LCD, pages, widgets, themes     Tests/Harness.cpp   offline tests + UI snapshots
Resources/Presets/Factory   all 620 presets as human-readable .pvpreset      Resources/Demos   13 audio renders (one per category)
Installer/             Inno Setup script (Windows), pkgbuild/notarize script (macOS)       Docs/   BUILD.md, TEST_CHECKLIST.md
dist/linux/            Linux build of the VST3 and Standalone produced here
```
See `Docs/BUILD.md` to build, and `Docs/TEST_CHECKLIST.md` for the FL Studio 20 -> 2025 test matrix.
