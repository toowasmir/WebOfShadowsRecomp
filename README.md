> [!IMPORTANT]
> **This project does not include any copyrighted game assets (`default.xex`, `.iso`, `.pak`, `.bik`).**
> You must provide the files from your own legally acquired Xbox 360 copy of *Spider-Man: Web of Shadows* (`Title ID: 41560815`) to install and play **WebOfShadowsRecomp**.

<div align="center">

<img src="assets/wos_icon.png" alt="WebOfShadowsRecomp Icon" width="120">

# Spider-Man: Web of Shadows Recompiled

**An unofficial native Windows x64 port of the Xbox 360 version of *Spider-Man: Web of Shadows* (2008) created through static recompilation.**

[![Latest Release](https://img.shields.io/github/v/release/toowasmir/WebOfShadowsRecomp?label=release&color=e11923)](https://github.com/toowasmir/WebOfShadowsRecomp/releases/latest)
[![Downloads](https://img.shields.io/github/downloads/toowasmir/WebOfShadowsRecomp/total?label=downloads&color=7341f5)](https://github.com/toowasmir/WebOfShadowsRecomp/releases)
[![Stars](https://img.shields.io/github/stars/toowasmir/WebOfShadowsRecomp?style=flat)](https://github.com/toowasmir/WebOfShadowsRecomp/stargazers)
[![Open Issues](https://img.shields.io/github/issues/toowasmir/WebOfShadowsRecomp?label=issues)](https://github.com/toowasmir/WebOfShadowsRecomp/issues)

![Windows x64](https://img.shields.io/badge/Windows-x64%20(AVX2%20%2B%20FMA3)-0078D6?logo=windows&logoColor=white)
![Direct3D 12](https://img.shields.io/badge/Direct3D-12%20(ROV)-5E5E5E)
![AMD FidelityFX](https://img.shields.io/badge/AMD%20FidelityFX-FSR%203%20%2B%202x%20FrameGen-ED1C24?logo=amd&logoColor=white)

### [Download Latest Release](https://github.com/toowasmir/WebOfShadowsRecomp/releases/latest) · [How to Install](#how-to-install) · [Features](#features) · [Controls](#controls) · [Report an Issue](https://github.com/toowasmir/WebOfShadowsRecomp/issues)

</div>

---

## Overview

**WebOfShadowsRecomp** translates the original Xbox 360 PowerPC executable (`default.xex`, Title ID `41560815`) ahead of time into native C++ and compiles it to 64-bit `x86-64` machine code using the **ReXGlue SDK**. Because the game's code runs directly on your CPU with **AVX2 + FMA3** vector instructions rather than inside a dynamic interpreter/JIT emulator, it eliminates emulation overhead while adding modern PC features such as **Direct3D 12** rendering, built-in **AMD FidelityFX Super Resolution (FSR 3 / FSR 2 / FSR 1 / CAS)**, **2x Frame Generation**, hardware **Occlusion Culling**, and a standalone **Modern Dual-Suit Launcher (`WoS_Launcher.exe`)**.

> [!IMPORTANT]
> **WebOfShadowsRecomp** only offers official downloads via [GitHub Releases](https://github.com/toowasmir/WebOfShadowsRecomp/releases). Any third-party website claiming to host this project is unaffiliated.

## Contents

- [Status](#status)
- [Features](#features)
- [System Requirements](#system-requirements)
- [How to Install](#how-to-install)
- [Configuration & Hardware Presets](#configuration--hardware-presets)
- [Controls (Controller, Keyboard & Mouse)](#controls)
- [Building from Source](#building-from-source)
- [Project Layout](#project-layout)
- [Legal & Credits](#legal--credits)

## Status

| Platform | Renderer | Status |
| :--- | :--- | :--- |
| **Windows 10 / 11 (x64)** | Direct3D 12 (ROV / EDRAM) | **Playable** (Boots, plays Bink FMVs, full Manhattan open-world web-swinging, combat, suit switching, saves & loads) |

## Features

| Category | What you get |
| :--- | :--- |
| **Native x86-64 Engine** | Statically recompiled from the Xbox 360 PowerPC binary (`18,622+` functions) into native 64-bit C++ compiled with `clang-cl` (`-O3 -march=haswell -mavx2 -mfma`). |
| **AVX2 + FMA3 & Zero-Stall MXCSR** | PowerPC VMX128 physics, animation skinning, and vector dot-products map directly to hardware `vfmadd213sd`, `vsqrtsd`, and `pshufb` (`_mm_shuffle_epi8`). Redundant per-function `LDMXCSR` (`SetFlushMode`) pipeline stalls (`66,421` call sites) are completely eliminated via persistent per-thread FTZ/DAZ initialization. |
| **Built-in AMD FidelityFX & 2x Frame Generation** | Integrated directly into the D3D12 presenter (`src/hooks/frame_generation.h`) — **no external tools like Lossless Scaling required**. Choose **AMD FSR 3**, **FSR 2**, **FSR 1**, or **CAS** sharpening, and toggle **2x Frame Generation** live in-game at any time with **`F4`**. |
| **Smart VRAM Cache & Manhattan Occlusion Culling** | Hardware D3D12 occlusion queries cull hidden geometry behind Manhattan skyscrapers during high-speed web-swinging, paired with **4 GB**, **6 GB**, and **8+ GB** VRAM texture cache profiles to prevent VRAM thrashing on 4 GB GPUs. |
| **Fast ROV Alpha & Particle Optimization** | Optimized early-alpha discard thresholds (`fuzzy_alpha_epsilon`) reduce pixel shader stalls during heavy explosions, smoke, and symbiote tendril effects. |
| **Standalone Modern Launcher (`WoS_Launcher.exe`)** | Per-Monitor High-DPI frosted-glass launcher with **Red Suit / Symbiote Suit** live theme switching, **English / Turkish** UI localization, **One-Click Hardware Presets**, **Xbox 360 `.ISO` Extractor & Verifier**, **Save Game Backup/Restore Manager**, and **Telemetry Analytics**. |
| **Simultaneous Controller + Keyboard & Mouse** | Full XInput / SDL gamepad support alongside raw **Mouse Camera Look** and customizable **Keyboard & Mouse Keybindings**. |

## System Requirements

- **CPU:** 64-bit x86 processor with **AVX2** and **FMA3** support:
  - **Intel:** 4th Gen Core (Haswell, 2013) or newer
  - **AMD:** Ryzen series or Excavator (2015) or newer
- **GPU:** Direct3D 12 compatible graphics card with Shader Model 6.0 / Rasterizer Ordered Views (ROV):
  - **Minimum (1080p Performance Preset):** NVIDIA GeForce GTX 1050 Ti / GTX 1650 (4 GB VRAM) or AMD Radeon RX 570 (4 GB VRAM)
  - **Recommended (1080p/1440p Balanced & Ultra Presets):** NVIDIA GeForce RTX 2060 / AMD Radeon RX 6600 (6–8+ GB VRAM) or newer
- **Memory:** 8 GB RAM minimum (16 GB recommended)
- **Operating System:** Windows 10 (version 1909+) or Windows 11 (64-bit)
- **Storage:** ~8 GB free disk space for extracted Xbox 360 game assets

## How to Install

1. **Acquire your legal Xbox 360 game copy** of *Spider-Man: Web of Shadows* (`Title ID: 41560815`) as an `.ISO` disc image or an extracted folder containing `default.xex`.
2. Download `WoS_ReComp_Modern_Edition_v2.0_Win64.zip` from the [Latest Release](https://github.com/toowasmir/WebOfShadowsRecomp/releases/latest) page and extract it to any folder on your PC.
3. Run **`WoS_Launcher.exe`** (standalone executable — no Python installation required).
   - *Tip: Switch the launcher language between **English (🇬🇧)** and **Turkish (🇹🇷)** using the flag icons in the top header bar.*
4. Open the **💿 Files** tab in the left navigation rail:
   - Click **💿 Install from .ISO** and select your Xbox 360 *Spider-Man: Web of Shadows* `.iso` file. The built-in `extract-xiso` wizard will automatically extract and verify all game files into the `game/` directory.
   - *(Alternatively, if you already have the extracted Xbox 360 files, copy `default.xex` and all game folders directly into the `game/` folder and click **🔍 Verify Game Files**).*
5. Open the **🏠 General** tab, choose a hardware preset (**Performance (4 GB)**, **Balanced (1080p)**, or **Ultra Quality (2K)**), and click **▶ LAUNCH GAME**!

> [!NOTE]
> **Windows Defender / SmartScreen Notice:** Because `WoS_Launcher.exe` and `bin/wos_recomp.exe` are community-built open-source binaries and are not commercially code-signed, Windows SmartScreen may show an *"Unknown Publisher"* prompt on first launch. Click **More info → Run anyway**.

## Configuration & Hardware Presets

All settings are managed visually through **`WoS_Launcher.exe`** and saved automatically to `recomp_config.json` (and synced to `bin/recomp_config.json` and `wos_settings.toml` on launch):

- **Performance (4 GB):** Tailored for 4 GB VRAM GPUs and laptops. Sets 1x internal render scale (`1280x720`) upscaled to `1080p` via **AMD FSR 3 (Quality)** + **2x Frame Generation** + aggressive VRAM cache eviction.
- **Balanced (1080p):** Native `1080p` output with **AMD FSR 3**, **8 GB VRAM** cache profile, hardware occlusion culling, and 60 FPS frame pacing.
- **Ultra Quality (2K):** Enables **2x Internal Resolution Scale (`2560x1440` supersampling)** with **AMD FSR 3 (Quality)** and **8+ GB VRAM** cache limits for razor-sharp webbing and suit textures.

## Controls

Both **XInput / SDL Controllers** and **Keyboard & Mouse** work simultaneously. Every key can be rebound in the **🎮 Controls** tab of `WoS_Launcher.exe`.

| Action | Default Keyboard & Mouse | Xbox 360 Controller |
| :--- | :--- | :--- |
| **Move** | `W`, `A`, `S`, `D` | Left Stick (LS) |
| **Camera Look** | `Mouse Movement` | Right Stick (RS) |
| **Web-Swing / Sprint** | `Left Shift (LShift)` | Right Trigger (`RT`) |
| **Lock-On / Wall Run** | `Right Mouse Button (RMB)` | Left Trigger (`LT`) |
| **Jump** | `Space` | `A` |
| **Attack** | `Left Mouse Button (LMB)` | `X` |
| **Web Strike** | `F` | `Y` |
| **Web Shot / Tendril** | `C` | `B` |
| **Switch Suit (Red / Black Symbiote)** | `Left Alt (LAlt)` | Left Stick Click (`L3`) |
| **Summon Ally / Special Assist** | `Q` / `E` | `LB` / `RB` |
| **Center Camera** | `V` | Right Stick Click (`R3`) |
| **Upgrade / Map Menu** | `Tab` | `Start` |
| **Pause Menu** | `Escape` | `Back` |
| **Toggle 2x Frame Generation (Live)** | **`F4`** *(In-Game Hotkey)* | — |

## Building from Source

To modify the engine hooks (`src/hooks/`) or rebuild `wos_recomp.exe` from source:

1. Install **Visual Studio 2022** (Desktop development with C++ workload + Clang/LLVM toolset), **CMake 3.25+**, **Ninja**, and the **ReXGlue SDK**.
2. Apply the zero-stall MXCSR and AVX2/SSSE3 header patches from `patches/context.h` and `patches/intrinsics.h`.
3. Configure and build the release binary with `clang-cl` and `lld-link`:
   ```powershell
   cmake --preset win-amd64-release
   cmake --build out/build/win-amd64-release --target wos_recomp
   ```
4. To rebuild the standalone launcher executable (`WoS_Launcher.exe`):
   ```powershell
   pip install pyinstaller pillow
   pyinstaller --noconfirm --onefile --windowed --icon="assets/wos_icon.ico" --add-data "assets;assets" --distpath "." --name "WoS_Launcher" launcher.py
   ```

## Reporting a Problem & Crash Logs

Every time the game is launched via **`WoS_Launcher.exe`** or **`Play_WoS.bat`**, the engine automatically records a full diagnostic log (including Direct3D 12 initialization, CVAR states, AMD FSR 3 / FrameGen telemetry, and our built-in Vectored Exception Handler `[WoS-CRASH]` stack trace in case of an access violation) into the **`logs/`** directory:

- **Latest Session Log:** `logs/wos_recomp_latest.log`
- **Previous Sessions (Up to 10 kept automatically):** `logs/wos_session_YYYYMMDD_HHMMSS.log`

**How to attach logs to a GitHub Issue:**
1. Open **`WoS_Launcher.exe`** and click the **📊 Stats (`Analiz`)** tab on the left navigation rail.
2. Click **📦 Export Bug Report (`.ZIP`)** (`Hata Raporu (.ZIP) Oluştur`).
3. This creates `logs/WoS_BugReport_<timestamp>.zip` containing your latest logs, `recomp_config.json`, and `last_session_stats.json` (with zero personal/save data), which you can drag and drop directly into a [GitHub Issue](https://github.com/toowasmir/WebOfShadowsRecomp/issues).

## Project Layout

```text
WebOfShadowsRecomp/
├── WoS_Launcher.exe          # Standalone Modern Dual-Suit Launcher (No Python required)
├── Launcher.bat              # Quick-start launcher wrapper
├── Play_WoS.bat              # Direct game boot script (bypasses launcher UI)
├── launcher.py               # Launcher source code (Tkinter High-DPI Canvas)
├── recomp_config.json        # User configuration & keybindings
├── assets/                   # 4K-supersampled Red/Symbiote UI themes & icons
├── bin/
│   ├── wos_recomp.exe        # Native x86-64 AVX2+FMA3 recompiled game engine
│   ├── rexruntime.dll        # ReXGlue Xbox 360 kernel, audio & input runtime
│   └── rexgpu-xenos.dll      # Direct3D 12 Xenos GPU & shader presenter
├── game/                     # Extract your Xbox 360 ISO files (default.xex) here
├── logs/                     # Automatic session & crash logs (wos_recomp_latest.log)
├── patches/                  # Zero-stall MXCSR & AVX2/SSSE3 intrinsic patches
├── src/
│   ├── main.cpp              # Engine entry point, config loader & D3D12 CVAR bridge
│   ├── hooks/                # AMD FSR 3 + 2x FrameGen, hash hooks & perf hooks
│   └── stubs/                # Kernel & unregistered function stubs
└── tools/
    └── extract-xiso.exe      # Built-in Xbox 360 ISO extraction utility
```

## Legal & Credits

- **Disclaimer:** This project is a non-commercial, unofficial static recompilation effort for software preservation and interoperability. **No Xbox 360 game code, assets, textures, audio, or copyrighted files from *Spider-Man: Web of Shadows* are distributed in this repository or its releases.**
- *Spider-Man: Web of Shadows* was originally developed by **Shaba Games** and **Treyarch** and published by **Activision**.
- Built with the **ReXGlue SDK** (derived from the **Xenia** Xbox 360 research project) and **AMD FidelityFX** shader technology.
