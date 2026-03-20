#!/bin/bash
# ROG Ally Command Center - Install Script
# Builds, installs, and configures everything in one step.
# Requires: Arch Linux / CachyOS on ROG Ally

set -e
cd "$(dirname "$0")"

echo "=========================================="
echo "  ROG Ally Command Center - Installer"
echo "=========================================="
echo ""

# Pre-flight checks
FAIL=0

# ROG Ally
if ! grep -q "RC71L" /sys/class/dmi/id/board_name 2>/dev/null; then
    echo "WARNING: This does not appear to be an ROG Ally (RC71L)."
    read -p "Continue anyway? [y/N] " -n 1 -r
    echo
    [[ $REPLY =~ ^[Yy]$ ]] || exit 1
fi

# Arch/pacman
if ! command -v pacman &>/dev/null; then
    echo "ERROR: pacman not found. This installer requires Arch Linux or CachyOS."
    FAIL=1
fi

# Wayland session
if [ "$XDG_SESSION_TYPE" != "wayland" ]; then
    echo "ERROR: Wayland session required. You appear to be running: ${XDG_SESSION_TYPE:-unknown}"
    FAIL=1
fi

# InputPlumber
if ! systemctl is-active inputplumber.service &>/dev/null; then
    echo "WARNING: InputPlumber is not running. The CC button remap will not work."
    echo "         Install InputPlumber or the CC button won't open this menu."
fi

# PipeWire/WirePlumber
if ! command -v wpctl &>/dev/null; then
    echo "WARNING: wpctl not found. Volume control will not work."
    echo "         Install pipewire and wireplumber for volume support."
fi

# sudo
if ! command -v sudo &>/dev/null; then
    echo "ERROR: sudo is required to install system files."
    FAIL=1
fi

if [ "$FAIL" -eq 1 ]; then
    echo ""
    echo "Fix the errors above and try again."
    exit 1
fi
echo "All checks passed."
echo ""

# 1. Install dependencies
echo "[1/7] Installing dependencies..."
sudo pacman -S --needed --noconfirm \
    qt6-base qt6-declarative layer-shell-qt \
    cmake polkit grim 2>/dev/null || {
    echo "ERROR: Failed to install dependencies. Check your internet connection."
    exit 1
}

# 2. Build
echo "[2/7] Building..."
cmake -B build -DCMAKE_BUILD_TYPE=Release -DCMAKE_INSTALL_PREFIX=/usr
cmake --build build -j$(nproc)

# 3. Install system files
echo "[3/7] Installing system files (requires password)..."
sudo bash -c "
    cp build/rog-ally-menu /usr/bin/rog-ally-menu
    cp rog-ally-menu-helper /usr/bin/rog-ally-menu-helper
    chmod +x /usr/bin/rog-ally-menu /usr/bin/rog-ally-menu-helper
    cp com.rogallymenu.pkexec.policy /usr/share/polkit-1/actions/
    cp 99-rog-ally-menu.rules /etc/udev/rules.d/
    udevadm control --reload-rules
    udevadm trigger
"

# 4. Fix device permissions now
echo "[4/7] Fixing device permissions..."
pkexec /usr/bin/rog-ally-menu-helper fix-hidraw

# 5. Install systemd user service
echo "[5/7] Setting up systemd service..."
mkdir -p ~/.config/systemd/user
cp rog-ally-menu.service ~/.config/systemd/user/
systemctl --user daemon-reload
systemctl --user enable rog-ally-menu.service

# 6. Configure CC button (remap away from Steam)
echo "[6/7] Configuring ROG CC button..."
mkdir -p ~/.config/autostart

PROFILE='version: 1
kind: DeviceProfile
name: ROG Ally Menu
mapping:
- name: CC Button Consumed
  source_event:
    gamepad:
      button: Guide
  target_events:
  - keyboard: KeyPause
- name: LeftTop
  source_event:
    gamepad:
      button: LeftTop
  target_events:
  - gamepad:
      button: LeftPaddle1
- name: RightTop
  source_event:
    gamepad:
      button: RightTop
  target_events:
  - gamepad:
      button: RightPaddle1
- name: Keyboard
  source_event:
    gamepad:
      button: Keyboard
  target_events:
  - gamepad:
      button: Guide
  - gamepad:
      button: North
- name: QuickAccess2
  source_event:
    gamepad:
      button: QuickAccess2
  target_events:
  - gamepad:
      button: Screenshot
- name: Left Dial Counter-clockwise
  source_event:
    gamepad:
      dial:
        name: LeftStickDial
        direction: counter-clockwise
  target_events:
  - keyboard: KeyVolumeDown
- name: Left Dial Clockwise
  source_event:
    gamepad:
      dial:
        name: LeftStickDial
        direction: clockwise
  target_events:
  - keyboard: KeyVolumeUp
- name: Right Dial Counter-clockwise
  source_event:
    gamepad:
      dial:
        name: RightStickDial
        direction: counter-clockwise
  target_events:
  - keyboard: KeyBrightnessDown
- name: Right Dial Clockwise
  source_event:
    gamepad:
      dial:
        name: RightStickDial
        direction: clockwise
  target_events:
  - keyboard: KeyBrightnessUp'

# Apply profile now
busctl call org.shadowblip.InputPlumber \
    /org/shadowblip/InputPlumber/CompositeDevice0 \
    org.shadowblip.Input.CompositeDevice \
    LoadProfileFromYaml s "$PROFILE" 2>/dev/null || true

# Create autostart entry for CC button remap on every login
cat > ~/.config/autostart/rog-ally-menu-cc-button.desktop << DESKTOP
[Desktop Entry]
Type=Application
Name=ROG Ally CC Button Config
Comment=Remap CC button for ROG Ally Command Center
Exec=bash -c 'sleep 5 && busctl call org.shadowblip.InputPlumber /org/shadowblip/InputPlumber/CompositeDevice0 org.shadowblip.Input.CompositeDevice LoadProfileFromYaml s "$PROFILE"'
X-GNOME-Autostart-enabled=true
DESKTOP

# 7. Start the service
echo "[7/7] Starting service..."
systemctl --user start rog-ally-menu.service

echo ""
echo "=========================================="
echo "  Installation complete!"
echo "=========================================="
echo ""
echo "  Press the ROG CC button to open/close."
echo "  The app starts automatically on login."
echo ""
echo "  To uninstall, run: ./uninstall.sh"
echo "=========================================="
