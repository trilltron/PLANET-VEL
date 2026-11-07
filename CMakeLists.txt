cmake_minimum_required(VERSION 3.22)

# ---- platform baselines (FL Studio 20 -> 2025 compatibility) -------------------------------
if(CMAKE_HOST_APPLE)   # (APPLE/MSVC are only defined after project(), so test the host here)
  # macOS 10.13 High Sierra+, universal (Intel slice is what FL 20 uses, even on Apple Silicon)
  set(CMAKE_OSX_DEPLOYMENT_TARGET "10.13" CACHE STRING "Minimum macOS" FORCE)
  set(CMAKE_OSX_ARCHITECTURES "x86_64;arm64" CACHE STRING "Universal binary" FORCE)
endif()
# Static MSVC runtime (/MT): no VC++ redistributable needed on the user's machine. Ignored by non-MSVC compilers.
# (Must NOT be wrapped in if(MSVC): that variable does not exist before project(). An older VC++ runtime on the user's PC
#  crashed the dynamic-runtime build inside MSVCP140.dll at startup.)
set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>" CACHE STRING "" FORCE)

project(PlanetVel VERSION 1.0.0 LANGUAGES C CXX)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)

# JUCE location: bundled in third_party/ or override with -DPV_JUCE_DIR=...
set(PV_JUCE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/third_party/JUCE" CACHE PATH "Path to JUCE")
set(PV_JUCE_TAG "8.0.8" CACHE STRING "JUCE tag fetched when PV_JUCE_DIR is absent (JUCE 8 needs Windows 10+; use 7.0.12 for Windows 7/8.1)")
if(EXISTS "${PV_JUCE_DIR}/CMakeLists.txt")
  add_subdirectory(${PV_JUCE_DIR} JUCE EXCLUDE_FROM_ALL)
else()
  include(FetchContent)
  FetchContent_Declare(JUCE GIT_REPOSITORY https://github.com/juce-framework/JUCE.git GIT_TAG ${PV_JUCE_TAG} GIT_SHALLOW ON)
  FetchContent_MakeAvailable(JUCE)
endif()

# Optional formats
set(PV_VST2_SDK "" CACHE PATH "Path to Steinberg VST2 SDK (requires a valid VST2 license). Empty = no VST2")
# JUCE's VST3 wrapper can append 2048 hidden-ish "MIDI CC n Ch m" parameters so hosts that only deliver pitch-bend /
# mod-wheel via IMidiMapping (Cubase, Studio One) work. FL Studio sends real MIDI events and would list all 2048
# extras in "Browse parameters", so this is OFF by default. Turn ON for a build aimed at Cubase/Nuendo/Studio One.
option(PV_VST3_MIDI_CC_PARAMS "Expose MIDI CC emulation parameters in the VST3" OFF)
option(PV_BUILD_CLAP "Also build a CLAP (FL 2024+ and modern hosts only; never the only format)" OFF)

set(PV_FORMATS VST3 Standalone)
if(APPLE)
  list(APPEND PV_FORMATS AU)
endif()
if(PV_VST2_SDK)
  juce_set_vst2_sdk_path(${PV_VST2_SDK})
  list(APPEND PV_FORMATS VST)
endif()

juce_add_plugin(PlanetVel
  COMPANY_NAME                "Trill Tron"
  PRODUCT_NAME                "Planet Vel"
  PLUGIN_NAME                 "Planet Vel"
  DESCRIPTION                 "Hybrid Cinematic Ambient Workstation"
  # IDs are frozen forever so old FL projects keep loading the plugin. NEVER change these.
  PLUGIN_MANUFACTURER_CODE    TRTN
  PLUGIN_CODE                 Pvel
  BUNDLE_ID                   com.trilltron.planetvel
  FORMATS                     ${PV_FORMATS}
  IS_SYNTH                    TRUE
  NEEDS_MIDI_INPUT            TRUE
  NEEDS_MIDI_OUTPUT           FALSE
  EDITOR_WANTS_KEYBOARD_FOCUS FALSE
  VST3_CATEGORIES             Instrument Synth
  VST2_CATEGORY               kPlugCategSynth
  AU_MAIN_TYPE                kAudioUnitType_MusicDevice
  COPY_PLUGIN_AFTER_BUILD     FALSE
  VERSION                     ${PROJECT_VERSION})

file(GLOB_RECURSE PV_SOURCES CONFIGURE_DEPENDS Source/*.cpp Source/*.h)
target_sources(PlanetVel PRIVATE ${PV_SOURCES})
target_include_directories(PlanetVel PRIVATE Source)

target_compile_definitions(PlanetVel PUBLIC
  JUCE_WEB_BROWSER=0
  JUCE_USE_CURL=0                  # no network calls, ever (plugin scanner safety)
  JUCE_VST3_CAN_REPLACE_VST2=0
  JUCE_VST3_EMULATE_MIDI_CC_WITH_PARAMETERS=$<BOOL:${PV_VST3_MIDI_CC_PARAMS}>
  JUCE_DISPLAY_SPLASH_SCREEN=0
  JUCE_REPORT_APP_USAGE=0
  JUCE_STRICT_REFCOUNTEDPOINTER=1
  JUCE_MODAL_LOOPS_PERMITTED=0)

target_link_libraries(PlanetVel
  PRIVATE
    juce::juce_audio_utils juce::juce_audio_formats juce::juce_dsp
  PUBLIC
    juce::juce_recommended_config_flags
    juce::juce_recommended_warning_flags)

# Baseline is SSE2 on x86-64; no -mavx*. Any AVX paths must be chosen at runtime.
if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang" AND NOT APPLE)
  target_compile_options(PlanetVel PRIVATE -fno-math-errno)
endif()

# ---- CLAP (optional; FL 2024+ and other modern hosts only - FL 20 cannot load CLAP, VST3 stays primary) ----
# UNTESTED in this repo's CI so far. Fetches third-party code at configure time: pin GIT_TAG to a release you have reviewed.
if(PV_BUILD_CLAP)
  include(FetchContent)
  FetchContent_Declare(clap-juce-extensions GIT_REPOSITORY https://github.com/free-audio/clap-juce-extensions.git GIT_TAG main GIT_SHALLOW ON)
  FetchContent_MakeAvailable(clap-juce-extensions)
  clap_juce_extensions_plugin(TARGET PlanetVel CLAP_ID "com.trilltron.planetvel" CLAP_FEATURES instrument synthesizer ambient)
endif()

# ---- headless test / preset-export harness --------------------------------------------------
option(PV_BUILD_TESTS "Build headless DSP test harness" ON)
if(PV_BUILD_TESTS)
  juce_add_console_app(PlanetVelTests PRODUCT_NAME "PlanetVelTests")
  target_sources(PlanetVelTests PRIVATE Tests/Harness.cpp)
  target_include_directories(PlanetVelTests PRIVATE Source)
  target_compile_definitions(PlanetVelTests PRIVATE JUCE_WEB_BROWSER=0 JUCE_USE_CURL=0 JUCE_PLUGINHOST_VST3=1)
  target_link_libraries(PlanetVelTests PRIVATE PlanetVel
    juce::juce_audio_utils juce::juce_audio_formats juce::juce_dsp
    juce::juce_recommended_config_flags)
endif()
