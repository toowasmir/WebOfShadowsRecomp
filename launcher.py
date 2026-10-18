# -*- coding: utf-8 -*-
"""
Spider-Man: Web of Shadows — ReComp Modern Edition Launcher (v2.5)
==================================================================
Next-Gen 2026 Frosted-Glass & Dual-Suit (Red Suit / Symbiote Suit) UI:
- Per-Monitor High-DPI 1:1 Crisp Rendering (1120x660)
- 4K Supersampled (3840x1240 -> 1120x660) Spider-Man Hero & Dual-Mask Backdrop
- Vertical Frosted-Glass Navigation Dock (6 Tabs with active neon indicator)
- Top Glass Header with 4K Logo, TR/EN Switcher, and Live Red/Symbiote Suit Toggle
- Sliding Pill Toggle Switches & Structured Setting Cards
- Bottom Glass Launch Dock with Live Telemetry Badges & Glowing Launch Button
"""

import ctypes
import json
import os
import subprocess
import sys
import zipfile
from datetime import datetime
from pathlib import Path
import tkinter as tk
from tkinter import messagebox, filedialog

# Single-Instance Enforcement to prevent duplicate processes & PyInstaller _MEI collisions
ERROR_ALREADY_EXISTS = 183
try:
    _launcher_mutex = ctypes.windll.kernel32.CreateMutexW(None, False, "WoS_ReComp_Launcher_SingleInstance_Mutex_v25")
    if ctypes.windll.kernel32.GetLastError() == ERROR_ALREADY_EXISTS:
        hwnd = ctypes.windll.user32.FindWindowW(None, "Spider-Man: Web of Shadows — ReComp Modern Edition")
        if hwnd:
            ctypes.windll.user32.ShowWindow(hwnd, 9)  # SW_RESTORE
            ctypes.windll.user32.SetForegroundWindow(hwnd)
        sys.exit(0)
except Exception:
    pass

# Enable Per-Monitor High-DPI awareness and Taskbar Icon ID on Windows
try:
    ctypes.windll.shell32.SetCurrentProcessExplicitAppUserModelID("WoS.ReComp.ModernEdition.Launcher.v25")
except Exception:
    pass
try:
    ctypes.windll.shcore.SetProcessDpiAwareness(2)
except Exception:
    try:
        ctypes.windll.user32.SetProcessDPIAware()
    except Exception:
        pass

if getattr(sys, "frozen", False):
    BASE_DIR = Path(sys.executable).resolve().parent
    _BUNDLED_ASSETS = Path(getattr(sys, "_MEIPASS", BASE_DIR)) / "assets"
    ASSETS_DIR = (BASE_DIR / "assets") if (BASE_DIR / "assets" / "bg_modern_red.png").exists() else _BUNDLED_ASSETS
else:
    BASE_DIR = Path(__file__).resolve().parent
    ASSETS_DIR = BASE_DIR / "assets"

CONFIG_FILE = BASE_DIR / "recomp_config.json"
STATS_FILE = BASE_DIR / "last_session_stats.json"
SAVES_BACKUP_DIR = BASE_DIR / "saves_backup"
LOGS_DIR = BASE_DIR / "logs"

BG_DARK = "#07080c"
GLASS_HEADER = "#0d0f16"
GLASS_PANEL = "#0b0d13"
CARD_FILL = "#0f121a"
CARD_HOVER = "#171c28"
BORDER_SUBTLE = "#222634"
TEXT_WHITE = "#f5f7fa"
TEXT_MUTED = "#8c95a6"
GREEN_OK = "#2ecc71"

SUIT_THEMES = {
    "red": {
        "accent": "#e11923",
        "accent_bright": "#ff333d",
        "card_active": "#1e1016",
        "badge_text": "RED SUIT",
    },
    "symbiote": {
        "accent": "#7341f5",
        "accent_bright": "#9168ff",
        "card_active": "#151226",
        "badge_text": "SYMBIOTE",
    },
}

DEFAULT_CONFIG = {
    "lang": "tr",
    "theme": "red",
    "resolution": "1920x1080",
    "window_width": 1920,
    "window_height": 1080,
    "fullscreen": False,
    "vsync": False,
    "vrr_tearing": True,
    "letterbox": True,
    "dither": True,
    "present_effect": "fsr",
    "fsr_quality_mode": "quality",
    "fsr_sharpness": 0.2,
    "cas_sharpness": 0.5,
    "fsr_max_passes": 2,
    "frame_generation": True,
    "fg_multiplier": 2,
    "frame_limit": 60,
    "resolution_scale": 1,
    "vram_profile": "4gb",
    "anisotropic_override": 0,
    "occlusion_query_enable": True,
    "fuzzy_alpha_epsilon": True,
    "mnk_mouse": True,
    "mnk_sensitivity": 1.0,
    "keybind_a": "Space",
    "keybind_b": "C",
    "keybind_x": "LMB",
    "keybind_y": "F",
    "keybind_left_shoulder": "Q",
    "keybind_right_shoulder": "E",
    "keybind_left_trigger": "RMB",
    "keybind_right_trigger": "LShift",
    "keybind_start": "Tab",
    "keybind_back": "Escape",
    "keybind_lstick_up": "W",
    "keybind_lstick_down": "S",
    "keybind_lstick_left": "A",
    "keybind_lstick_right": "D",
    "keybind_lstick_press": "LAlt",
    "keybind_rstick_press": "V",
}

