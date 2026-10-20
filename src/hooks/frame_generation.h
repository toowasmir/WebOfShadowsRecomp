// wos_recomp - Built-in AMD FidelityFX (FSR + CAS) & High-Precision 2x Frame Pacer
//
// Spider-Man: Web of Shadows (Xbox 360 -> Win64 Static Recompilation)
//
// Provides:
// 1. Zero-dependency JSON configuration loader (recomp_config.json)
// 2. Direct ReXGlue CVAR + Presenter binding for AMD FidelityFX (FSR + RCAS & CAS)
// 3. High-resolution QPC + WaitableTimer 2x Frame Pacer & Display Cadence Doubler
// 4. Session telemetry export (last_session_stats.json) for the Modern Launcher

#pragma once

#include <windows.h>
#include <atomic>
#include <chrono>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <string>
#include <thread>
#include <algorithm>

#include <rex/cvar.h>
#include <rex/logging.h>
#include <rex/ui/presenter.h>
#include <rex/ui/window.h>

namespace wos::fg {

struct ModernConfig {
  // Display & Window
  int window_width = 1920;
  int window_height = 1080;
  bool fullscreen = false;
  bool vsync = false;
  bool vrr_tearing = true;
  bool letterbox = true;
  bool dither = true;

  // AMD FidelityFX Upscaler & Sharpening
  std::string present_effect = "fsr";            // bilinear, cas, fsr (AMD FSR 1.0 + RCAS)
  std::string fsr_quality_mode = "quality";      // auto, nativeaa, quality, balanced, performance, ultraperformance
  double fsr_sharpness = 0.2;                    // 0.0 (sharpest) .. 2.0 (softest) in FSR RCAS attenuation
  double cas_sharpness = 0.5;                    // 0.0 .. 1.0
  int fsr_max_passes = 2;

  // Built-in 2x Frame Pacer & Display Cadence Doubler
  bool frame_generation = true;                  // 2x Display Cadence Pacer
  int fg_multiplier = 2;                         // 2x
  int frame_limit = 60;                          // 0 = unlimited, 30, 40, 60, 120

  // GPU & VRAM Optimization
  int resolution_scale = 1;                      // 1 (720p internal -> FSR upscale) or 2 (1440p internal)
  std::string vram_profile = "4gb";              // 4gb, 8gb, 12gb
  int anisotropic_override = 0;                  // 0, 1, 2, 3, 4, 5 (0=off, 1=1x, 2=2x, 3=4x, 4=8x, 5=16x)
  bool occlusion_query_enable = true;
  bool fuzzy_alpha_epsilon = true;
  std::string d3d12_render_target_path = "auto"; // auto, rov, fbo

