#!/bin/bash
# Configure the ROG CC button to open the Command Center instead of Steam
# This loads a custom InputPlumber profile that remaps the Guide button
# so Steam no longer intercepts it.
#
# The app itself detects the CC button directly via the hidraw device,
# so this script only needs to BLOCK Steam from seeing the Guide button.

set -e

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

# Apply the profile now
echo "Loading custom InputPlumber profile..."
if busctl call org.shadowblip.InputPlumber \
    /org/shadowblip/InputPlumber/CompositeDevice0 \
    org.shadowblip.Input.CompositeDevice \
    LoadProfileFromYaml s "$PROFILE" 2>/dev/null; then
    echo "InputPlumber profile loaded - CC button will open Command Center instead of Steam."
else
    echo "WARNING: Could not load InputPlumber profile."
    echo "InputPlumber may not be running. The CC button will still open the menu,"
    echo "but Steam may also open its overlay."
fi

# Install autostart script so the profile is loaded on every login
AUTOSTART_DIR="${XDG_CONFIG_HOME:-$HOME/.config}/autostart"
mkdir -p "$AUTOSTART_DIR"

cat > "$AUTOSTART_DIR/rog-ally-menu-cc-button.desktop" << 'DESKTOP'
[Desktop Entry]
Type=Application
Name=ROG Ally CC Button Config
Comment=Remap CC button away from Steam for ROG Ally Menu
Exec=bash -c 'sleep 3 && /usr/share/rog-ally-menu/configure-cc-button.sh'
X-GNOME-Autostart-enabled=true
DESKTOP

# Also install the script to a shared location
sudo mkdir -p /usr/share/rog-ally-menu
sudo cp "$0" /usr/share/rog-ally-menu/configure-cc-button.sh
sudo chmod +x /usr/share/rog-ally-menu/configure-cc-button.sh

echo "Autostart entry created - profile will be applied on every login."
