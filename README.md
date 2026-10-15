# Rogue Legacy C++ (Raylib)

A faithful, high-performance remake of **Rogue Legacy** written in **C++17** using **Raylib**.
Featuring **100% pure procedural rendering** (no external sprite sheets required) and **synthesized real-time 8-bit sound effects**.

![Gameplay Preview](https://raw.githubusercontent.com/BohatyrovIvanUcu/rogue_legacy/main/preview.png)

---

## Key Features

- **Generational Lineage System**: Choose between 3 randomized heirs upon every death, each with distinct classes, spells, and genetic traits (e.g. Gigantism, Dwarfism, IBS, Nostalgia, Alzheimers, Dementia, Colorblindness, PAD, ADHD).
- **Extensive Class Roster**:
  - Knight / Paladin (Shield Block)
  - Barbarian / Barbarian King (Ground Shouts)
  - Mage / Archmage (Spell Rotation)
  - Assassin / Shinobi (Shadow Mist Form)
  - Shinobi / Hokage (Speed Dash)
  - Miner / Spelunker (Headlamp & Ore Radar)
  - Lich / Lich King (Max HP conversion)
  - Dragon (Omnidirectional Free Flight & Fireballs)
- **Procedural Castle Generation**:
  - Dynamically generated castle layouts spanning 4 Biomes: *Castle Hamson*, *Forest Abkhazia*, *The Maya*, *The Land of Darkness*.
  - Fairy Chest trials (No Damage, No Jumping, Speed Trial, Defeat All).
  - Secret mechanics: breakable cracked secret walls (`'X'`), hidden illusion walls (`'I'`), dwarf tunnels (`'D'`), and secret platform drops.
- **Epic Boss Battles**:
  - **Khidr** (Castle Hamson)
  - **Alexander** (Forest Abkhazia)
  - **Ponce de Leon** (The Maya)
  - **Herodotus** (The Land of Darkness)
  - **Johannes the Traitor & The Fountain** (Final Boss)
- **Persistent Manor Upgrade Tree**:
  - Expand your ancestral castle between runs to boost Health, Mana, Strength, Armor, Crit Chance, and unlock new classes.
- **Charon's Gold Toll**:
  - Pay Charon all leftover gold upon re-entering the castle (unless upgraded).
- **Architect & Teleporter Network**:
  - Lock down castle layouts or fast-travel between unlocked teleporter shrines.
- **Equipment & Runes**:
  - Blacksmith & Enchantress systems with blueprints, Vault runes (Dash, Double Jump, Vampirism, Grace).

---

## Controls

| Action | Primary Key | Secondary / Alternative |
|---|---|---|
| **Move Left / Right** | `A` / `D` | `Left` / `Right` Arrow |
| **Look Up / Down** | `W` / `S` | `Up` / `Down` Arrow |
| **Jump** | `Space` | Variable jump height |
| **Sword Slash** | `J` | `Z` / `Left Mouse Button` |
| **Down-Strike (Pogo Bounce)** | `S` + `J` (in air) | `Down` + `Slash` on spikes/enemies |
| **Cast Spell** | `L` | `X` / `Right Mouse Button` |
| **Class Talent** | `Q` | `E` / `Left Ctrl` (Shield, Shout, Mist) |
| **Dash** | `K` | `C` / `Left Shift` |
| **Interact / Enter Door** | `W` | `E` / `Up Arrow` |
| **Drop Through Platform** | `S` + `Space` | `Down` + `Jump` |

---

## Quick Download (No Compilation Required)

🎮 **Want to play immediately?** Check [**GitHub Releases**](https://github.com/BohatyrovIvanUcu/rogue_legacy/releases/latest) for standalone precompiled builds:

### 🐧 Linux (64-bit) — 1-Click Executable (Just like an `.exe`!)
- **Recommended (Standalone AppImage)**:
  1. Download 👉 **[RogueLegacy-x86_64.AppImage](https://github.com/BohatyrovIvanUcu/rogue_legacy/releases/download/latest/RogueLegacy-x86_64.AppImage)**
  2. Allow execution (right-click -> *Properties* -> *Allow executing file as program*, or `chmod +x RogueLegacy-x86_64.AppImage`).
  3. **Double-click to play!** No installation, no compiler, and no package manager required.
- **Alternative (Tarball & Desktop Shortcut)**:
  1. Download 👉 **[rogue_legacy_linux_x64.tar.gz](https://github.com/BohatyrovIvanUcu/rogue_legacy/releases/download/latest/rogue_legacy_linux_x64.tar.gz)**
  2. Extract: `tar -xvf rogue_legacy_linux_x64.tar.gz`
  3. Run `./start_game.sh` to play, or `./install_desktop.sh` to add the game with its icon to your system Applications Menu!

### 🪟 Windows (64-bit)
1. Download 👉 **[rogue_legacy_windows_x64.zip](https://github.com/BohatyrovIvanUcu/rogue_legacy/releases/download/latest/rogue_legacy_windows_x64.zip)**
2. Extract the `.zip` archive completely into a folder.
3. Double-click `rogue_legacy.exe` (or `start_game.bat`)!

---

## Building from Source

### Prerequisites
- CMake 3.20+
- C++20 compatible compiler (GCC 11+, Clang 13+, or MSVC 2019+)
- Git (Raylib 5.0 and nlohmann/json are fetched automatically via CMake FetchContent)
- On Linux (Ubuntu/Debian):
  ```bash
  sudo apt install -y build-essential cmake libasound2-dev libx11-dev libxrandr-dev libxi-dev libgl1-mesa-dev libglu1-mesa-dev libxcursor-dev libxinerama-dev
  ```

### Windows Local Build:
- Double-click `build_windows.bat` in the repository root.
  *(Or run `cmake -B build -DCMAKE_BUILD_TYPE=Release && cmake --build build --config Release -j`)*

### Linux Local Build:
- Run `./build_linux.sh` in the terminal:
  ```bash
  chmod +x build_linux.sh
  ./build_linux.sh
  ```
- Or build manually:
  ```bash
  cmake -B build -DCMAKE_BUILD_TYPE=Release
  cmake --build build -j$(nproc)
  ./build/rogue_legacy_cpp
  ```

---

## License
MIT License. Created for educational and enthusiast purposes.

