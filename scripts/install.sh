#!/usr/bin/env bash
# Install venv vrequency for the current user (no root needed):
#   ~/.local/opt/vrequency/         the application
#   ~/.local/bin/vrequency          launcher command
#   ~/.local/share/applications/  desktop entry
# It does NOT start at boot. Remove with scripts/uninstall.sh.
set -euo pipefail
cd "$(dirname "$0")/.."
[ -x build/vrequency ] || ./scripts/build.sh
PREFIX="$HOME/.local/opt/vrequency"
mkdir -p "$PREFIX/bin" "$PREFIX/share" "$HOME/.local/bin" "$HOME/.local/share/applications" \
         "$HOME/.local/share/icons/hicolor/scalable/apps"
install -m 755 build/vrequency "$PREFIX/bin/vrequency"
install -m 644 assets/vrequency.svg "$PREFIX/share/vrequency.svg"
install -m 644 assets/vrequency.svg "$HOME/.local/share/icons/hicolor/scalable/apps/vrequency.svg"
install -m 644 README.md "$PREFIX/share/README.md"
ln -sf "$PREFIX/bin/vrequency" "$HOME/.local/bin/vrequency"
cat > "$HOME/.local/share/applications/vrequency.desktop" <<DESK
[Desktop Entry]
Type=Application
Name=venv vrequency
GenericName=Pocket Synthesiser
Comment=Slide-string pocket synth for the Hackberry Pi
Exec=$PREFIX/bin/vrequency
Icon=vrequency
Terminal=false
Categories=AudioVideo;Audio;Music;
Keywords=synth;synthesizer;music;instrument;
StartupWMClass=vrequency
DESK
update-desktop-database "$HOME/.local/share/applications" >/dev/null 2>&1 || true
echo "Installed. Start it with:  vrequency      (or the venv vrequency icon in the app grid)"
case ":$PATH:" in *":$HOME/.local/bin:"*) ;; *) echo "note: $HOME/.local/bin is not on your PATH";; esac
