#!/usr/bin/env bash
# Remove Tendril. Your sounds, sessions and recordings in ~/Tendril are kept
# unless you pass --purge.
set -euo pipefail
rm -rf "$HOME/.local/opt/tendril"
rm -f "$HOME/.local/bin/tendril" "$HOME/.local/share/applications/tendril.desktop" \
      "$HOME/.local/share/icons/hicolor/scalable/apps/tendril.svg"
update-desktop-database "$HOME/.local/share/applications" >/dev/null 2>&1 || true
if [ "${1:-}" = "--purge" ]; then rm -rf "$HOME/Tendril"; echo "Removed Tendril and ~/Tendril."; else echo "Removed Tendril. Your data in ~/Tendril was kept (use --purge to delete it)."; fi
