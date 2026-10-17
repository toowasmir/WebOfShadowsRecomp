#!/usr/bin/env python3
"""
Spider-Man: Web of Shadows - PC Recompilation Launcher
Native 64-bit PC Port based on ReXGlue & Xbox 360 Static Recompilation
"""

import os
import sys
import json
import subprocess
import tkinter as tk
from tkinter import ttk, messagebox

CONFIG_FILE = "recomp_config.json"
DEFAULT_CONFIG = {
    "resolution": "1920x1080",
    "fullscreen": False,
    "vsync": True,
    "resolution_scale": 1,
    "present_effect": "cas",
    "mnk_mouse": True
}

def load_config():
    if os.path.exists(CONFIG_FILE):
        try:
            with open(CONFIG_FILE, "r") as f:
                cfg = json.load(f)
                return {**DEFAULT_CONFIG, **cfg}
        except Exception:
            pass
    return DEFAULT_CONFIG.copy()

def save_config(cfg):
    try:
        with open(CONFIG_FILE, "w") as f:
            json.dump(cfg, f, indent=2)
    except Exception as e:
        print(f"Config save error: {e}")

class RecompLauncher:
    def __init__(self, root):
        self.root = root
        self.root.title("Spider-Man: Web of Shadows - PC Recompilation Launcher")
        self.root.geometry("680x560")
        self.root.resizable(False, False)
        self.root.configure(bg="#121318")

        self.config = load_config()
        self.create_widgets()

    def create_widgets(self):
        # Header banner frame
        header_frame = tk.Frame(self.root, bg="#1a1c24", height=90)
        header_frame.pack(fill="x", side="top")
        header_frame.pack_propagate(False)

        title_lbl = tk.Label(
            header_frame,
            text="SPIDER-MAN: WEB OF SHADOWS",
            font=("Segoe UI", 18, "bold"),
            fg="#e53935",
            bg="#1a1c24"
        )
        title_lbl.pack(pady=(12, 0))

        sub_lbl = tk.Label(
            header_frame,
            text="Xbox 360 to 64-bit PC Native Recompilation (Direct3D 12)",
            font=("Segoe UI", 10),
            fg="#90caf9",
            bg="#1a1c24"
        )
        sub_lbl.pack(pady=(2, 6))

        # Main content container
        content = tk.Frame(self.root, bg="#121318", padx=25, pady=15)
        content.pack(fill="both", expand=True)

        # Settings Group
        settings_box = tk.LabelFrame(
            content,
            text=" GÖRÜNTÜ VE GRAFİK AYARLARI ",
            font=("Segoe UI", 11, "bold"),
            fg="#e0e0e0",
            bg="#1a1c24",
            padx=15,
            pady=12,
            bd=1,
            relief="solid"
        )
        settings_box.pack(fill="x", pady=8)

        # Resolution dropdown
        tk.Label(settings_box, text="Pencere / Ekran Çözünürlüğü:", font=("Segoe UI", 10), fg="#b0bec5", bg="#1a1c24").grid(row=0, column=0, sticky="w", pady=6)
        self.res_var = tk.StringVar(value=self.config.get("resolution", "1920x1080"))
        res_cb = ttk.Combobox(settings_box, textvariable=self.res_var, state="readonly", width=18, font=("Segoe UI", 10))
        res_cb["values"] = ("1280x720", "1600x900", "1920x1080", "2560x1440", "3840x2160")
        res_cb.grid(row=0, column=1, sticky="e", padx=10, pady=6)

        # Resolution scale (Internal Rendering Resolution)
        tk.Label(settings_box, text="Dahili Çizim Çarpanı (Resolution Scale):", font=("Segoe UI", 10), fg="#b0bec5", bg="#1a1c24").grid(row=1, column=0, sticky="w", pady=6)
        self.scale_var = tk.IntVar(value=self.config.get("resolution_scale", 1))
        scale_cb = ttk.Combobox(settings_box, textvariable=self.scale_var, state="readonly", width=18, font=("Segoe UI", 10))
        scale_cb["values"] = (1, 2)
        scale_cb.grid(row=1, column=1, sticky="e", padx=10, pady=6)

        # Post Processing Filter
        tk.Label(settings_box, text="Netleştirme / Filtre (Presenter Effect):", font=("Segoe UI", 10), fg="#b0bec5", bg="#1a1c24").grid(row=2, column=0, sticky="w", pady=6)
        self.fx_var = tk.StringVar(value=self.config.get("present_effect", "cas"))
        fx_cb = ttk.Combobox(settings_box, textvariable=self.fx_var, state="readonly", width=18, font=("Segoe UI", 10))
        fx_cb["values"] = ("bilinear", "cas", "fsr")
        fx_cb.grid(row=2, column=1, sticky="e", padx=10, pady=6)

        # Checkboxes (Fullscreen, VSync)
        chk_frame = tk.Frame(settings_box, bg="#1a1c24")
        chk_frame.grid(row=3, column=0, columnspan=2, sticky="w", pady=8)

        self.fs_var = tk.BooleanVar(value=self.config.get("fullscreen", False))
        fs_chk = tk.Checkbutton(
            chk_frame, text="Tam Ekran (Fullscreen)", variable=self.fs_var,
            font=("Segoe UI", 10), fg="#eceff1", bg="#1a1c24", selectcolor="#263238", activebackground="#1a1c24"
        )
        fs_chk.pack(side="left", padx=(0, 20))

        self.vsync_var = tk.BooleanVar(value=self.config.get("vsync", True))
        vsync_chk = tk.Checkbutton(
            chk_frame, text="Dikey Eşitleme (V-Sync 60 FPS)", variable=self.vsync_var,
            font=("Segoe UI", 10), fg="#eceff1", bg="#1a1c24", selectcolor="#263238", activebackground="#1a1c24"
        )
        vsync_chk.pack(side="left")

        # Controls info group
        ctrl_box = tk.LabelFrame(
            content,
            text=" KONTROLLER & GİRDİ BİLGİSİ ",
            font=("Segoe UI", 10, "bold"),
            fg="#90caf9",
            bg="#1a1c24",
            padx=15,
            pady=8,
            bd=1,
            relief="solid"
        )
        ctrl_box.pack(fill="x", pady=6)

        info_text = (
            "• Klavye & Fare: WASD (Hareket), Fare (Kamera), Space (Zıplama/A), Enter/Sol Tık (Saldırı/X)\n"
            "• Web Strike (Y): Sağ Tık / P | Web Swing (RT): Shift | Menü / Duraklat: Enter veya Esc\n"
            "• Gamepad: Xbox, PlayStation DualSense veya uyumlu tüm kollar otomatik desteklenir (SDL3/XInput)."
        )
        tk.Label(
            ctrl_box,
            text=info_text,
            font=("Segoe UI", 9),
            justify="left",
            fg="#cfd8dc",
            bg="#1a1c24"
        ).pack(anchor="w")

        # Action Buttons frame
        btn_frame = tk.Frame(self.root, bg="#121318", pady=15)
        btn_frame.pack(fill="x", side="bottom")

        launch_btn = tk.Button(
            btn_frame,
            text="▶  OYUNU BAŞLAT (PLAY)",
            font=("Segoe UI", 13, "bold"),
            bg="#d32f2f",
            fg="#ffffff",
            activebackground="#b71c1c",
            activeforeground="#ffffff",
            relief="flat",
            padx=25,
            pady=10,
            cursor="hand2",
            command=self.launch_game
        )
        launch_btn.pack(side="right", padx=30)

        status_lbl = tk.Label(
            btn_frame,
            text="Durum: D3D12 Recomp Binary Hazır (60 MB)",
            font=("Segoe UI", 9, "italic"),
            fg="#4caf50",
            bg="#121318"
        )
        status_lbl.pack(side="left", padx=30)

    def launch_game(self):
        # Save current settings
        res = self.res_var.get()
        w, h = res.split("x") if "x" in res else ("1280", "720")
        self.config["resolution"] = res
        self.config["fullscreen"] = self.fs_var.get()
        self.config["vsync"] = self.vsync_var.get()
        self.config["resolution_scale"] = self.scale_var.get()
        self.config["present_effect"] = self.fx_var.get()
        save_config(self.config)

        # Locate executable
        exe_path = os.path.abspath("out/build/win-amd64-release/wos_recomp.exe")
        if not os.path.exists(exe_path):
            messagebox.showerror("Hata", f"Oyun çalıştırılabilir dosyası bulunamadı:\n{exe_path}")
            return

        work_dir = os.path.dirname(exe_path)
        cmd = [
            exe_path,
            f"--window_width={w}",
            f"--window_height={h}",
            f"--fullscreen={'true' if self.fs_var.get() else 'false'}",
            f"--vsync={'true' if self.vsync_var.get() else 'false'}",
            f"--resolution_scale={self.scale_var.get()}",
            f"--present_effect={self.fx_var.get()}"
        ]

        try:
            subprocess.Popen(cmd, cwd=work_dir)
            self.root.destroy()
        except Exception as e:
            messagebox.showerror("Başlatma Hatası", f"Oyun başlatılırken hata oluştu:\n{e}")

if __name__ == "__main__":
    root = tk.Tk()
    app = RecompLauncher(root)
    root.mainloop()