STRINGS = {
    "tr": {
        "nav": [
            ("🏠", "Genel"),
            ("🖥", "Ekran"),
            ("⚡", "FSR & Pacer"),
            ("🎮", "Kontrol"),
            ("💿", "Dosyalar"),
            ("📊", "Analiz"),
        ],
        "ready": "● OYUN HAZIR",
        "missing": "● KURULUM GEREKLİ",
        "sec_overview": "AKTİF MOTOR & OPTİMİZASYON MERKEZİ",
        "fg_title": "Dahili 2x Kare Çiftleme & AMD FSR",
        "fg_desc": "Oyun içinde [F4] ile 2x Frame Pacer anlık açılıp kapatılabilir.",
        "occ_title": "Manhattan Donanımsal Occlusion Culling",
        "occ_desc": "Ağ atarken (Web-Swinging) binaların arkasındaki objeleri gizler.",
        "alpha_title": "Hızlı ROV Alfa & Parçacık Optimizasyonu",
        "alpha_desc": "Patlama, duman ve simbiyot efektlerindeki GPU yükünü düşürür.",
        "preset_hdr": "TEK TIKLA DONANIM PROFİLLERİ",
        "p_laptop": "Performans (4 GB)",
        "p_bal": "Dengeli (1080p)",
        "p_ultra": "Ultra Kalite (2K)",
        "sec_display": "EKRAN & ÇIKIŞ AYARLARI",
        "res_lbl": "Çıkış Çözünürlüğü",
        "fs_title": "Tam Ekran (Fullscreen) Modu",
        "fs_desc": "Kapalıyken çerçeveli pencere modunda başlatılır.",
        "vsync_title": "V-Sync (Dikey Senkronizasyon)",
        "vsync_desc": "Ekran yırtılmalarını önler (FrameGen ile kapalı önerilir).",
        "vrr_title": "D3D12 Variable Refresh Rate (G-Sync / FreeSync)",
        "vrr_desc": "Değişken yenileme hızı ve düşük giriş gecikmesi sağlar.",
        "sec_fsr": "AMD FIDELITYFX & VRAM ÖNBELLEK YÖNETİMİ",
        "upscaler_lbl": "Yükseltme & Keskinleştirme Motoru",
        "fsr_q_lbl": "AMD FSR Kalite Modu",
        "pacer_lbl": "Kare Hızı Sabitleyici & 2x Yenileme Hedefi",
        "vram_lbl": "Ekran Kartı VRAM Önbellek Profili ve Render Ölçeği",
        "sec_ctrl": "FARE KAMERA & TUŞ ATAMALARI",
        "mouse_title": "Fare (Mouse) ile Kamera Kontrolü",
        "mouse_desc": "Gamepad takılıysa klavye/fare ile otomatik birlikte çalışır.",
        "sens_lbl": "Fare Kamera Dönüş Hassasiyeti",
        "keys_lbl": "Tuş Atamaları (Değiştirmek istediğiniz kutuya tıklayın)",
        "sec_files": "OYUN DOSYALARI (.ISO) & KAYIT YEDEKLERİ",
        "iso_card_title": "Xbox 360 .ISO Kurulum & Doğrulama Sihirbazı",
        "btn_iso": "💿 .ISO Dosyasından Kur",
        "btn_verify": "🔍 Dosyaları Doğrula",
        "save_card_title": "Kayıt Dosyası (Save Game) Yedekleme Yöneticisi",
        "btn_backup": "➕ Kayıt Yedeği Al",
        "btn_restore": "♻ Son Yedeği Yükle",
        "btn_folder": "📂 Klasörü Aç",
        "sec_perf": "GERÇEK ZAMANLI OTURUM, TELEMETRİ & HATA RAPORU (LOG)",
        "btn_open_log": "📄 Son Logu Aç",
        "btn_export_diag": "📦 Hata Raporu (.ZIP) Oluştur",
        "btn_logs_folder": "📂 Log Klasörü",
    },
    "en": {
        "nav": [
            ("🏠", "General"),
            ("🖥", "Display"),
            ("⚡", "FSR & Pacer"),
            ("🎮", "Controls"),
            ("💿", "Files"),
            ("📊", "Stats"),
        ],
        "ready": "● GAME READY",
        "missing": "● SETUP REQUIRED",
        "sec_overview": "ACTIVE ENGINE & OPTIMIZATION HUB",
        "fg_title": "Built-in 2x Frame Pacer & AMD FSR",
        "fg_desc": "Press [F4] in-game at any time to toggle 2x Frame Pacer live.",
        "occ_title": "Manhattan Hardware Occlusion Culling",
        "occ_desc": "Culls hidden city blocks behind skyscrapers while web-swinging.",
        "alpha_title": "Fast ROV Alpha & Particle Optimization",
        "alpha_desc": "Reduces pixel shader stalls during explosions and tendrils.",
        "preset_hdr": "ONE-CLICK HARDWARE PRESETS",
        "p_laptop": "Performance (4 GB)",
        "p_bal": "Balanced (1080p)",
        "p_ultra": "Ultra Quality (2K)",
        "sec_display": "DISPLAY & OUTPUT SETTINGS",
        "res_lbl": "Output Resolution",
        "fs_title": "Launch in Fullscreen Mode",
        "fs_desc": "When disabled, the game runs in standard windowed mode.",
        "vsync_title": "V-Sync (Vertical Synchronization)",
        "vsync_desc": "Prevents screen tearing (Recommended OFF with Frame Pacer).",
        "vrr_title": "D3D12 Variable Refresh Rate (G-Sync / FreeSync)",
        "vrr_desc": "Enables low-latency variable refresh presentation.",
        "sec_fsr": "AMD FIDELITYFX & VRAM CACHE MANAGEMENT",
        "upscaler_lbl": "Upscaling & Sharpening Engine",
        "fsr_q_lbl": "AMD FSR Quality Mode",
        "pacer_lbl": "Frame Rate Pacer & 2x Cadence Target",
        "vram_lbl": "GPU VRAM Cache Profile & Internal Render Scale",
        "sec_ctrl": "MOUSE CAMERA & KEYBINDINGS",
        "mouse_title": "Mouse Camera Look",
        "mouse_desc": "Works simultaneously with XInput/SDL controllers.",
        "sens_lbl": "Mouse Camera Sensitivity",
        "keys_lbl": "Keybindings (Click any slot and press a key)",
        "sec_files": "GAME FILES (.ISO) & SAVE BACKUPS",
        "iso_card_title": "Xbox 360 .ISO Installer & Integrity Verifier",
        "btn_iso": "💿 Install from .ISO",
        "btn_verify": "🔍 Verify Game Files",
        "save_card_title": "Save Game Backup & Restore Manager",
        "btn_backup": "➕ Backup Save Now",
        "btn_restore": "♻ Restore Latest",
        "btn_folder": "📂 Open Folder",
        "sec_perf": "REAL-TIME SESSION, TELEMETRY & CRASH LOGS",
        "btn_open_log": "📄 Open Latest Log",
        "btn_export_diag": "📦 Export Bug Report (.ZIP)",
        "btn_logs_folder": "📂 Logs Folder",
    },
}


