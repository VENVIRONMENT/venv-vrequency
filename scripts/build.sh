#!/usr/bin/env bash
# Configure, build and run the engine tests.
set -euo pipefail
cd "$(dirname "$0")/.."
./scripts/fetch-deps.sh
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j"$(nproc)"
./build/vrequency_tests
