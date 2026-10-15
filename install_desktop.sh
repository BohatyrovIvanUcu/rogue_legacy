#!/usr/bin/env bash
# ==============================================================================
# Rogue Legacy C++ Desktop Launcher Installer for Linux
# Adds Rogue Legacy C++ to your system Application Menu and Desktop!
# ==============================================================================

set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
BIN_PATH=""

# Detect binary location
if [ -f "$SCRIPT_DIR/rogue_legacy" ]; then
    BIN_PATH="$SCRIPT_DIR/rogue_legacy"
elif [ -f "$SCRIPT_DIR/build/rogue_legacy_cpp" ]; then
    BIN_PATH="$SCRIPT_DIR/build/rogue_legacy_cpp"
elif [ -f "$SCRIPT_DIR/RogueLegacy-x86_64.AppImage" ]; then
    BIN_PATH="$SCRIPT_DIR/RogueLegacy-x86_64.AppImage"
fi

if [ -z "$BIN_PATH" ] || [ ! -f "$BIN_PATH" ]; then
    echo "❌ Error: Could not find Rogue Legacy executable in $SCRIPT_DIR!"
    echo "Please build the game (cmake --build build) or download a release binary first."
    exit 1
fi

chmod +x "$BIN_PATH"

# Detect icon location
ICON_PATH=""
if [ -f "$SCRIPT_DIR/assets/icon.png" ]; then
    ICON_PATH="$SCRIPT_DIR/assets/icon.png"
elif [ -f "$SCRIPT_DIR/icon.png" ]; then
    ICON_PATH="$SCRIPT_DIR/icon.png"
fi

# Target directory for desktop entry
APPS_DIR="${XDG_DATA_HOME:-$HOME/.local/share}/applications"
mkdir -p "$APPS_DIR"

DESKTOP_FILE="$APPS_DIR/rogue_legacy.desktop"

cat << EOF > "$DESKTOP_FILE"
[Desktop Entry]
Type=Application
Name=Rogue Legacy C++
GenericName=Roguelite Action Platformer
Comment=A faithful procedural remake of Rogue Legacy in C++
Exec="$BIN_PATH"
Path="$SCRIPT_DIR"
Icon=${ICON_PATH:-applications-games}
Terminal=false
Categories=Game;ActionGame;
EOF

chmod +x "$DESKTOP_FILE"

# Also add to Desktop if folder exists
DESKTOP_FOLDER="${XDG_DESKTOP_DIR:-$HOME/Desktop}"
if [ -d "$DESKTOP_FOLDER" ]; then
    cp "$DESKTOP_FILE" "$DESKTOP_FOLDER/"
    chmod +x "$DESKTOP_FOLDER/rogue_legacy.desktop" 2>/dev/null || true
    # Allow launch on GNOME
    gio set "$DESKTOP_FOLDER/rogue_legacy.desktop" metadata::trusted true 2>/dev/null || true
fi

echo "=========================================================="
echo "  ✅ Rogue Legacy C++ installed successfully!"
echo "  🎮 Launcher created at: $DESKTOP_FILE"
if [ -d "$DESKTOP_FOLDER" ]; then
    echo "  🖥️  Desktop shortcut created at: $DESKTOP_FOLDER/rogue_legacy.desktop"
fi
echo "  You can now launch the game from your system Application Menu!"
echo "=========================================================="

