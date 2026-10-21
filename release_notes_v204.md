## Spider-Man: Web of Shadows Recompiled — Modern Edition v2.0.4 Hotfix

### 🌟 What's New & Fixed in v2.0.4

1. **Fixed Modifier Movement Freeze (WASD + Shift / Ctrl / Alt):**
   - Eliminated the strict keyboard modifier check in `rexruntime.dll` that caused Spider-Man to freeze dead in place whenever `Shift`, `Ctrl`, or `Alt` was pressed while moving.
   - You can now sprint, web-swing, or change costumes fluidly while holding `W`, `A`, `S`, or `D`.

2. **Fixed Standalone Shift & Alt Bindings:**
   - Standalone `Shift` (Right Trigger / Web-Swing) and `Alt` (Left Stick Click / Costume Switch) now reliably register in-game as active button presses without requiring combination keys.

3. **Complete Mouse Button Binding Support:**
   - The Launcher's Controls tab now natively supports binding Left Mouse Button (`LMB`), Right Mouse Button (`RMB`), Middle Mouse Button (`MMB`), and side buttons (`Mouse4` and `Mouse5`).
   - Integrated an ultra-low latency background forwarder for mouse side buttons with <4ms response time, ensuring rapid web-strikes and combat maneuvers feel completely seamless.

4. **Resolved Mouse Movement / Click Crash:**
   - Fixed a memory address conflict in `MnkInputDriver` where mouse clicks or camera movement could corrupt virtual table pointers and cause an instant crash (`0xC0000005`).

5. **Launcher UI Polish & Version Badges:**
   - Added prominent `v2.0.4` neon badges in the top header, top-right window bar, bottom launch dock, and the telemetry dashboard.

---

### 📦 Installation
1. Download **`WoS_ReComp_Modern_Edition_v2.0.4_Win64.zip`** below and extract it anywhere on your PC.
2. Launch **`WoS_Launcher.exe`** (standalone — no Python required).
3. Open the **Files** tab, click **Install from .ISO** and select your legal Xbox 360 copy of *Spider-Man: Web of Shadows* (`default.xex`).
4. Click **▶ LAUNCH GAME**!
