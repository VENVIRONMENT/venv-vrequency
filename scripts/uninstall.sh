#!/usr/bin/env bash
# Remove venv vrequency. Your sounds, sessions and recordings in ~/vrequency are kept
# unless you pass --purge.
set -euo pipefail
rm -rf "$HOME/.local/opt/vrequency"
rm -f "$HOME/.local/bin/vrequency" "$HOME/.local/share/applications/vrequency.desktop" \
      "$HOME/.local/share/icons/hicolor/scalable/apps/vrequency.svg"
update-desktop-database "$HOME/.local/share/applications" >/dev/null 2>&1 || true
if [ "${1:-}" = "--purge" ]; then rm -rf "$HOME/vrequency"; echo "Removed venv vrequency and ~/vrequency."; else echo "Removed venv vrequency. Your data in ~/vrequency was kept (use --purge to delete it)."; fi
