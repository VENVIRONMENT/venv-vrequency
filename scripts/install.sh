#!/usr/bin/env bash
# Install Tendril for the current user (no root needed):
#   ~/.local/opt/tendril/         the application
#   ~/.local/bin/tendril          launcher command
#   ~/.local/share/applications/  desktop entry
# It does NOT start at boot. Remove with scripts/uninstall.sh.
set -euo pipefail
cd "$(dirname "$0")/.."
[ -x build/tendril ] || ./scripts/build.sh
PREFIX="$HOME/.local/opt/tendril"
mkdir -p "$PREFIX/bin" "$PREFIX/share" "$HOME/.local/bin" "$HOME/.local/share/applications" \
         "$HOME/.local/share/icons/hicolor/scalable/apps"
install -m 755 build/tendril "$PREFIX/bin/tendril"
install -m 644 assets/tendril.svg "$PREFIX/share/tendril.svg"
install -m 644 assets/tendril.svg "$HOME/.local/share/icons/hicolor/scalable/apps/tendril.svg"
install -m 644 README.md "$PREFIX/share/README.md"
ln -sf "$PREFIX/bin/tendril" "$HOME/.local/bin/tendril"
cat > "$HOME/.local/share/applications/tendril.desktop" <<DESK
[Desktop Entry]
Type=Application
Name=Tendril
GenericName=Pocket Synthesiser
Comment=Slide-string pocket synth for the Hackberry Pi
Exec=$PREFIX/bin/tendril
Icon=tendril
Terminal=false
Categories=AudioVideo;Audio;Music;
Keywords=synth;synthesizer;music;instrument;
StartupWMClass=tendril
DESK
update-desktop-database "$HOME/.local/share/applications" >/dev/null 2>&1 || true
echo "Installed. Start it with:  tendril      (or the Tendril icon in the app grid)"
case ":$PATH:" in *":$HOME/.local/bin:"*) ;; *) echo "note: $HOME/.local/bin is not on your PATH";; esac
