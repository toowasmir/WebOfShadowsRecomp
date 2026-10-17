// wos_recomp - Real-time Performance & Telemetry Monitor
//
// Spider-Man: Web of Shadows (Xbox 360 -> Win64 Static Recompilation)
//
// Measures real-time Native FPS, Effective AMD FSR FrameGen FPS, frame times,
// and process CPU usage. Displays metrics on the Window title bar and exports
// last_session_stats.json for the Modern Edition Launcher.

#pragma once

#include <windows.h>
#include <atomic>
#include <chrono>
#include <string>
#include <format>
#include <fstream>
#include <algorithm>
#include <rex/logging.h>
#include <rex/ui/window.h>

#include "frame_generation.h"

namespace wos::perf {

inline std::atomic<uint64_t> g_total_frames{0};
inline std::atomic<uint32_t> g_frames_this_sec{0};
inline std::atomic<double> g_current_fps{0.0};
inline std::atomic<double> g_current_fg_fps{0.0};
inline std::atomic<double> g_current_frametime_ms{0.0};
inline std::atomic<double> g_current_cpu_pct{0.0};

inline void RecordFrame() {
  g_total_frames.fetch_add(1, std::memory_order_relaxed);
  g_frames_this_sec.fetch_add(1, std::memory_order_relaxed);
  wos::fg::OnNativeFrameSwap();
}

inline double MeasureProcessCpuUsage(uint32_t elapsed_ms) {
  static uint64_t prev_proc_time = 0;
  static SYSTEM_INFO sys_info = []() {
    SYSTEM_INFO si;
    GetSystemInfo(&si);
    return si;
  }();

  FILETIME create_time, exit_time, kernel_time, user_time;
  if (!GetProcessTimes(GetCurrentProcess(), &create_time, &exit_time, &kernel_time, &user_time)) {
    return 0.0;
  }

  ULARGE_INTEGER kt, ut;
  kt.LowPart = kernel_time.dwLowDateTime; kt.HighPart = kernel_time.dwHighDateTime;
  ut.LowPart = user_time.dwLowDateTime;   ut.HighPart = user_time.dwHighDateTime;
  uint64_t total_proc = kt.QuadPart + ut.QuadPart;

  double cpu_pct = 0.0;
  if (prev_proc_time != 0 && elapsed_ms > 0 && sys_info.dwNumberOfProcessors > 0) {
    uint64_t diff = total_proc - prev_proc_time;
    double total_capacity = static_cast<double>(elapsed_ms) * 10000.0 * static_cast<double>(sys_info.dwNumberOfProcessors);
    if (total_capacity > 0.0) {
      cpu_pct = (static_cast<double>(diff) / total_capacity) * 100.0;
      cpu_pct = std::clamp(cpu_pct, 0.0, 100.0);
    }
  }
  prev_proc_time = total_proc;
  return cpu_pct;
}

inline void StartTelemetryThread(rex::ui::Window* window) {
  if (!window) return;
  wos::fg::StartFrameGenThread(window);

  std::thread([window]() {
    REXLOG_INFO("[WoS-PERF] Performance telemetry thread started (1s update interval, F4 toggles 2x Frame Pacer)");
    auto last_time = std::chrono::steady_clock::now();
    uint32_t log_counter = 0;
    double sum_native_fps = 0.0;
    double sum_fg_fps = 0.0;
    double max_fg_fps = 0.0;
    uint64_t stat_samples = 0;
    bool prev_f4_down = false;

    while (true) {
      for (int i = 0; i < 10; ++i) {
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
        bool f4_down = (GetAsyncKeyState(VK_F4) & 0x8000) != 0;
        if (f4_down && !prev_f4_down) {
          bool new_state = !wos::fg::g_fg_enabled.load(std::memory_order_relaxed);
          wos::fg::g_fg_enabled.store(new_state, std::memory_order_relaxed);
          REXLOG_INFO("[WoS-PACER] Hotkey F4 pressed: Built-in 2x Frame Pacer is now {}",
                      new_state ? "ENABLED (2x)" : "DISABLED (1x)");
        }
        prev_f4_down = f4_down;
      }

      auto now = std::chrono::steady_clock::now();
      auto elapsed_ms = static_cast<uint32_t>(
          std::chrono::duration_cast<std::chrono::milliseconds>(now - last_time).count());
      last_time = now;

      uint32_t frames = g_frames_this_sec.exchange(0, std::memory_order_relaxed);
      uint32_t fg_frames = wos::fg::g_fg_frames_this_sec.exchange(0, std::memory_order_relaxed);
      double fps = (elapsed_ms > 0) ? (static_cast<double>(frames) * 1000.0 / elapsed_ms) : 0.0;
      bool fg_on = wos::fg::g_fg_enabled.load(std::memory_order_relaxed);
      double fg_fps = fg_on
          ? std::max(fps * 2.0, (elapsed_ms > 0) ? (static_cast<double>(fg_frames) * 1000.0 / elapsed_ms) : 0.0)
          : fps;
      double ft_ms = (fps > 0.1) ? (1000.0 / fps) : 0.0;
      double cpu_pct = MeasureProcessCpuUsage(elapsed_ms);

      g_current_fps.store(fps, std::memory_order_relaxed);
      g_current_fg_fps.store(fg_fps, std::memory_order_relaxed);
      g_current_frametime_ms.store(ft_ms, std::memory_order_relaxed);
      g_current_cpu_pct.store(cpu_pct, std::memory_order_relaxed);

      if (fps > 5.0) {
        sum_native_fps += fps;
        sum_fg_fps += fg_fps;
        if (fg_fps > max_fg_fps) max_fg_fps = fg_fps;
        stat_samples++;
      }

      std::string upscaler_tag = wos::fg::g_config.present_effect;
      for (char& c : upscaler_tag) c = static_cast<char>( std::toupper(static_cast<unsigned char>(c)) );

      std::string title;
      if (fg_on) {
        title = std::format(
            "Spider-Man: Web of Shadows | {}: {:.1f} FPS (2x Paced: {:.1f} Hz, {:.1f} ms) | CPU: {:.1f}% [F4: 2x Pacer]",
            upscaler_tag, fps, fg_fps, ft_ms, cpu_pct);
      } else {
        title = std::format(
            "Spider-Man: Web of Shadows | {}: {:.1f} FPS ({:.1f} ms) | CPU: {:.1f}% [F4: 2x Pacer]",
            upscaler_tag, fps, ft_ms, cpu_pct);
      }

      window->SetTitle(title);
      HWND hwnd = static_cast<HWND>(window->GetNativeWindowHandle());
      if (hwnd) {
        SetWindowTextA(hwnd, title.c_str());
      }

      if (++log_counter >= 5) {
        log_counter = 0;
        REXLOG_INFO("[WoS-PERF] Telemetry: Render_FPS: {:.1f} | Display_Hz (2x Paced): {:.1f} ({:.1f} ms) | CPU: {:.1f}% | Upscaler: {}",
                    fps, fg_fps, ft_ms, cpu_pct, upscaler_tag);

        if (stat_samples > 0) {
          std::ofstream sf("last_session_stats.json", std::ios::trunc);
          if (sf.is_open()) {
            sf << "{\n"
               << "  \"avg_native_fps\": " << std::format("{:.1f}", sum_native_fps / stat_samples) << ",\n"
               << "  \"avg_fg_fps\": " << std::format("{:.1f}", sum_fg_fps / stat_samples) << ",\n"
               << "  \"peak_fg_fps\": " << std::format("{:.1f}", max_fg_fps) << ",\n"
               << "  \"last_frametime_ms\": " << std::format("{:.1f}", ft_ms) << ",\n"
               << "  \"last_cpu_pct\": " << std::format("{:.1f}", cpu_pct) << ",\n"
               << "  \"upscaler\": \"" << upscaler_tag << "\",\n"
               << "  \"frame_generation\": " << (fg_on ? "true" : "false") << "\n"
               << "}\n";
          }
        }
      }
    }
  }).detach();
}

}  // namespace wos::perf