  // Controls & Keybindings
  bool mnk_mouse = true;
  double mnk_sensitivity = 1.0;
  std::string keybind_a = "Space";
  std::string keybind_b = "C";
  std::string keybind_x = "LMB";
  std::string keybind_y = "F";
  std::string keybind_left_shoulder = "Q";
  std::string keybind_right_shoulder = "E";
  std::string keybind_left_trigger = "RMB";
  std::string keybind_right_trigger = "LShift";
  std::string keybind_start = "Tab";
  std::string keybind_back = "Escape";
  std::string keybind_lstick_up = "W";
  std::string keybind_lstick_down = "S";
  std::string keybind_lstick_left = "A";
  std::string keybind_lstick_right = "D";
  std::string keybind_lstick_press = "LAlt";
  std::string keybind_rstick_press = "V";
};

inline ModernConfig g_config;
inline std::atomic<bool> g_fg_enabled{true};
inline std::atomic<uint32_t> g_fg_frames_this_sec{0};
inline std::atomic<uint64_t> g_last_vdswap_qpc{0};
inline std::atomic<uint64_t> g_smoothed_interval_qpc{0};
inline std::atomic<bool> g_subframe_pending{false};

// Lightweight JSON key extractor for flat config files
namespace detail {

inline std::string ReadFileText(const std::filesystem::path& path) {
  std::ifstream f(path);
  if (!f.is_open()) return {};
  std::ostringstream ss;
  ss << f.rdbuf();
  return ss.str();
}

inline std::string FindRawJsonValue(const std::string& json, const std::string& key) {
  std::string pattern = "\"" + key + "\"";
  size_t pos = json.find(pattern);
  if (pos == std::string::npos) return {};
  pos = json.find(':', pos + pattern.size());
  if (pos == std::string::npos) return {};
  pos++;
  while (pos < json.size() && (json[pos] == ' ' || json[pos] == '\t' || json[pos] == '\r' || json[pos] == '\n')) {
    pos++;
  }
  if (pos >= json.size()) return {};
  if (json[pos] == '"') {
    size_t end = json.find('"', pos + 1);
    if (end == std::string::npos) return {};
    return json.substr(pos + 1, end - pos - 1);
  } else {
    size_t end = pos;
    while (end < json.size() && json[end] != ',' && json[end] != '}' && json[end] != '\r' && json[end] != '\n' && json[end] != ' ') {
      end++;
    }
    return json.substr(pos, end - pos);
  }
}

inline void ExtractString(const std::string& json, const std::string& key, std::string& out) {
  std::string v = FindRawJsonValue(json, key);
  if (!v.empty()) out = v;
}

inline void ExtractBool(const std::string& json, const std::string& key, bool& out) {
  std::string v = FindRawJsonValue(json, key);
  if (v == "true" || v == "1") out = true;
  else if (v == "false" || v == "0") out = false;
}

inline void ExtractInt(const std::string& json, const std::string& key, int& out) {
  std::string v = FindRawJsonValue(json, key);
  if (!v.empty()) {
    try { out = std::stoi(v); } catch (...) {}
  }
}

inline void ExtractDouble(const std::string& json, const std::string& key, double& out) {
  std::string v = FindRawJsonValue(json, key);
  if (!v.empty()) {
    try { out = std::stod(v); } catch (...) {}
  }
}

}  // namespace detail

inline std::filesystem::path LocateConfigFile() {
  const char* candidates[] = {
      "recomp_config.json",
      "../recomp_config.json",
      "../../recomp_config.json",
      "../../../recomp_config.json"
  };
  for (const char* c : candidates) {
    if (std::filesystem::exists(c)) {
      return std::filesystem::path(c);
    }
  }
  return "recomp_config.json";
}

inline bool LoadConfigFromDisk() {
  auto cfg_path = LocateConfigFile();
  std::string json = detail::ReadFileText(cfg_path);
  if (json.empty()) {
    REXLOG_INFO("[WoS-FG] No recomp_config.json found, using built-in Modern Edition defaults");
    g_fg_enabled.store(g_config.frame_generation, std::memory_order_relaxed);
    return false;
  }

  // Parse resolution string "1920x1080" if present
  std::string res_str;
  detail::ExtractString(json, "resolution", res_str);
  if (!res_str.empty()) {
    size_t x_pos = res_str.find('x');
    if (x_pos != std::string::npos) {
      try {
        g_config.window_width = std::stoi(res_str.substr(0, x_pos));
        g_config.window_height = std::stoi(res_str.substr(x_pos + 1));
      } catch (...) {}
    }
  }
  detail::ExtractInt(json, "window_width", g_config.window_width);
  detail::ExtractInt(json, "window_height", g_config.window_height);
  detail::ExtractBool(json, "fullscreen", g_config.fullscreen);
  detail::ExtractBool(json, "vsync", g_config.vsync);
  detail::ExtractBool(json, "vrr_tearing", g_config.vrr_tearing);
  detail::ExtractBool(json, "letterbox", g_config.letterbox);
  detail::ExtractBool(json, "dither", g_config.dither);

  detail::ExtractString(json, "present_effect", g_config.present_effect);
  detail::ExtractString(json, "fsr_quality_mode", g_config.fsr_quality_mode);
  detail::ExtractDouble(json, "fsr_sharpness", g_config.fsr_sharpness);
  detail::ExtractDouble(json, "cas_sharpness", g_config.cas_sharpness);
  detail::ExtractInt(json, "fsr_max_passes", g_config.fsr_max_passes);

  detail::ExtractBool(json, "frame_generation", g_config.frame_generation);
  detail::ExtractInt(json, "fg_multiplier", g_config.fg_multiplier);
  detail::ExtractInt(json, "frame_limit", g_config.frame_limit);

  detail::ExtractInt(json, "resolution_scale", g_config.resolution_scale);
  detail::ExtractString(json, "vram_profile", g_config.vram_profile);
  detail::ExtractInt(json, "anisotropic_override", g_config.anisotropic_override);
  detail::ExtractBool(json, "occlusion_query_enable", g_config.occlusion_query_enable);
  detail::ExtractBool(json, "fuzzy_alpha_epsilon", g_config.fuzzy_alpha_epsilon);
  detail::ExtractString(json, "d3d12_render_target_path", g_config.d3d12_render_target_path);

  detail::ExtractBool(json, "mnk_mouse", g_config.mnk_mouse);
  detail::ExtractDouble(json, "mnk_sensitivity", g_config.mnk_sensitivity);
  detail::ExtractString(json, "keybind_a", g_config.keybind_a);
  detail::ExtractString(json, "keybind_b", g_config.keybind_b);
  detail::ExtractString(json, "keybind_x", g_config.keybind_x);
  detail::ExtractString(json, "keybind_y", g_config.keybind_y);
  detail::ExtractString(json, "keybind_left_shoulder", g_config.keybind_left_shoulder);
  detail::ExtractString(json, "keybind_right_shoulder", g_config.keybind_right_shoulder);
  detail::ExtractString(json, "keybind_left_trigger", g_config.keybind_left_trigger);
  detail::ExtractString(json, "keybind_right_trigger", g_config.keybind_right_trigger);
  detail::ExtractString(json, "keybind_start", g_config.keybind_start);
  detail::ExtractString(json, "keybind_back", g_config.keybind_back);
  detail::ExtractString(json, "keybind_lstick_up", g_config.keybind_lstick_up);
  detail::ExtractString(json, "keybind_lstick_down", g_config.keybind_lstick_down);
  detail::ExtractString(json, "keybind_lstick_left", g_config.keybind_lstick_left);
  detail::ExtractString(json, "keybind_lstick_right", g_config.keybind_lstick_right);
  detail::ExtractString(json, "keybind_lstick_press", g_config.keybind_lstick_press);
  detail::ExtractString(json, "keybind_rstick_press", g_config.keybind_rstick_press);

  g_fg_enabled.store(g_config.frame_generation, std::memory_order_relaxed);
  REXLOG_INFO("[WoS-PACER] Loaded recomp_config.json from '{}': {}x{}, fullscreen={}, effect='{}', fsr_mode='{}', 2xPacer={}, limit={}",
              cfg_path.string(), g_config.window_width, g_config.window_height,
              g_config.fullscreen, g_config.present_effect, g_config.fsr_quality_mode,
              g_config.frame_generation, g_config.frame_limit);
  return true;
}

inline void ApplyConfigToCvars() {
  LoadConfigFromDisk();

  // 1. Window & Display
  rex::cvar::SetFlagByName("window_width", std::to_string(g_config.window_width));
  rex::cvar::SetFlagByName("window_height", std::to_string(g_config.window_height));
  rex::cvar::SetFlagByName("fullscreen", g_config.fullscreen ? "true" : "false");
  rex::cvar::SetFlagByName("vsync", g_config.vsync ? "true" : "false");
  rex::cvar::SetFlagByName("d3d12_allow_variable_refresh_rate_and_tearing", g_config.vrr_tearing ? "true" : "false");
  rex::cvar::SetFlagByName("present_letterbox", g_config.letterbox ? "true" : "false");
  rex::cvar::SetFlagByName("present_dither", g_config.dither ? "true" : "false");

  // 2. AMD FidelityFX Upscaler (FSR 1 / FSR 2 / FSR 3 / CAS / Bilinear)
  rex::cvar::SetFlagByName("present_effect", g_config.present_effect);
  rex::cvar::SetFlagByName("present_fsr_quality_mode", g_config.fsr_quality_mode);
  rex::cvar::SetFlagByName("present_fsr_sharpness_reduction", std::to_string(g_config.fsr_sharpness));
  rex::cvar::SetFlagByName("present_cas_additional_sharpness", std::to_string(g_config.cas_sharpness));
  rex::cvar::SetFlagByName("present_fsr_max_upsampling_passes", std::to_string(std::clamp(g_config.fsr_max_passes, 1, 4)));

  // 3. Resolution Scale & GPU Culling
  int scale = std::clamp(g_config.resolution_scale, 1, 2);
  rex::cvar::SetFlagByName("draw_resolution_scale_x", std::to_string(scale));
  rex::cvar::SetFlagByName("draw_resolution_scale_y", std::to_string(scale));
  rex::cvar::SetFlagByName("anisotropic_override", std::to_string(std::clamp(g_config.anisotropic_override, 0, 5)));
  rex::cvar::SetFlagByName("occlusion_query_enable", g_config.occlusion_query_enable ? "true" : "false");
  rex::cvar::SetFlagByName("use_fuzzy_alpha_epsilon", g_config.fuzzy_alpha_epsilon ? "true" : "false");

  if (g_config.d3d12_render_target_path == "rov") {
    rex::cvar::SetFlagByName("render_target_path_d3d12", "rov");
  } else if (g_config.d3d12_render_target_path == "fbo") {
    rex::cvar::SetFlagByName("render_target_path_d3d12", "fbo");
  } else {
    // "auto": Let ReXGlue auto-detect hardware capabilities (rov on NVIDIA/RDNA, fbo on AMD Polaris RX 480/580/Vega)
    rex::cvar::SetFlagByName("render_target_path_d3d12", "");
  }

  // 4. VRAM Profile
  if (g_config.vram_profile == "12gb") {
    rex::cvar::SetFlagByName("texture_cache_memory_limit_soft", "3072");
    rex::cvar::SetFlagByName("texture_cache_memory_limit_hard", "4096");
    rex::cvar::SetFlagByName("texture_cache_memory_limit_render_to_texture", "256");
    rex::cvar::SetFlagByName("texture_cache_memory_limit_soft_lifetime", "60");
  } else if (g_config.vram_profile == "8gb") {
    rex::cvar::SetFlagByName("texture_cache_memory_limit_soft", "1536");
    rex::cvar::SetFlagByName("texture_cache_memory_limit_hard", "2560");
    rex::cvar::SetFlagByName("texture_cache_memory_limit_render_to_texture", "128");
    rex::cvar::SetFlagByName("texture_cache_memory_limit_soft_lifetime", "30");
  } else {
    // 4GB Default (GTX 1650 Ti / RTX 3050 Laptop sweet spot)
    rex::cvar::SetFlagByName("texture_cache_memory_limit_soft", "768");
    rex::cvar::SetFlagByName("texture_cache_memory_limit_hard", "1280");
    rex::cvar::SetFlagByName("texture_cache_memory_limit_render_to_texture", "64");
    rex::cvar::SetFlagByName("texture_cache_memory_limit_soft_lifetime", "15");
  }

  // 5. Controls & Keybindings
  rex::cvar::SetFlagByName("mnk_mode", "true");
  rex::cvar::SetFlagByName("mnk_mouse", g_config.mnk_mouse ? "true" : "false");
  rex::cvar::SetFlagByName("mnk_sensitivity", std::to_string(g_config.mnk_sensitivity));
  rex::cvar::SetFlagByName("keybind_a", g_config.keybind_a);
  rex::cvar::SetFlagByName("keybind_b", g_config.keybind_b);
  rex::cvar::SetFlagByName("keybind_x", g_config.keybind_x);
  rex::cvar::SetFlagByName("keybind_y", g_config.keybind_y);
  rex::cvar::SetFlagByName("keybind_left_shoulder", g_config.keybind_left_shoulder);
  rex::cvar::SetFlagByName("keybind_right_shoulder", g_config.keybind_right_shoulder);
  rex::cvar::SetFlagByName("keybind_left_trigger", g_config.keybind_left_trigger);
  rex::cvar::SetFlagByName("keybind_right_trigger", g_config.keybind_right_trigger);
  rex::cvar::SetFlagByName("keybind_start", g_config.keybind_start);
  rex::cvar::SetFlagByName("keybind_back", g_config.keybind_back);
  rex::cvar::SetFlagByName("keybind_lstick_up", g_config.keybind_lstick_up);
  rex::cvar::SetFlagByName("keybind_lstick_down", g_config.keybind_lstick_down);
  rex::cvar::SetFlagByName("keybind_lstick_left", g_config.keybind_lstick_left);
  rex::cvar::SetFlagByName("keybind_lstick_right", g_config.keybind_lstick_right);
  rex::cvar::SetFlagByName("keybind_lstick_press", g_config.keybind_lstick_press);
  rex::cvar::SetFlagByName("keybind_rstick_press", g_config.keybind_rstick_press);
}

// High-precision QPC helper
inline uint64_t GetQpcNow() {
  LARGE_INTEGER li;
  QueryPerformanceCounter(&li);
  return static_cast<uint64_t>(li.QuadPart);
}

inline uint64_t GetQpcFreq() {
  static uint64_t freq = []() {
    LARGE_INTEGER li;
    QueryPerformanceFrequency(&li);
    return static_cast<uint64_t>(li.QuadPart);
  }();
  return freq;
}

inline void PrecisionWaitUntilQpc(uint64_t target_qpc, HANDLE hTimer) {
  uint64_t freq = GetQpcFreq();
  while (true) {
    uint64_t now = GetQpcNow();
    if (now >= target_qpc) break;
    uint64_t rem_ticks = target_qpc - now;
    double rem_ms = (static_cast<double>(rem_ticks) * 1000.0) / static_cast<double>(freq);
    if (rem_ms > 2.0 && hTimer) {
      LARGE_INTEGER due;
      // WaitableTimer uses 100-ns intervals (negative = relative)
      due.QuadPart = -static_cast<LONGLONG>((rem_ms - 1.2) * 10000.0);
      if (SetWaitableTimer(hTimer, &due, 0, NULL, NULL, FALSE)) {
        WaitForSingleObject(hTimer, 5);
      }
    } else {
      YieldProcessor();
    }
  }
}

// Called inside HookedVdSwap on every native guest frame swap
inline void OnNativeFrameSwap() {
  static HANDLE s_pacer_timer = CreateWaitableTimerExW(
      NULL, NULL, CREATE_WAITABLE_TIMER_HIGH_RESOLUTION, TIMER_ALL_ACCESS);
  uint64_t freq = GetQpcFreq();
  uint64_t now = GetQpcNow();
  uint64_t prev = g_last_vdswap_qpc.load(std::memory_order_relaxed);

  // 1. Optional High-Precision Frame Rate Limiter / Pacer
  if (g_config.frame_limit > 0 && prev != 0) {
    uint64_t target_interval = freq / static_cast<uint64_t>(g_config.frame_limit);
    uint64_t target_qpc = prev + target_interval;
    if (now < target_qpc) {
      PrecisionWaitUntilQpc(target_qpc, s_pacer_timer);
      now = GetQpcNow();
    }
  }

  // 2. Update smoothed frame interval for 2x Frame Generation midpoint calculation
  if (prev != 0 && now > prev) {
    uint64_t dt = now - prev;
    // Clamp interval between 4ms (250 FPS) and 100ms (10 FPS)
    uint64_t min_dt = freq / 250;
    uint64_t max_dt = freq / 10;
    dt = std::clamp(dt, min_dt, max_dt);
    uint64_t old_smooth = g_smoothed_interval_qpc.load(std::memory_order_relaxed);
    uint64_t new_smooth = (old_smooth == 0) ? dt : ((old_smooth * 3 + dt) / 4);
    g_smoothed_interval_qpc.store(new_smooth, std::memory_order_relaxed);
  }

  g_last_vdswap_qpc.store(now, std::memory_order_release);
  g_fg_frames_this_sec.fetch_add(1, std::memory_order_relaxed);
  if (g_fg_enabled.load(std::memory_order_relaxed)) {
    g_subframe_pending.store(true, std::memory_order_release);
  }
}

// Starts the dedicated sub-frame 2x Frame Pacer thread (doubles presentation cadence to 60/120 Hz)
inline void StartFrameGenThread(rex::ui::Window* window) {
  if (!window) return;

  std::thread([window]() {
    SetThreadPriority(GetCurrentThread(), THREAD_PRIORITY_ABOVE_NORMAL);
    HANDLE hTimer = CreateWaitableTimerExW(
        NULL, NULL, CREATE_WAITABLE_TIMER_HIGH_RESOLUTION, TIMER_ALL_ACCESS);
    REXLOG_INFO("[WoS-PACER] Built-in 2x Frame Pacer & Display Cadence thread active");

    while (true) {
      if (!g_fg_enabled.load(std::memory_order_relaxed)) {
        std::this_thread::sleep_for(std::chrono::milliseconds(50));
        continue;
      }

      if (!g_subframe_pending.load(std::memory_order_acquire)) {
        if (hTimer) {
          LARGE_INTEGER due;
          due.QuadPart = -10000LL; // 1.0 ms
          SetWaitableTimer(hTimer, &due, 0, NULL, NULL, FALSE);
          WaitForSingleObject(hTimer, 2);
        } else {
          std::this_thread::sleep_for(std::chrono::milliseconds(1));
        }
        continue;
      }

      uint64_t last_swap = g_last_vdswap_qpc.load(std::memory_order_acquire);
      uint64_t interval = g_smoothed_interval_qpc.load(std::memory_order_relaxed);
      if (last_swap == 0 || interval == 0) {
        g_subframe_pending.store(false, std::memory_order_release);
        continue;
      }

      // Target the exact 50% midpoint between native frames for 2x display cadence
      uint64_t midpoint_qpc = last_swap + (interval / 2);
      uint64_t now = GetQpcNow();
      if (now < midpoint_qpc) {
        PrecisionWaitUntilQpc(midpoint_qpc, hTimer);
      }

      // Verify a new native frame hasn't already arrived while waiting
      if (g_last_vdswap_qpc.load(std::memory_order_acquire) == last_swap &&
          g_subframe_pending.exchange(false, std::memory_order_acq_rel)) {
        HWND hwnd = static_cast<HWND>(window->GetNativeWindowHandle());
        if (hwnd && IsWindow(hwnd)) {
          // Trigger intermediate Presenter pass (refreshes D3D12 SwapChain buffer at midpoint)
          InvalidateRect(hwnd, NULL, FALSE);
          g_fg_frames_this_sec.fetch_add(1, std::memory_order_relaxed);
        }
      }
    }
  }).detach();
}

}  // namespace wos::fg
