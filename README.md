# ROG Ally Command Center for Linux

A native Linux overlay menu for the ASUS ROG Ally, inspired by the Windows ROG Command Center. Built with Qt6/QML and designed for CachyOS Deckify.

## Features

- **Overlay panel** — opens on top of fullscreen games without minimizing them
- **ROG CC button** — press to open/close, just like on Windows
- **D-pad / arrow key navigation** — navigate tiles and activate with A button or Enter
- **Operating Mode** — cycle between Silent, Balanced, and Performance (auto-maxes TDP)
- **LED Control** — cycle brightness (0%/10%/100%) and colors (Dark Red, Red, Blue, Green, Orange, White)
- **Screen Brightness & Volume** — vertical sliders on the sidebar
- **Quick actions** — Take Screenshot, Show Keyboard, Show Desktop, End Task, Airplane Mode
- **Live stats** — battery %, wattage draw, WiFi/Bluetooth status, clock

## Requirements

- ASUS ROG Ally (RC71L) running Arch Linux or CachyOS
- InputPlumber (for CC button remapping)
- KDE Plasma / KWin (for Show Desktop, virtual keyboard)
- PipeWire + WirePlumber (for volume control)

## Install

```bash
git clone https://github.com/YOUR_USERNAME/rog-ally-menu.git
cd rog-ally-menu
./install.sh
```

That's it. The app auto-starts on login and the CC button is remapped.

## Uninstall

```bash
./uninstall.sh
```

This removes all installed files and restores the CC button to its default behavior (opening Steam).

## Usage

| Input | Action |
|-------|--------|
| CC button | Toggle overlay |
| D-pad / Arrow keys | Move selection |
| A button / Enter | Activate tile |
| Escape | Close overlay |
| X button (top right) | Close overlay |

## Building manually

```bash
sudo pacman -S qt6-base qt6-declarative layer-shell-qt cmake polkit grim
cmake -B build -DCMAKE_BUILD_TYPE=Release -DCMAKE_INSTALL_PREFIX=/usr
cmake --build build -j$(nproc)
```

## License

MIT
