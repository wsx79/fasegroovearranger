#!/usr/bin/env bash
set -euo pipefail
cd "$(dirname "$0")/.."
mkdir -p build/core
"${CXX:-g++}" -std=c++17 -O2 -Wall -Wextra -Wpedantic \
  Source/core/Project.cpp Source/core/PianoComping.cpp Source/core/Midi.cpp \
  ThirdParty/fasegroovepiano/Source/engine/*.cpp \
  ThirdParty/fasegroovedrums/Source/engine/*.cpp \
  ThirdParty/fasegrooveguitar/Source/engine/*.cpp \
  tests/CoreTests.cpp -o build/core/arranger-tests
build/core/arranger-tests build/core