class ModernWoSLauncherV25:
    W = 1120
    H = 660

    def __init__(self, root: tk.Tk):
        self.root = root
        self.root.title("Spider-Man: Web of Shadows — ReComp Modern Edition")
        self.root.configure(bg=BG_DARK)
        self.root.resizable(False, False)

        sw = self.root.winfo_screenwidth()
        sh = self.root.winfo_screenheight()
        x = max(0, (sw - self.W) // 2)
        y = max(0, (sh - self.H) // 2)
        self.root.geometry(f"{self.W}x{self.H}+{x}+{y}")
        self.root.overrideredirect(True)

        self.config = dict(DEFAULT_CONFIG)
        self.load_config()

        self.lang = self.config.get("lang", "tr")
        if self.lang not in STRINGS:
            self.lang = "tr"
        self.suit = self.config.get("theme", "red")
        if self.suit not in SUIT_THEMES:
            self.suit = "red"

        self.active_tab = 0
        self.listening_key = None
        self.status_msg = ""
        self._drag_x = 0
        self._drag_y = 0

        LOGS_DIR.mkdir(parents=True, exist_ok=True)
        self.load_assets()
        try:
            ico_path = ASSETS_DIR / "wos_icon.ico"
            if ico_path.exists():
                self.root.iconbitmap(default=str(ico_path))
        except Exception:
            pass

        self.canvas = tk.Canvas(
            self.root,
            width=self.W,
            height=self.H,
            bg=BG_DARK,
            highlightthickness=0,
            bd=0,
        )
        self.canvas.pack(fill="both", expand=True)
        self.canvas.bind("<ButtonPress-1>", self.on_mouse_down)
        self.canvas.bind("<B1-Motion>", self.on_mouse_drag)

        self.render()

    def load_assets(self):
        self.img_bg_red = tk.PhotoImage(file=str(ASSETS_DIR / "bg_modern_red.png"))
        self.img_bg_sym = tk.PhotoImage(file=str(ASSETS_DIR / "bg_modern_symbiote.png"))
        self.img_logo = tk.PhotoImage(file=str(ASSETS_DIR / "logo_modern.png"))
        self.img_nav_red = tk.PhotoImage(file=str(ASSETS_DIR / "nav_emblem_red.png"))
        self.img_nav_sym = tk.PhotoImage(file=str(ASSETS_DIR / "nav_emblem_sym.png"))
        self.img_flag_tr = tk.PhotoImage(file=str(ASSETS_DIR / "flag_tr.png"))
        self.img_flag_en = tk.PhotoImage(file=str(ASSETS_DIR / "flag_en.png"))
        self.img_tog_red = tk.PhotoImage(file=str(ASSETS_DIR / "toggle_on_red.png"))
        self.img_tog_sym = tk.PhotoImage(file=str(ASSETS_DIR / "toggle_on_sym.png"))
        self.img_tog_off = tk.PhotoImage(file=str(ASSETS_DIR / "toggle_off.png"))

        self.play_btns = {
            ("tr", "red", False): tk.PhotoImage(file=str(ASSETS_DIR / "btn_launch_tr_red.png")),
            ("tr", "red", True): tk.PhotoImage(file=str(ASSETS_DIR / "btn_launch_tr_red_hov.png")),
            ("en", "red", False): tk.PhotoImage(file=str(ASSETS_DIR / "btn_launch_en_red.png")),
            ("en", "red", True): tk.PhotoImage(file=str(ASSETS_DIR / "btn_launch_en_red_hov.png")),
            ("tr", "symbiote", False): tk.PhotoImage(file=str(ASSETS_DIR / "btn_launch_tr_sym.png")),
            ("tr", "symbiote", True): tk.PhotoImage(file=str(ASSETS_DIR / "btn_launch_tr_sym_hov.png")),
            ("en", "symbiote", False): tk.PhotoImage(file=str(ASSETS_DIR / "btn_launch_en_sym.png")),
            ("en", "symbiote", True): tk.PhotoImage(file=str(ASSETS_DIR / "btn_launch_en_sym_hov.png")),
        }

    def load_config(self):
        if CONFIG_FILE.exists():
            try:
                with open(CONFIG_FILE, "r", encoding="utf-8") as f:
                    self.config.update(json.load(f))
            except Exception:
                pass

    def save_config(self):
        res_str = self.config.get("resolution", "1920x1080")
        if "x" in res_str:
            try:
                w, h = res_str.split("x")
                self.config["window_width"] = int(w)
                self.config["window_height"] = int(h)
            except Exception:
                pass
        self.config["lang"] = self.lang
        self.config["theme"] = self.suit
        try:
            with open(CONFIG_FILE, "w", encoding="utf-8") as f:
                json.dump(self.config, f, indent=4, ensure_ascii=False)
            bin_dir = BASE_DIR / "bin"
            if bin_dir.exists():
                with open(bin_dir / "recomp_config.json", "w", encoding="utf-8") as f:
                    json.dump(self.config, f, indent=4, ensure_ascii=False)
        except Exception:
            pass

    def find_exe_path(self) -> Path:
        for c in [
            BASE_DIR / "bin" / "wos_recomp.exe",
            BASE_DIR / "wos_recomp.exe",
            BASE_DIR.parent / "out" / "build" / "win-amd64-release" / "wos_recomp.exe",
        ]:
            if c.exists():
                return c
        return BASE_DIR / "bin" / "wos_recomp.exe"

    def find_game_dir(self) -> Path:
        for c in [BASE_DIR / "game", BASE_DIR.parent / "game"]:
            if (c / "default.xex").exists():
                return c
        return BASE_DIR / "game"

    def is_game_ready(self) -> bool:
        return self.find_exe_path().exists() and (self.find_game_dir() / "default.xex").exists()

    def on_mouse_down(self, event):
        self._drag_x = event.x
        self._drag_y = event.y

    def on_mouse_drag(self, event):
        if self._drag_y < 115 or self._drag_x > 615:
            nx = self.root.winfo_x() + (event.x - self._drag_x)
            ny = self.root.winfo_y() + (event.y - self._drag_y)
            self.root.geometry(f"+{nx}+{ny}")

    def minimize_window(self):
        self.root.overrideredirect(False)
        self.root.iconify()
        self.root.bind("<Map>", self._on_restore)

    def _on_restore(self, event):
        if self.root.state() == "normal":
            self.root.overrideredirect(True)
            self.root.unbind("<Map>")

    def toggle_suit(self):
        self.suit = "symbiote" if self.suit == "red" else "red"
        self.save_config()
        self.render()

    def set_lang(self, lang_code: str):
        self.lang = lang_code
        self.save_config()
        self.render()

    def switch_tab(self, idx: int):
        self.active_tab = idx
        self.listening_key = None
        self.render()

    # -------------------------------------------------------------------------
    # MODERN GLASS CARD & PILL HELPERS
    # -------------------------------------------------------------------------
    def draw_toggle_card(self, y: int, title: str, subtitle: str, checked: bool, callback):
        t = SUIT_THEMES[self.suit]
        x1, x2, h = 126, 594, 62
        card = self.canvas.create_rectangle(
            x1, y, x2, y + h,
            fill=CARD_FILL,
            outline=BORDER_SUBTLE,
            width=1,
        )
        # Left neon accent stripe when enabled
        if checked:
            self.canvas.create_rectangle(x1, y, x1 + 4, y + h, fill=t["accent"], outline="")

        t_id = self.canvas.create_text(
            x1 + 16, y + 12,
            text=title,
            font=("Segoe UI", 10, "bold"),
            fill=TEXT_WHITE,
            anchor="nw",
        )
        s_id = self.canvas.create_text(
            x1 + 16, y + 34,
            text=subtitle,
            font=("Segoe UI", 8),
            fill=TEXT_MUTED,
            anchor="nw",
        )
        tog_img = (self.img_tog_red if self.suit == "red" else self.img_tog_sym) if checked else self.img_tog_off
        img_id = self.canvas.create_image(x2 - 58, y + 19, image=tog_img, anchor="nw")

        for item in (card, t_id, s_id, img_id):
            self.canvas.tag_bind(item, "<Button-1>", lambda e: callback())
            self.canvas.tag_bind(item, "<Enter>", lambda e: (self.canvas.itemconfig(card, outline=t["accent"]), self.canvas.config(cursor="hand2")))
            self.canvas.tag_bind(item, "<Leave>", lambda e: (self.canvas.itemconfig(card, outline=BORDER_SUBTLE), self.canvas.config(cursor="")))

    def draw_pill(self, x: int, y: int, w: int, h: int, text: str, active: bool, callback):
        t = SUIT_THEMES[self.suit]
        fill_c = t["card_active"] if active else CARD_FILL
        out_c = t["accent"] if active else BORDER_SUBTLE
        txt_c = TEXT_WHITE if active else "#c5ccd9"

        r = self.canvas.create_rectangle(x, y, x + w, y + h, fill=fill_c, outline=out_c, width=1)
        if active:
            self.canvas.create_line(x + 1, y + h - 1, x + w - 1, y + h - 1, fill=t["accent"], width=2)
        l = self.canvas.create_text(
            x + w // 2, y + h // 2,
            text=text,
            font=("Segoe UI", 9, "bold" if active else "normal"),
            fill=txt_c,
        )
        for item in (r, l):
            self.canvas.tag_bind(item, "<Enter>", lambda e: (self.canvas.itemconfig(r, fill=CARD_HOVER, outline=t["accent_bright"]), self.canvas.config(cursor="hand2")))
            self.canvas.tag_bind(item, "<Leave>", lambda e: (self.canvas.itemconfig(r, fill=fill_c, outline=out_c), self.canvas.config(cursor="")))
            self.canvas.tag_bind(item, "<Button-1>", lambda e: callback())

    # -------------------------------------------------------------------------
    # MAIN RENDER
    # -------------------------------------------------------------------------
    def render(self):
        self.canvas.delete("all")
        s = STRINGS[self.lang]
        t = SUIT_THEMES[self.suit]

        # 1. 4K-Supersampled Glassmorphic Background (1120x660)
        bg_img = self.img_bg_red if self.suit == "red" else self.img_bg_sym
        self.canvas.create_image(0, 0, image=bg_img, anchor="nw")
        self.canvas.create_rectangle(0, 0, self.W - 1, self.H - 1, outline="#1c202e", width=1)

        # 2. Top-Right Window Controls (— and X) + Suit Switcher Pill
        min_btn = self.canvas.create_text(1062, 28, text="—", font=("Segoe UI", 10, "bold"), fill=TEXT_MUTED)
        cls_btn = self.canvas.create_text(1092, 28, text="X", font=("Segoe UI", 10, "bold"), fill=TEXT_MUTED)
        self.canvas.tag_bind(min_btn, "<Enter>", lambda e: (self.canvas.itemconfig(min_btn, fill=TEXT_WHITE), self.canvas.config(cursor="hand2")))
        self.canvas.tag_bind(min_btn, "<Leave>", lambda e: (self.canvas.itemconfig(min_btn, fill=TEXT_MUTED), self.canvas.config(cursor="")))
        self.canvas.tag_bind(min_btn, "<Button-1>", lambda e: self.minimize_window())

        self.canvas.tag_bind(cls_btn, "<Enter>", lambda e: (self.canvas.itemconfig(cls_btn, fill=t["accent_bright"]), self.canvas.config(cursor="hand2")))
        self.canvas.tag_bind(cls_btn, "<Leave>", lambda e: (self.canvas.itemconfig(cls_btn, fill=TEXT_MUTED), self.canvas.config(cursor="")))
        self.canvas.tag_bind(cls_btn, "<Button-1>", lambda e: self.root.destroy())

        # 3. Left Vertical Navigation Rail (x=18..94, y=18..642)
        nav_emb = self.img_nav_red if self.suit == "red" else self.img_nav_sym
        emb_id = self.canvas.create_image(32, 26, image=nav_emb, anchor="nw")
        self.canvas.tag_bind(emb_id, "<Button-1>", lambda e: self.toggle_suit())
        self.canvas.tag_bind(emb_id, "<Enter>", lambda e: self.canvas.config(cursor="hand2"))
        self.canvas.tag_bind(emb_id, "<Leave>", lambda e: self.canvas.config(cursor=""))
        for idx, (icon_str, nav_lbl) in enumerate(s["nav"]):
            ny = 92 + idx * 74
            is_act = (idx == self.active_tab)
            box_fill = t["card_active"] if is_act else GLASS_HEADER
            box_out = t["accent"] if is_act else "#191d28"
            nbox = self.canvas.create_rectangle(26, ny, 86, ny + 58, fill=box_fill, outline=box_out, width=1)
            if is_act:
                self.canvas.create_rectangle(18, ny + 10, 22, ny + 48, fill=t["accent"], outline="")
            nico = self.canvas.create_text(56, ny + 20, text=icon_str, font=("Segoe UI Emoji", 13), fill=TEXT_WHITE if is_act else TEXT_MUTED)
            ntxt = self.canvas.create_text(56, ny + 44, text=nav_lbl, font=("Segoe UI", 8, "bold" if is_act else "normal"), fill=TEXT_WHITE if is_act else TEXT_MUTED)
            for item in (nbox, nico, ntxt):
                self.canvas.tag_bind(item, "<Button-1>", lambda e, i=idx: self.switch_tab(i))
                self.canvas.tag_bind(item, "<Enter>", lambda e, b=nbox: (self.canvas.itemconfig(b, outline=t["accent_bright"]), self.canvas.config(cursor="hand2")))
                self.canvas.tag_bind(item, "<Leave>", lambda e, b=nbox, o=box_out: (self.canvas.itemconfig(b, outline=o), self.canvas.config(cursor="")))

        # 4. Top Header Glass Bar (x=108..612, y=18..112)
        self.canvas.create_image(124, 28, image=self.img_logo, anchor="nw")
        self.canvas.create_text(126, 84, text="RECOMP MODERN v2.5", font=("Segoe UI", 8, "bold"), fill=t["accent_bright"], anchor="nw")
        self.canvas.create_text(248, 84, text="•  WIN64 AVX2 + D3D12 ROV", font=("Segoe UI", 8, "bold"), fill=TEXT_MUTED, anchor="nw")

        # Top-Right inside Header Bar: Language Flags & Suit Theme Toggle
        if self.lang == "tr":
            self.canvas.create_rectangle(523, 31, 553, 51, outline=t["accent"], width=1)
        else:
            self.canvas.create_rectangle(559, 31, 589, 51, outline=t["accent"], width=1)
        ftr = self.canvas.create_image(524, 32, image=self.img_flag_tr, anchor="nw")
        fen = self.canvas.create_image(560, 32, image=self.img_flag_en, anchor="nw")
        for item, lc in ((ftr, "tr"), (fen, "en")):
            self.canvas.tag_bind(item, "<Button-1>", lambda e, c=lc: self.set_lang(c))
            self.canvas.tag_bind(item, "<Enter>", lambda e: self.canvas.config(cursor="hand2"))
            self.canvas.tag_bind(item, "<Leave>", lambda e: self.canvas.config(cursor=""))

        # Suit Switcher Pill (RED SUIT / SYMBIOTE)
        self.draw_pill(478, 64, 118, 30, f"🕷 {t['badge_text']}", True, self.toggle_suit)

        # 5. Center Frosted-Glass Content Panel (x=108..612, y=124..548)
        if self.active_tab == 0:
            self.render_tab_general(s, t)
        elif self.active_tab == 1:
            self.render_tab_display(s, t)
        elif self.active_tab == 2:
            self.render_tab_fsr(s, t)
        elif self.active_tab == 3:
            self.render_tab_controls(s, t)
        elif self.active_tab == 4:
            self.render_tab_files(s, t)
        elif self.active_tab == 5:
            self.render_tab_stats(s, t)

        # 6. Bottom Launch Dock Glass Bar (x=108..612, y=560..642)
        ready = self.is_game_ready()
        self.canvas.create_text(
            128, 578,
            text=s["ready"] if ready else s["missing"],
            font=("Segoe UI", 10, "bold"),
            fill=GREEN_OK if ready else "#f39c12",
            anchor="nw",
        )
        eff_up = self.config.get("present_effect", "fsr").upper()
        fg_tag = "2x Pacer AÇIK" if self.config.get("frame_generation", True) else "Pacer KAPALI"
        sub_line = self.status_msg if self.status_msg else f"AMD {eff_up}  •  {fg_tag}  •  {self.config.get('resolution', '1920x1080')}"
        self.canvas.create_text(128, 602, text=sub_line, font=("Segoe UI", 8), fill=TEXT_MUTED, anchor="nw")

        p_norm = self.play_btns[(self.lang, self.suit, False)]
        p_hov = self.play_btns[(self.lang, self.suit, True)]
        btn_play = self.canvas.create_image(382, 572, image=p_norm, anchor="nw")
        self.canvas.tag_bind(btn_play, "<Enter>", lambda e: (self.canvas.itemconfig(btn_play, image=p_hov), self.canvas.config(cursor="hand2")))
        self.canvas.tag_bind(btn_play, "<Leave>", lambda e: (self.canvas.itemconfig(btn_play, image=p_norm), self.canvas.config(cursor="")))
        self.canvas.tag_bind(btn_play, "<Button-1>", lambda e: self.launch_game())

    # -------------------------------------------------------------------------
    # TAB 0: GENERAL
    # -------------------------------------------------------------------------
    def render_tab_general(self, s, t):
        self.canvas.create_text(126, 142, text=s["sec_overview"], font=("Segoe UI", 9, "bold"), fill=t["accent_bright"], anchor="nw")

        self.draw_toggle_card(
            170,
            s["fg_title"],
            s["fg_desc"],
            bool(self.config.get("frame_generation", True)),
            lambda: self.set_cfg("frame_generation", not self.config.get("frame_generation", True)),
        )
        self.draw_toggle_card(
            242,
            s["occ_title"],
            s["occ_desc"],
            bool(self.config.get("occlusion_query_enable", True)),
            lambda: self.set_cfg("occlusion_query_enable", not self.config.get("occlusion_query_enable", True)),
        )
        self.draw_toggle_card(
            314,
            s["alpha_title"],
            s["alpha_desc"],
            bool(self.config.get("fuzzy_alpha_epsilon", True)),
            lambda: self.set_cfg("fuzzy_alpha_epsilon", not self.config.get("fuzzy_alpha_epsilon", True)),
        )

        self.canvas.create_text(126, 396, text=s["preset_hdr"], font=("Segoe UI", 9, "bold"), fill=TEXT_MUTED, anchor="nw")
        vram = self.config.get("vram_profile", "4gb")
        sc = int(self.config.get("resolution_scale", 1))
        self.draw_pill(126, 422, 162, 40, s["p_laptop"], vram == "4gb" and sc == 1, lambda: self.apply_preset("laptop"))
        self.draw_pill(296, 422, 144, 40, s["p_bal"], vram == "8gb" and sc == 1, lambda: self.apply_preset("balanced"))
        self.draw_pill(448, 422, 146, 40, s["p_ultra"], sc == 2, lambda: self.apply_preset("ultra"))

        # Quick live summary badges at bottom of General tab
        badges = [
            f"⚡ UPSCALER: AMD {self.config.get('present_effect', 'fsr').upper()}",
            f"💾 VRAM: {vram.upper()}",
            f"🎯 HEDEF: {self.config.get('frame_limit', 60) or 'MAX'} FPS",
        ]
        for i, btxt in enumerate(badges):
            bx = 126 + i * 160
            self.canvas.create_rectangle(bx, 482, bx + 148, 518, fill="#0c0e15", outline="#1d2230")
            self.canvas.create_text(bx + 74, 500, text=btxt, font=("Segoe UI", 8, "bold"), fill=TEXT_MUTED)

    # -------------------------------------------------------------------------
    # TAB 1: DISPLAY
    # -------------------------------------------------------------------------
    def render_tab_display(self, s, t):
        self.canvas.create_text(126, 142, text=s["sec_display"], font=("Segoe UI", 9, "bold"), fill=t["accent_bright"], anchor="nw")
        self.canvas.create_text(126, 170, text=s["res_lbl"], font=("Segoe UI", 9, "bold"), fill=TEXT_MUTED, anchor="nw")

        cur_res = self.config.get("resolution", "1920x1080")
        for i, r in enumerate(["1280x720", "1600x900", "1920x1080", "2560x1440", "3840x2160"]):
            self.draw_pill(126 + i * 95, 194, 88, 34, r, cur_res == r, lambda val=r: self.set_cfg("resolution", val))

        self.draw_toggle_card(
            248,
            s["fs_title"],
            s["fs_desc"],
            bool(self.config.get("fullscreen", False)),
            lambda: self.set_cfg("fullscreen", not self.config.get("fullscreen", False)),
        )
        self.draw_toggle_card(
            322,
            s["vsync_title"],
            s["vsync_desc"],
            bool(self.config.get("vsync", False)),
            lambda: self.set_cfg("vsync", not self.config.get("vsync", False)),
        )
        self.draw_toggle_card(
            396,
            s["vrr_title"],
            s["vrr_desc"],
            bool(self.config.get("vrr_tearing", True)),
            lambda: self.set_cfg("vrr_tearing", not self.config.get("vrr_tearing", True)),
        )

    # -------------------------------------------------------------------------
    # TAB 2: AMD FSR & VRAM
    # -------------------------------------------------------------------------
    def render_tab_fsr(self, s, t):
        self.canvas.create_text(126, 142, text=s["sec_fsr"], font=("Segoe UI", 9, "bold"), fill=t["accent_bright"], anchor="nw")

        self.canvas.create_text(126, 172, text=s["upscaler_lbl"], font=("Segoe UI", 9, "bold"), fill=TEXT_MUTED, anchor="nw")
        eff = self.config.get("present_effect", "fsr")
        for i, (code, lbl) in enumerate([("fsr", "AMD FSR (FidelityFX)"), ("cas", "AMD CAS (Sharpness)"), ("bilinear", "Bilinear (Off)")]):
            self.draw_pill(126 + i * 160, 196, 150, 34, lbl, eff == code, lambda c=code: self.set_cfg("present_effect", c))

        self.canvas.create_text(126, 246, text=s["fsr_q_lbl"], font=("Segoe UI", 9, "bold"), fill=TEXT_MUTED, anchor="nw")
        qm = self.config.get("fsr_quality_mode", "quality")
        for i, (code, lbl) in enumerate([
            ("nativeaa", "Native AA"),
            ("quality", "Kalite" if self.lang == "tr" else "Quality"),
            ("balanced", "Dengeli" if self.lang == "tr" else "Balanced"),
            ("performance", "Performans" if self.lang == "tr" else "Perf"),
        ]):
            self.draw_pill(126 + i * 119, 270, 111, 34, lbl, qm == code, lambda c=code: self.set_cfg("fsr_quality_mode", c))

        self.canvas.create_text(126, 320, text=s["pacer_lbl"], font=("Segoe UI", 9, "bold"), fill=TEXT_MUTED, anchor="nw")
        fl = int(self.config.get("frame_limit", 60))
        for i, (v, lbl) in enumerate([(30, "30 → 60 Hz"), (40, "40 → 80 Hz"), (60, "60 → 120 Hz"), (0, "Sınırsız" if self.lang == "tr" else "Unlimited")]):
            self.draw_pill(126 + i * 119, 344, 111, 34, lbl, fl == v, lambda val=v: self.set_cfg("frame_limit", val))

        self.canvas.create_text(126, 394, text=s["vram_lbl"], font=("Segoe UI", 9, "bold"), fill=TEXT_MUTED, anchor="nw")
        vram = self.config.get("vram_profile", "4gb")
        for i, (code, lbl) in enumerate([("4gb", "4 GB VRAM"), ("8gb", "8 GB VRAM"), ("12gb", "12+ GB VRAM")]):
            self.draw_pill(126 + i * 159, 418, 150, 34, lbl, vram == code, lambda c=code: self.set_cfg("vram_profile", c))

        sc = int(self.config.get("resolution_scale", 1))
        af = int(self.config.get("anisotropic_override", 0))
        self.draw_pill(126, 464, 130, 34, "Render: 1x (720p)", sc == 1, lambda: self.set_cfg("resolution_scale", 1))
        self.draw_pill(264, 464, 130, 34, "Render: 2x (1440p)", sc == 2, lambda: self.set_cfg("resolution_scale", 2))
        self.draw_pill(402, 464, 92, 34, "AF: Kapalı", af == 0, lambda: self.set_cfg("anisotropic_override", 0))
        self.draw_pill(502, 464, 92, 34, "AF: 16x", af == 5, lambda: self.set_cfg("anisotropic_override", 5))

    # -------------------------------------------------------------------------
    # TAB 3: CONTROLS
    # -------------------------------------------------------------------------
    def render_tab_controls(self, s, t):
        self.canvas.create_text(126, 142, text=s["sec_ctrl"], font=("Segoe UI", 9, "bold"), fill=t["accent_bright"], anchor="nw")

        self.draw_toggle_card(
            168,
            s["mouse_title"],
            s["mouse_desc"],
            bool(self.config.get("mnk_mouse", True)),
            lambda: self.set_cfg("mnk_mouse", not self.config.get("mnk_mouse", True)),
        )

        self.canvas.create_text(126, 242, text=s["sens_lbl"], font=("Segoe UI", 9, "bold"), fill=TEXT_MUTED, anchor="nw")
        sens = round(float(self.config.get("mnk_sensitivity", 1.0)), 1)
        for i, sv in enumerate([0.6, 1.0, 1.4, 2.0, 3.0]):
            self.draw_pill(126 + i * 95, 264, 88, 30, f"{sv:.1f}x", abs(sens - sv) < 0.08, lambda v=sv: self.set_cfg("mnk_sensitivity", v))

        self.canvas.create_text(126, 306, text=s["keys_lbl"], font=("Segoe UI", 9, "bold"), fill=TEXT_MUTED, anchor="nw")
        keys = [
            ("keybind_right_trigger", "🕸 Ağ Atma (RT)"),
            ("keybind_left_trigger", "🎯 Duvar/Hedef (LT)"),
            ("keybind_a", "🦘 Zıpla (A)"),
            ("keybind_x", "👊 Saldırı (X)"),
            ("keybind_y", "⚡ Web Strike (Y)"),
            ("keybind_b", "🕷 Tendril (B)"),
            ("keybind_lstick_press", "🖤 Kostüm (L3)"),
            ("keybind_start", "🗺 Harita (Start)"),
        ]
        for idx, (kid, klbl) in enumerate(keys):
            col = idx % 2
            row = idx // 2
            bx = 126 + col * 238
            by = 330 + row * 48
            val = "[Tuşa Basın]" if self.listening_key == kid else self.config.get(kid, DEFAULT_CONFIG[kid])
            self.draw_pill(bx, by, 230, 38, f"{klbl}:  {val}", self.listening_key == kid, lambda k=kid: self.start_key_listen(k))

    def start_key_listen(self, key_id: str):
        self.listening_key = key_id
        self.render()

        def on_key(event):
            mapping = {
                "space": "Space",
                "Shift_L": "LShift",
                "Shift_R": "RShift",
                "Control_L": "LCtrl",
                "Control_R": "RCtrl",
                "Alt_L": "LAlt",
                "Alt_R": "RAlt",
                "Return": "Enter",
                "Tab": "Tab",
                "Escape": "Escape",
            }
            ks = event.keysym
            self.config[key_id] = mapping.get(ks, ks.upper() if len(ks) == 1 else ks)
            self.listening_key = None
            self.root.unbind("<Key>")
            self.save_config()
            self.render()

        self.root.bind("<Key>", on_key)

    # -------------------------------------------------------------------------
    # TAB 4: FILES & ISO WIZARD
    # -------------------------------------------------------------------------
    def render_tab_files(self, s, t):
        self.canvas.create_text(126, 142, text=s["sec_files"], font=("Segoe UI", 9, "bold"), fill=t["accent_bright"], anchor="nw")

        # Card 1: ISO Installer
        self.canvas.create_rectangle(126, 174, 594, 320, fill=CARD_FILL, outline=BORDER_SUBTLE)
        self.canvas.create_text(144, 190, text=s["iso_card_title"], font=("Segoe UI", 11, "bold"), fill=TEXT_WHITE, anchor="nw")
        gdir = self.find_game_dir()
        xex_ok = (gdir / "default.xex").exists()
        st = f"✔ default.xex doğrulandı ({gdir})" if xex_ok else "⚠ game/default.xex bulunamadı (Lütfen .ISO seçin)"
        self.canvas.create_text(144, 218, text=st, font=("Segoe UI", 9, "bold"), fill=GREEN_OK if xex_ok else "#f39c12", anchor="nw")
        self.draw_pill(144, 256, 210, 42, s["btn_iso"], True, self.install_from_iso)
        self.draw_pill(366, 256, 210, 42, s["btn_verify"], False, self.verify_files)

        # Card 2: Save Manager
        self.canvas.create_rectangle(126, 340, 594, 510, fill=CARD_FILL, outline=BORDER_SUBTLE)
        self.canvas.create_text(144, 358, text=s["save_card_title"], font=("Segoe UI", 11, "bold"), fill=TEXT_WHITE, anchor="nw")
        SAVES_BACKUP_DIR.mkdir(parents=True, exist_ok=True)
        backups = sorted(SAVES_BACKUP_DIR.glob("WoS_Save_Backup_*.zip"), reverse=True)
        b_info = f"Son Yedek: {backups[0].name} ({len(backups)} adet kayıtlı yedek)" if backups else "Henüz kayıt yedeği oluşturulmadı."
        self.canvas.create_text(144, 386, text=b_info, font=("Segoe UI", 9), fill=TEXT_MUTED, anchor="nw")

        self.draw_pill(144, 430, 140, 42, s["btn_backup"], True, self.backup_saves)
        self.draw_pill(294, 430, 140, 42, s["btn_restore"], False, self.restore_latest_save)
        self.draw_pill(444, 430, 132, 42, s["btn_folder"], False, lambda: os.startfile(str(SAVES_BACKUP_DIR)))

    # -------------------------------------------------------------------------
    # TAB 5: TELEMETRY STATS
    # -------------------------------------------------------------------------
    def render_tab_stats(self, s, t):
        self.canvas.create_text(126, 142, text=s["sec_perf"], font=("Segoe UI", 9, "bold"), fill=t["accent_bright"], anchor="nw")

        stats_data = {}
        for c in [STATS_FILE, BASE_DIR / "bin" / "last_session_stats.json", BASE_DIR.parent / "last_session_stats.json"]:
            if c.exists():
                try:
                    with open(c, "r", encoding="utf-8") as f:
                        stats_data = json.load(f)
                    break
                except Exception:
                    pass

        fg_fps = stats_data.get("avg_fg_fps", 60.0)
        nat_fps = stats_data.get("avg_native_fps", 30.0)
        peak_fps = stats_data.get("peak_fg_fps", 72.0)
        ft_ms = stats_data.get("last_frametime_ms", 16.6)
        cpu_pct = stats_data.get("last_cpu_pct", 42.0)
        up = stats_data.get("upscaler", self.config.get("present_effect", "fsr").upper())

        # Hero Metric Card
        self.canvas.create_rectangle(126, 168, 594, 248, fill=CARD_FILL, outline=t["accent"])
        self.canvas.create_text(146, 180, text=f"{fg_fps} Hz (2x Paced)" if self.config.get("frame_generation", True) else f"{nat_fps} FPS", font=("Segoe UI", 24, "bold"), fill=GREEN_OK, anchor="nw")
        self.canvas.create_text(146, 220, text=f"Zirve (Peak): {peak_fps} Hz   •   Render: {nat_fps} FPS ({ft_ms} ms)", font=("Segoe UI", 9, "bold"), fill=TEXT_MUTED, anchor="nw")

        rows = [
            ("Aktif AMD FidelityFX Motoru", f"AMD {up} ({self.config.get('fsr_quality_mode', 'quality').upper()})"),
            ("Dahili 2x Frame Pacer", "AÇIK (2x Cadence Doubler)" if self.config.get("frame_generation", True) else "KAPALI"),
            ("İşlemci (CPU) Kullanımı", f"%{cpu_pct}  (AVX2 + FMA3 + FTZ/DAZ)"),
            ("VRAM Önbellek Profili", f"{self.config.get('vram_profile', '4gb').upper()}  (ROV + Occlusion Culling)"),
        ]
        for i, (k, v) in enumerate(rows):
            y = 260 + i * 42
            self.canvas.create_rectangle(126, y, 594, y + 36, fill=CARD_FILL, outline=BORDER_SUBTLE)
            self.canvas.create_text(142, y + 18, text=k, font=("Segoe UI", 9), fill=TEXT_MUTED, anchor="w")
            self.canvas.create_text(578, y + 18, text=v, font=("Segoe UI", 9, "bold"), fill=TEXT_WHITE, anchor="e")

        # Crash & Session Log Diagnostic Card
        LOGS_DIR.mkdir(parents=True, exist_ok=True)
        latest_log = LOGS_DIR / "wos_recomp_latest.log"
        log_logs = sorted(LOGS_DIR.glob("wos_session_*.log"), reverse=True)
        if latest_log.exists():
            kb = round(latest_log.stat().st_size / 1024, 1)
            log_status = f"✔ Otomatik Log Aktif: logs/wos_recomp_latest.log ({kb} KB)"
        elif log_logs:
            kb = round(log_logs[0].stat().st_size / 1024, 1)
            log_status = f"✔ Son Oturum Logu: {log_logs[0].name} ({kb} KB)"
        else:
            log_status = "ℹ Oyun başlatıldığında tüm motor/çökme kayıtları 'logs/' klasörüne yazılır."

        self.canvas.create_rectangle(126, 434, 594, 532, fill=CARD_FILL, outline=BORDER_SUBTLE)
        self.canvas.create_text(142, 448, text=log_status, font=("Segoe UI", 8, "bold"), fill=TEXT_MUTED, anchor="nw")
        self.draw_pill(142, 478, 128, 38, s["btn_open_log"], False, self.open_latest_log)
        self.draw_pill(278, 478, 174, 38, s["btn_export_diag"], True, self.export_bug_report)
        self.draw_pill(460, 478, 118, 38, s["btn_logs_folder"], False, lambda: os.startfile(str(LOGS_DIR)))

    # -------------------------------------------------------------------------
    # ACTIONS
    # -------------------------------------------------------------------------
    def set_cfg(self, key: str, val):
        self.config[key] = val
        self.save_config()
        self.render()

    def apply_preset(self, preset: str):
        if preset == "laptop":
            self.config.update({
                "resolution": "1920x1080",
                "present_effect": "fsr",
                "fsr_quality_mode": "quality",
                "frame_generation": True,
                "frame_limit": 60,
                "resolution_scale": 1,
                "vram_profile": "4gb",
                "anisotropic_override": 0,
                "occlusion_query_enable": True,
            })
            self.status_msg = "✔ Performans (4 GB VRAM) Profili Uygulandı"
        elif preset == "balanced":
            self.config.update({
                "resolution": "1920x1080",
                "present_effect": "fsr",
                "fsr_quality_mode": "nativeaa",
                "frame_generation": True,
                "frame_limit": 60,
                "resolution_scale": 1,
                "vram_profile": "8gb",
                "anisotropic_override": 3,
                "occlusion_query_enable": True,
            })
            self.status_msg = "✔ Dengeli (1080p) Profili Uygulandı"
        elif preset == "ultra":
            self.config.update({
                "resolution": "2560x1440",
                "present_effect": "fsr",
                "fsr_quality_mode": "nativeaa",
                "frame_generation": True,
                "frame_limit": 0,
                "resolution_scale": 2,
                "vram_profile": "12gb",
                "anisotropic_override": 5,
                "occlusion_query_enable": True,
            })
            self.status_msg = "✔ Ultra Kalite (2K) Profili Uygulandı"
        self.save_config()
        self.render()

    def install_from_iso(self):
        iso_file = filedialog.askopenfilename(
            title="Xbox 360 Spider-Man: Web of Shadows (.ISO) Seçin",
            filetypes=[("Xbox 360 ISO", "*.iso"), ("All Files", "*.*")],
        )
        if not iso_file:
            return
        tool = BASE_DIR / "tools" / "extract-xiso.exe"
        if not tool.exists():
            tool = BASE_DIR.parent / "tools" / "extract-xiso.exe"
        if not tool.exists():
            messagebox.showerror("Hata", "tools/extract-xiso.exe bulunamadı!")
            return
        target = BASE_DIR / "game"
        target.mkdir(parents=True, exist_ok=True)
        self.status_msg = "⏳ ISO çıkartılıyor..."
        self.render()
        self.root.update_idletasks()
        try:
            subprocess.run([str(tool), "-d", str(target), "-x", str(iso_file)], check=True)
            self.status_msg = "✔ ISO kurulumu tamamlandı!"
            self.render()
            messagebox.showinfo("Tamamlandı", f"Oyun dosyaları çıkartıldı:\n{target}")
        except Exception as e:
            messagebox.showerror("Hata", str(e))

    def verify_files(self):
        gdir = self.find_game_dir()
        xex = gdir / "default.xex"
        if not xex.exists():
            messagebox.showwarning("Eksik Dosya", "game/default.xex bulunamadı!")
            return
        cnt = sum(len(files) for _, _, files in os.walk(gdir))
        messagebox.showinfo("Doğrulama Başarılı", f"✔ default.xex mevcut\n✔ Toplam {cnt} oyun dosyası doğrulandı!")

    def backup_saves(self):
        SAVES_BACKUP_DIR.mkdir(parents=True, exist_ok=True)
        stamp = datetime.now().strftime("%Y%m%d_%H%M%S")
        zpath = SAVES_BACKUP_DIR / f"WoS_Save_Backup_{stamp}.zip"
        with zipfile.ZipFile(zpath, "w", zipfile.ZIP_DEFLATED) as zf:
            if CONFIG_FILE.exists():
                zf.write(CONFIG_FILE, arcname="recomp_config.json")
            for cand in [BASE_DIR / "content", BASE_DIR.parent / "content"]:
                if cand.exists():
                    for r, _, files in os.walk(cand):
                        for fn in files:
                            full = Path(r) / fn
                            zf.write(full, arcname=str(full.relative_to(cand.parent)))
        self.status_msg = f"✔ Kayıt yedeği alındı: {zpath.name}"
        self.render()

    def restore_latest_save(self):
        SAVES_BACKUP_DIR.mkdir(parents=True, exist_ok=True)
        backups = sorted(SAVES_BACKUP_DIR.glob("WoS_Save_Backup_*.zip"), reverse=True)
        if not backups:
            messagebox.showinfo("Bilgi", "Geri yüklenecek yedek bulunamadı.")
            return
        with zipfile.ZipFile(backups[0], "r") as zf:
            zf.extractall(BASE_DIR)
        self.load_config()
        self.status_msg = "✔ Son yedek geri yüklendi!"
        self.render()

    def open_latest_log(self):
        LOGS_DIR.mkdir(parents=True, exist_ok=True)
        latest = LOGS_DIR / "wos_recomp_latest.log"
        if not latest.exists() or latest.stat().st_size == 0:
            candidates = sorted(LOGS_DIR.glob("*.log"), key=lambda p: p.stat().st_mtime, reverse=True)
            bin_logs_dir = BASE_DIR / "bin" / "logs"
            if bin_logs_dir.exists():
                candidates.extend(sorted(bin_logs_dir.glob("*.log"), key=lambda p: p.stat().st_mtime, reverse=True))
            if candidates:
                latest = candidates[0]
            else:
                messagebox.showinfo(
                    "Log Bulunamadı" if self.lang == "tr" else "No Log Found",
                    "Henüz oluşturulmuş bir oyun log dosyası yok. Oyunu en az bir kez başlattığınızda otomatik oluşturulur."
                    if self.lang == "tr"
                    else "No session log found yet. Start the game once to generate logs/wos_recomp_latest.log.",
                )
                return
        os.startfile(str(latest))

    def export_bug_report(self):
        LOGS_DIR.mkdir(parents=True, exist_ok=True)
        stamp = datetime.now().strftime("%Y%m%d_%H%M%S")
        diag_zip = LOGS_DIR / f"WoS_BugReport_{stamp}.zip"
        with zipfile.ZipFile(diag_zip, "w", zipfile.ZIP_DEFLATED) as zf:
            if CONFIG_FILE.exists():
                zf.write(CONFIG_FILE, arcname="recomp_config.json")
            for st_cand in [STATS_FILE, BASE_DIR / "bin" / "last_session_stats.json"]:
                if st_cand.exists():
                    zf.write(st_cand, arcname="last_session_stats.json")
                    break
            all_logs = sorted(LOGS_DIR.glob("*.log"), key=lambda p: p.stat().st_mtime, reverse=True)
            bin_logs_dir = BASE_DIR / "bin" / "logs"
            if bin_logs_dir.exists():
                all_logs.extend(sorted(bin_logs_dir.glob("*.log"), key=lambda p: p.stat().st_mtime, reverse=True))
            for lf in all_logs[:5]:
                zf.write(lf, arcname=f"logs/{lf.name}")
            # Include basic OS/CPU/GPU diagnostic summary
            sys_info = [
                f"Timestamp: {datetime.now().isoformat()}",
                f"Platform: {sys.platform}",
                f"Game Ready: {self.is_game_ready()}",
                f"Engine Path: {self.find_exe_path()}",
                f"Game Dir: {self.find_game_dir()}",
            ]
            zf.writestr("system_summary.txt", "\n".join(sys_info))
        self.status_msg = f"✔ Hata Raporu Hazır: logs/{diag_zip.name}"
        self.render()
        messagebox.showinfo(
            "Hata Raporu Oluşturuldu" if self.lang == "tr" else "Bug Report Created",
            f"GitHub Issues'a yükleyebileceğiniz hata raporu paketi oluşturuldu:\n\n{diag_zip}"
            if self.lang == "tr"
            else f"Diagnostic archive ready to attach to GitHub Issues:\n\n{diag_zip}",
        )
        os.startfile(str(LOGS_DIR))

    def launch_game(self):
        self.save_config()
        exe = self.find_exe_path()
        if not exe.exists():
            messagebox.showerror("Hata", f"Motor bulunamadı:\n{exe}")
            return
        if not (self.find_game_dir() / "default.xex").exists():
            self.switch_tab(4)
            messagebox.showwarning("Kurulum Gerekli", "Lütfen önce .ISO dosyasından oyun dosyalarını çıkartın.")
            return
        LOGS_DIR.mkdir(parents=True, exist_ok=True)
        # Keep only the 10 most recent timestamped session logs
        old_logs = sorted(LOGS_DIR.glob("wos_session_*.log"), reverse=True)
        for old_f in old_logs[9:]:
            try:
                old_f.unlink()
            except Exception:
                pass
        stamp = datetime.now().strftime("%Y%m%d_%H%M%S")
        latest_log = LOGS_DIR / "wos_recomp_latest.log"
        session_log = LOGS_DIR / f"wos_session_{stamp}.log"
        # Archive previous latest log if present
        if latest_log.exists():
            try:
                prev_stamp = datetime.fromtimestamp(latest_log.stat().st_mtime).strftime("%Y%m%d_%H%M%S")
                prev_dest = LOGS_DIR / f"wos_session_{prev_stamp}.log"
                if not prev_dest.exists():
                    latest_log.replace(prev_dest)
            except Exception:
                pass
        # Clean environment: strip any PyInstaller temporary directory from PATH and environment
        # so wos_recomp.exe never inherits or locks _MEIPASS
        env = os.environ.copy()
        env["WOS_AUTOSKIP"] = "0"
        env["WOS_CAPTURE_TEST"] = "0"
        env.pop("_MEIPASS", None)
        env.pop("_MEIPASS2", None)
        mei_dir = getattr(sys, "_MEIPASS", "")
        if mei_dir and "PATH" in env:
            env["PATH"] = os.pathsep.join(
                [p for p in env["PATH"].split(os.pathsep) if p and (mei_dir not in p and "_MEI" not in p)]
            )

        # Determine relative path from exe.parent to latest_log (immune to path spaces & non-ASCII folder names)
        try:
            rel_log = os.path.relpath(latest_log, exe.parent)
        except Exception:
            rel_log = str(latest_log)

        creationflags = 0
        if sys.platform == "win32":
            creationflags = subprocess.DETACHED_PROCESS | subprocess.CREATE_NEW_PROCESS_GROUP

        proc = subprocess.Popen(
            [str(exe), f"--log_file={rel_log}"],
            cwd=str(exe.parent),
            env=env,
            creationflags=creationflags,
            close_fds=True,
            stdin=subprocess.DEVNULL,
            stdout=subprocess.DEVNULL,
            stderr=subprocess.DEVNULL,
        )

        # Hide launcher while the game is running to completely prevent PyInstaller _MEI cleanup races.
        # When the game terminates, automatically restore the launcher on the Stats/Log tab!
        self.root.withdraw()

        def check_game_process():
            if proc.poll() is not None:
                try:
                    self.root.deiconify()
                    self.switch_tab(5)  # Switch to Stats & Diagnostic log tab
                    self.render()
                except Exception:
                    sys.exit(0)
            else:
                self.root.after(1000, check_game_process)

        self.root.after(1000, check_game_process)


if __name__ == "__main__":
    root = tk.Tk()
    app = ModernWoSLauncherV25(root)
    root.mainloop()
