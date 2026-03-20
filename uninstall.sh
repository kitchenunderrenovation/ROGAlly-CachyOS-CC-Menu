#!/bin/bash
# ROG Ally Command Center - Uninstall Script
# Removes all installed files and restores default CC button behavior.

set -e

echo "=========================================="
echo "  ROG Ally Command Center - Uninstaller"
echo "=========================================="
echo ""

# 1. Stop and disable service
echo "[1/4] Stopping service..."
systemctl --user stop rog-ally-menu.service 2>/dev/null || true
systemctl --user disable rog-ally-menu.service 2>/dev/null || true
rm -f ~/.config/systemd/user/rog-ally-menu.service
systemctl --user daemon-reload

# 2. Remove autostart entry
echo "[2/4] Removing CC button config..."
rm -f ~/.config/autostart/rog-ally-menu-cc-button.desktop

# Restore default InputPlumber profile (CC button opens Steam again)
busctl call org.shadowblip.InputPlumber \
    /org/shadowblip/InputPlumber/CompositeDevice0 \
    org.shadowblip.Input.CompositeDevice \
    LoadProfilePath s "/usr/share/inputplumber/profiles/default.yaml" 2>/dev/null || true

# 3. Remove system files
echo "[3/4] Removing system files (requires password)..."
sudo bash -c "
    rm -f /usr/bin/rog-ally-menu
    rm -f /usr/bin/rog-ally-menu-helper
    rm -f /usr/share/polkit-1/actions/com.rogallymenu.pkexec.policy
    rm -f /etc/udev/rules.d/99-rog-ally-menu.rules
    udevadm control --reload-rules 2>/dev/null
    udevadm trigger 2>/dev/null
"

# 4. Kill any running instance
echo "[4/4] Cleaning up..."
killall rog-ally-menu 2>/dev/null || true

echo ""
echo "=========================================="
echo "  Uninstall complete!"
echo "=========================================="
echo ""
echo "  The ROG CC button will open Steam again"
echo "  after reboot (or it already does now)."
echo ""
echo "  Build files remain in this directory."
echo "  Delete this folder to fully remove."
echo "=========================================="
