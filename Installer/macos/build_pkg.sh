#!/usr/bin/env bash
# Builds, signs, packages and notarizes the macOS installer (UNTESTED - needs an Apple Developer account).
# Required env:  DEV_ID_APP="Developer ID Application: NAME (TEAMID)"  DEV_ID_INSTALLER="Developer ID Installer: NAME (TEAMID)"
#                NOTARY_PROFILE=<keychain profile created with: xcrun notarytool store-credentials>
set -euo pipefail
cd "$(dirname "$0")/../.."
VERSION=1.0.0
cmake -S . -B build -G Xcode -DPV_BUILD_TESTS=OFF
cmake --build build --config Release --target PlanetVel_VST3 PlanetVel_AU
ART=build/PlanetVel_artefacts/Release
ROOT=build/pkgroot; rm -rf "$ROOT"
mkdir -p "$ROOT/Library/Audio/Plug-Ins/VST3" "$ROOT/Library/Audio/Plug-Ins/Components" "$ROOT/Library/Application Support/Trill Tron/Planet Vel/Presets"
cp -R "$ART/VST3/Planet Vel.vst3" "$ROOT/Library/Audio/Plug-Ins/VST3/"
cp -R "$ART/AU/Planet Vel.component" "$ROOT/Library/Audio/Plug-Ins/Components/"
cp -R Resources/Presets/Factory "$ROOT/Library/Application Support/Trill Tron/Planet Vel/Presets/"
for b in "$ROOT"/Library/Audio/Plug-Ins/*/*; do
  codesign --force --deep --options runtime --timestamp --sign "$DEV_ID_APP" "$b"
done
pkgbuild --root "$ROOT" --identifier com.trilltron.planetvel.pkg --version "$VERSION" --install-location / build/PlanetVel-component.pkg
productbuild --package build/PlanetVel-component.pkg --sign "$DEV_ID_INSTALLER" "PlanetVel-$VERSION-macOS.pkg"
xcrun notarytool submit "PlanetVel-$VERSION-macOS.pkg" --keychain-profile "$NOTARY_PROFILE" --wait
xcrun stapler staple "PlanetVel-$VERSION-macOS.pkg"
echo "done: PlanetVel-$VERSION-macOS.pkg"
