#!/usr/bin/env bash
set -euo pipefail
cd "$(dirname "$0")/.."
cmake -S . -B build -G Xcode -DCMAKE_OSX_ARCHITECTURES='arm64;x86_64' -DCMAKE_OSX_DEPLOYMENT_TARGET=11.0
cmake --build build --config Release --parallel 2
ctest --test-dir build -C Release --output-on-failure
printf '%s\n' 'Built universal plugins: build/FreshLab_artefacts/Release/VST3 and build/FreshLabFX_artefacts/Release/VST3'
printf '%s\n' 'Copy both entire .vst3 bundles into ~/Library/Audio/Plug-Ins/VST3, then rescan in FL Studio.'
