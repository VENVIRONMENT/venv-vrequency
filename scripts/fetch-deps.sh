#!/usr/bin/env bash
# Fetch the audio development headers WITHOUT root: downloads the Ubuntu
# packages and unpacks them inside third_party/. Nothing is installed
# system-wide. Skipped automatically when the real -dev packages are present
# (sudo apt install libpipewire-0.3-dev libasound2-dev).
set -euo pipefail
cd "$(dirname "$0")/.."
if [ -f /usr/include/pipewire-0.3/pipewire/pipewire.h ] && [ -f /usr/include/alsa/asoundlib.h ]; then
  echo "system audio headers found - nothing to fetch"; exit 0
fi
if [ -f third_party/sysroot/usr/include/pipewire-0.3/pipewire/pipewire.h ] && [ -f third_party/sysroot/usr/include/alsa/asoundlib.h ]; then
  echo "third_party headers already present"; exit 0
fi
mkdir -p third_party/debs third_party/sysroot
( cd third_party/debs && apt-get download libasound2-dev libpipewire-0.3-dev libspa-0.2-dev )
for d in third_party/debs/*.deb; do dpkg -x "$d" third_party/sysroot; done
echo "headers unpacked into third_party/sysroot"
