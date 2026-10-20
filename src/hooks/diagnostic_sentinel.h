#pragma once

#include <windows.h>
#include <d3d12.h>
#include <dxgi1_6.h>
#include <dbghelp.h>
#include <psapi.h>

#include <chrono>
#include <string>
#include <vector>
#include <atomic>
#include <fstream>
#include <sstream>
#include <iomanip>
#include <filesystem>
#include <format>
#include <algorithm>

#include <rex/logging.h>
#include <rex/cvar.h>

namespace wos::diag {

struct DiagnosticReport {
  std::string timestamp;
  std::string status = "HEALTHY";
  std::string root_cause = "None";
  std::string recommendation = "None";

  // Hardware & Driver
  std::string gpu_name = "Unknown";
  std::string driver_version = "Unknown";
  uint64_t dedicated_vram_mb = 0;
  uint64_t shared_sys_ram_mb = 0;
  bool rov_supported = false;
  std::string max_feature_level = "Unknown";

  // OS & Environment
  std::string os_info = "Windows";
  bool is_wine_proton = false;
  std::string wine_version = "";

  // Game File
  bool xex_exists = false;
  uint64_t xex_size_bytes = 0;
  std::string xex_path = "";

  // Session Stats
  double session_duration_s = 0.0;
  double avg_fps = 0.0;
  double peak_fps = 0.0;
  double peak_ram_mb = 0.0;
  double last_ram_mb = 0.0;
  uint32_t stutter_spikes_count = 0;
  uint64_t black_frames_count = 0;

  // Crash Dump
  bool crashed = false;
  std::string minidump_path = "";
  uint32_t crash_code = 0;
  uintptr_t fault_addr = 0;
  uintptr_t rip = 0;
  std::string fault_module = "";
};

inline DiagnosticReport g_diag;
inline std::atomic<bool> g_initialized{false};
inline std::chrono::steady_clock::time_point g_boot_time;
inline std::atomic<uint32_t> g_stutter_spikes{0};
inline std::atomic<uint64_t> g_black_frame_samples{0};
inline std::atomic<double> g_peak_ram_mb{0.0};
inline std::atomic<double> g_last_ram_mb{0.0};

// Forward declaration of dump generator
inline void WriteDiagnosticSummaryFiles(bool is_crash, const std::string& custom_reason = "");

// ---------------------------------------------------------------------------
// 1. Hardware & Environment Probe (Called during startup)
// ---------------------------------------------------------------------------
inline void ProbeHardwareAndEnvironment() {
  if (g_initialized.exchange(true)) return;
  g_boot_time = std::chrono::steady_clock::now();

  auto now = std::chrono::system_clock::now();
  auto in_time_t = std::chrono::system_clock::to_time_t(now);
  struct tm local_tm {};
  localtime_s(&local_tm, &in_time_t);
  std::stringstream ss_time;
  ss_time << std::put_time(&local_tm, "%Y-%m-%d %H:%M:%S");
  g_diag.timestamp = ss_time.str();

  // A. Detect Wine / Proton (SteamOS)
  HMODULE hNtdll = GetModuleHandleA("ntdll.dll");
  if (hNtdll) {
    typedef const char* (CDECL *wine_get_version_fn)();
    auto pWineGetVersion = (wine_get_version_fn)GetProcAddress(hNtdll, "wine_get_version");
    if (pWineGetVersion) {
      g_diag.is_wine_proton = true;
      g_diag.wine_version = pWineGetVersion();
      g_diag.os_info = std::format("Linux/SteamOS via Proton (Wine {})", g_diag.wine_version);
    } else {
      g_diag.is_wine_proton = false;
      OSVERSIONINFOEXW osvi = { sizeof(osvi) };
      typedef LONG (WINAPI* RtlGetVersionFn)(PRTL_OSVERSIONINFOW);
      auto pRtlGetVersion = (RtlGetVersionFn)GetProcAddress(hNtdll, "RtlGetVersion");
      if (pRtlGetVersion) {
        pRtlGetVersion((PRTL_OSVERSIONINFOW)&osvi);
        g_diag.os_info = std::format("Windows {}.{}.{} (Native 64-bit)",
                                     osvi.dwMajorVersion, osvi.dwMinorVersion, osvi.dwBuildNumber);
      } else {
        g_diag.os_info = "Windows (Native 64-bit)";
      }
    }
  }

  // B. Detect Game Executable (.XEX)
  for (const auto& p : { "game/default.xex", "../game/default.xex", "default.xex" }) {
    std::error_code ec;
    if (std::filesystem::exists(p, ec)) {
      g_diag.xex_exists = true;
      g_diag.xex_path = p;
      g_diag.xex_size_bytes = std::filesystem::file_size(p, ec);
      break;
    }
  }

  // C. Direct3D 12 & DXGI Hardware Probe
  HMODULE hDxgi = LoadLibraryA("dxgi.dll");
  HMODULE hD3D12 = LoadLibraryA("d3d12.dll");

  if (hDxgi && hD3D12) {
    typedef HRESULT (WINAPI *CreateDXGIFactory1Fn)(REFIID, void**);
    typedef HRESULT (WINAPI *D3D12CreateDeviceFn)(IUnknown*, D3D_FEATURE_LEVEL, REFIID, void**);

    auto pCreateDXGIFactory1 = (CreateDXGIFactory1Fn)GetProcAddress(hDxgi, "CreateDXGIFactory1");
    auto pD3D12CreateDevice = (D3D12CreateDeviceFn)GetProcAddress(hD3D12, "D3D12CreateDevice");

    if (pCreateDXGIFactory1 && pD3D12CreateDevice) {
      IDXGIFactory4* factory = nullptr;
      if (SUCCEEDED(pCreateDXGIFactory1(IID_PPV_ARGS(&factory)))) {
        IDXGIAdapter1* adapter = nullptr;
        // Enumerate high performance primary adapter
        for (UINT i = 0; SUCCEEDED(factory->EnumAdapters1(i, &adapter)); ++i) {
          DXGI_ADAPTER_DESC1 desc;
          adapter->GetDesc1(&desc);
          if (desc.Flags & DXGI_ADAPTER_FLAG_SOFTWARE) {
            adapter->Release();
            continue;
          }

          // Convert wide description to string
          std::wstring ws(desc.Description);
          g_diag.gpu_name = std::string(ws.begin(), ws.end());
          g_diag.dedicated_vram_mb = desc.DedicatedVideoMemory / (1024 * 1024);
          g_diag.shared_sys_ram_mb = desc.SharedSystemMemory / (1024 * 1024);

          // Test D3D12 Device creation and ROV feature support
          ID3D12Device* device = nullptr;
          if (SUCCEEDED(pD3D12CreateDevice(adapter, D3D_FEATURE_LEVEL_11_0, IID_PPV_ARGS(&device)))) {
            // Check Feature Levels
            D3D_FEATURE_LEVEL levels[] = {
              D3D_FEATURE_LEVEL_12_2,
              D3D_FEATURE_LEVEL_12_1,
              D3D_FEATURE_LEVEL_12_0,
              D3D_FEATURE_LEVEL_11_1,
              D3D_FEATURE_LEVEL_11_0,
            };
            D3D12_FEATURE_DATA_FEATURE_LEVELS fl_data = { _countof(levels), levels, D3D_FEATURE_LEVEL_11_0 };
            if (SUCCEEDED(device->CheckFeatureSupport(D3D12_FEATURE_FEATURE_LEVELS, &fl_data, sizeof(fl_data)))) {
              switch (fl_data.MaxSupportedFeatureLevel) {
                case D3D_FEATURE_LEVEL_12_2: g_diag.max_feature_level = "12_2 (Ultimate)"; break;
                case D3D_FEATURE_LEVEL_12_1: g_diag.max_feature_level = "12_1"; break;
                case D3D_FEATURE_LEVEL_12_0: g_diag.max_feature_level = "12_0"; break;
                case D3D_FEATURE_LEVEL_11_1: g_diag.max_feature_level = "11_1"; break;
                default: g_diag.max_feature_level = "11_0"; break;
              }
            }

            // Check ROV (Rasterizer-Ordered Views) support
            D3D12_FEATURE_DATA_D3D12_OPTIONS options = {};
            if (SUCCEEDED(device->CheckFeatureSupport(D3D12_FEATURE_D3D12_OPTIONS, &options, sizeof(options)))) {
              g_diag.rov_supported = (options.ROVsSupported != FALSE);
            }

            device->Release();
          }

          adapter->Release();
          break; // Primary discrete GPU inspected
        }
        factory->Release();
      }
    }
  }

  // D. Emit Formatted Startup Probe Log
  REXLOG_INFO("================================================================================");
  REXLOG_INFO("              WoS RECOMP OMNISCIENT DIAGNOSTIC PROBE (v2.0.2)");
  REXLOG_INFO("================================================================================");
  REXLOG_INFO("[DIAG-OS] Operating System: {}", g_diag.os_info);
  REXLOG_INFO("[DIAG-GPU] Adapter Name:   {}", g_diag.gpu_name);
  REXLOG_INFO("[DIAG-GPU] Dedicated VRAM: {} MB (Shared RAM: {} MB)", g_diag.dedicated_vram_mb, g_diag.shared_sys_ram_mb);
  REXLOG_INFO("[DIAG-GPU] D3D12 Level:    {}", g_diag.max_feature_level);

  if (g_diag.rov_supported) {
    REXLOG_INFO("[DIAG-GPU] Direct3D 12 ROV: [SUPPORTED] (Hardware Rasterizer-Ordered Views verified)");
  } else {
    REXLOG_ERROR("--------------------------------------------------------------------------------");
    REXLOG_ERROR("[DIAG-ALERT] *** CRITICAL: GPU LACKS RASTERIZER-ORDERED VIEWS (ROV) SUPPORT! ***");
    REXLOG_ERROR("[DIAG-ALERT] Spider-Man: Web of Shadows requires hardware ROV for Xenos eDRAM blending.");
    REXLOG_ERROR("[DIAG-ALERT] 3D gameplay on this GPU (e.g. AMD Polaris RX 580) may produce a BLACK SCREEN!");
    REXLOG_ERROR("--------------------------------------------------------------------------------");
    g_diag.status = "WARNING_NO_ROV";
    g_diag.root_cause = "GPU lacks Direct3D 12 Rasterizer-Ordered Views (ROV) support required for 3D eDRAM blending.";
    g_diag.recommendation = "Run on an ROV-compatible GPU (NVIDIA GTX 900/1000/RTX, AMD RX 5000/6000/7000/Z1/Z2, Intel Arc).";
  }

  if (g_diag.xex_exists) {
    REXLOG_INFO("[DIAG-XEX] Game Image:     {} (Size: {} MB) [VERIFIED OK]",
                g_diag.xex_path, g_diag.xex_size_bytes / (1024 * 1024));
  } else {
    REXLOG_ERROR("[DIAG-XEX] Game Image:     'game/default.xex' [MISSING / NOT FOUND]");
    REXLOG_ERROR("[DIAG-XEX] Please extract your legal Xbox 360 copy into the 'game/' directory!");
    g_diag.status = "CRITICAL_MISSING_XEX";
    g_diag.root_cause = "game/default.xex was not found on disk.";
  }
  WriteDiagnosticSummaryFiles(false);
  REXLOG_INFO("================================================================================");
}

// ---------------------------------------------------------------------------
// 2. Effective CVAR State Verification
// ---------------------------------------------------------------------------
inline void LogEffectiveEngineState() {
  REXLOG_INFO("[DIAG-CONFIG] ---------------- EFFECTIVE ENGINE STATE ----------------");
  for (const char* key : {
    "render_target_path_d3d12", "occlusion_query_enable", "use_fuzzy_alpha_epsilon",
    "anisotropic_override", "texture_cache_memory_limit_soft", "texture_cache_memory_limit_hard",
    "texture_cache_memory_limit_soft_lifetime", "async_shader_compilation",
    "d3d12_pipeline_creation_threads", "d3d12_queue_priority", "present_effect", "vsync"
  }) {
    const auto* info = rex::cvar::GetFlagInfo(key);
    if (info && info->getter) {
      REXLOG_INFO("[DIAG-CONFIG] Active CVAR {:<35} = '{}'", key, info->getter());
    }
  }
  REXLOG_INFO("[DIAG-CONFIG] --------------------------------------------------------");
}

// ---------------------------------------------------------------------------
// 3. Runtime Telemetry, RAM Monitoring & Stutter Tracker
// ---------------------------------------------------------------------------
inline void OnTelemetryTick(double fps, double ft_ms, double cpu_pct) {
  // A. Query Private Working Set / RAM footprint
  PROCESS_MEMORY_COUNTERS_EX pmc = { sizeof(pmc) };
  if (K32GetProcessMemoryInfo(GetCurrentProcess(), (PROCESS_MEMORY_COUNTERS*)&pmc, sizeof(pmc))) {
    double ram_mb = static_cast<double>(pmc.PrivateUsage) / (1024.0 * 1024.0);
    g_last_ram_mb.store(ram_mb, std::memory_order_relaxed);

    double cur_peak = g_peak_ram_mb.load(std::memory_order_relaxed);
    while (ram_mb > cur_peak && !g_peak_ram_mb.compare_exchange_weak(cur_peak, ram_mb, std::memory_order_relaxed)) {}

    // Warn if RAM exceeds 3.5 GB (on 32-bit guest arena leak)
    if (ram_mb > 3500.0) {
      REXLOG_WARN("[DIAG-MEM-WARN] High RAM footprint: {:.1f} MB! Possible memory leak over long play session.", ram_mb);
    }
  }

  // B. Detect Frame Stutters (Frame time > 45 ms is a noticeable drop below 22 FPS)
  if (ft_ms > 45.0 && fps > 1.0) {
    uint32_t sp = g_stutter_spikes.fetch_add(1, std::memory_order_relaxed) + 1;
    if (sp <= 10 || (sp % 50 == 0)) {
      REXLOG_WARN("[DIAG-STUTTER] Frame spike detected: {:.1f} ms ({:.1f} FPS) | Spike #{:d} | RAM: {:.1f} MB",
                  ft_ms, fps, sp, g_last_ram_mb.load(std::memory_order_relaxed));
    }
  }
}

// ---------------------------------------------------------------------------
// 4. Crash Sentinel & Windows Minidump (.DMP) Generator
// ---------------------------------------------------------------------------
inline LONG WINAPI VectoredCrashHandler(PEXCEPTION_POINTERS p) {
  DWORD code = p->ExceptionRecord->ExceptionCode;

  // Intercept fatal hardware exceptions only (Do NOT intercept 0xE06D7363 C++ exceptions as they are caught by try/catch)
  if (code == EXCEPTION_ACCESS_VIOLATION ||
      code == EXCEPTION_ILLEGAL_INSTRUCTION ||
      code == EXCEPTION_INT_DIVIDE_BY_ZERO ||
      code == EXCEPTION_STACK_OVERFLOW) {

    static std::atomic<bool> s_dumped{false};
    if (s_dumped.exchange(true)) return EXCEPTION_CONTINUE_SEARCH;

    g_diag.crashed = true;
    g_diag.status = "CRASHED";
    g_diag.crash_code = code;
    g_diag.rip = (uintptr_t)p->ContextRecord->Rip;

    // Module name & RVA
    HMODULE hMod = NULL;
    GetModuleHandleExA(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
                       (LPCSTR)g_diag.rip, &hMod);
    char modName[MAX_PATH] = "Unknown";
    if (hMod) GetModuleFileNameA(hMod, modName, MAX_PATH);
    g_diag.fault_module = modName;
    uintptr_t rva = hMod ? (g_diag.rip - (uintptr_t)hMod) : 0;

    std::string root_explanation = "Fatal Exception";
    if (code == EXCEPTION_ACCESS_VIOLATION) {
      uintptr_t access_type = p->ExceptionRecord->ExceptionInformation[0];
      uintptr_t fault_addr = p->ExceptionRecord->ExceptionInformation[1];
      g_diag.fault_addr = fault_addr;

      std::string op = (access_type == 0) ? "READ" : (access_type == 1) ? "WRITE" : "EXECUTE";
      if (fault_addr == 0 || fault_addr < 0x10000) {
        root_explanation = std::format("NULL POINTER DEREFERENCE! Tried to {} address 0x{:08X}", op, fault_addr);
      } else if (fault_addr >= 0x0000000100000000ULL && fault_addr < 0x0000000200000000ULL) {
        root_explanation = std::format("Invalid Guest Memory Access! Arena offset 0x{:08X}", fault_addr - 0x100000000ULL);
      } else {
        root_explanation = std::format("Access Violation ({} at address 0x{:p})", op, (void*)fault_addr);
      }
    } else if (code == EXCEPTION_INT_DIVIDE_BY_ZERO) {
      root_explanation = "Integer Division by Zero!";
    } else if (code == EXCEPTION_STACK_OVERFLOW) {
      root_explanation = "Thread Stack Overflow!";
    }

    g_diag.root_cause = root_explanation;
    g_diag.recommendation = "Review generated .dmp minidump and callstack in Visual Studio or WinDbg.";

    REXLOG_ERROR("================================================================================");
    REXLOG_ERROR("                     *** WoS FATAL CRASH INTERCEPTED ***");
    REXLOG_ERROR("================================================================================");
    REXLOG_ERROR("[CRASH-CODE]   0x{:08X} ({})", code, root_explanation);
    REXLOG_ERROR("[CRASH-LOC]    RIP=0x{:p} in module '{}' (RVA=0x{:X})", (void*)g_diag.rip, modName, rva);
    REXLOG_ERROR("[CRASH-THREAD] Thread ID: {:d}", GetCurrentThreadId());

    // Generate MiniDump (.dmp)
    HMODULE hDbgHelp = LoadLibraryA("dbghelp.dll");
    if (hDbgHelp) {
      typedef BOOL (WINAPI *MiniDumpWriteDumpFn)(HANDLE, DWORD, HANDLE, MINIDUMP_TYPE,
                                                PMINIDUMP_EXCEPTION_INFORMATION,
                                                PMINIDUMP_USER_STREAM_INFORMATION,
                                                PMINIDUMP_CALLBACK_INFORMATION);
      auto pMiniDumpWriteDump = (MiniDumpWriteDumpFn)GetProcAddress(hDbgHelp, "MiniDumpWriteDump");
      if (pMiniDumpWriteDump) {
        std::error_code ec;
        std::filesystem::create_directories("logs", ec);

        auto now_t = std::chrono::system_clock::to_time_t(std::chrono::system_clock::now());
        struct tm crash_tm {};
        localtime_s(&crash_tm, &now_t);
        std::stringstream ss_fn;
        ss_fn << "logs/wos_crash_" << std::put_time(&crash_tm, "%Y%m%d_%H%M%S") << ".dmp";
        std::string dmp_path = ss_fn.str();

        HANDLE hFile = CreateFileA(dmp_path.c_str(), GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
        if (hFile != INVALID_HANDLE_VALUE) {
          MINIDUMP_EXCEPTION_INFORMATION mdei;
          mdei.ThreadId = GetCurrentThreadId();
          mdei.ExceptionPointers = p;
          mdei.ClientPointers = FALSE;

          BOOL dumped = pMiniDumpWriteDump(GetCurrentProcess(), GetCurrentProcessId(), hFile, MiniDumpNormal, &mdei, NULL, NULL);
          CloseHandle(hFile);

          if (dumped) {
            g_diag.minidump_path = dmp_path;
            REXLOG_ERROR("[CRASH-DUMP]   MiniDump successfully written to: {}", dmp_path);
          }
        }
      }
    }

    // Write final summary
    WriteDiagnosticSummaryFiles(true, root_explanation);
    REXLOG_ERROR("================================================================================");
  }

  return EXCEPTION_CONTINUE_SEARCH;
}

inline void InstallCrashSentinel() {
  static bool installed = false;
  if (!installed) {
    installed = true;
    AddVectoredExceptionHandler(1, VectoredCrashHandler);
  }
}

// ---------------------------------------------------------------------------
// 5. Final Diagnostic Summary & JSON Export (For WoS_Launcher.exe)
// ---------------------------------------------------------------------------
inline void WriteDiagnosticSummaryFiles(bool is_crash, const std::string& custom_reason) {
  auto now = std::chrono::steady_clock::now();
  g_diag.session_duration_s = std::chrono::duration<double>(now - g_boot_time).count();
  g_diag.peak_ram_mb = g_peak_ram_mb.load(std::memory_order_relaxed);
  g_diag.last_ram_mb = g_last_ram_mb.load(std::memory_order_relaxed);
  g_diag.stutter_spikes_count = g_stutter_spikes.load(std::memory_order_relaxed);

  if (is_crash) {
    g_diag.status = "CRASHED";
    if (!custom_reason.empty()) g_diag.root_cause = custom_reason;
  } else if (!g_diag.rov_supported) {
    g_diag.status = "WARNING_NO_ROV";
  } else {
    g_diag.status = "HEALTHY";
  }

  // A. Write ASCII Box to Logger
  REXLOG_INFO("================================================================================");
  REXLOG_INFO("                   WoS RECOMP ENGINE HEALTH SUMMARY");
  REXLOG_INFO("================================================================================");
  REXLOG_INFO("  [STATUS]       {}", g_diag.status);
  REXLOG_INFO("  [ROOT CAUSE]   {}", g_diag.root_cause);
  REXLOG_INFO("  [HARDWARE]     {} (Dedicated VRAM: {} MB)", g_diag.gpu_name, g_diag.dedicated_vram_mb);
  REXLOG_INFO("  [ROV SUPPORT]  {}", g_diag.rov_supported ? "YES (Hardware Verified)" : "NO (Incompatible GPU!)");
  REXLOG_INFO("  [D3D12 FL]     {}", g_diag.max_feature_level);
  REXLOG_INFO("  [OS LAYER]     {}", g_diag.os_info);
  REXLOG_INFO("  [DURATION]     {:.1f} minutes ({:.0f} seconds)", g_diag.session_duration_s / 60.0, g_diag.session_duration_s);
  REXLOG_INFO("  [MEMORY]       Peak RAM: {:.1f} MB | Final RAM: {:.1f} MB", g_diag.peak_ram_mb, g_diag.last_ram_mb);
  REXLOG_INFO("  [PERFORMANCE]  Total Frame Spikes (>45ms): {:d}", g_diag.stutter_spikes_count);
  if (is_crash && !g_diag.minidump_path.empty()) {
    REXLOG_INFO("  [CRASH DUMP]   {}", g_diag.minidump_path);
  }
  REXLOG_INFO("================================================================================");

  // B. Export JSON for WoS_Launcher.exe Stats & Diagnostic View
  std::error_code ec;
  std::filesystem::create_directories("logs", ec);
  std::ofstream jf("logs/diagnostic_summary.json");
  if (jf.is_open()) {
    jf << "{\n"
       << "  \"timestamp\": \"" << g_diag.timestamp << "\",\n"
       << "  \"status\": \"" << g_diag.status << "\",\n"
       << "  \"root_cause\": \"" << g_diag.root_cause << "\",\n"
       << "  \"recommendation\": \"" << g_diag.recommendation << "\",\n"
       << "  \"gpu_name\": \"" << g_diag.gpu_name << "\",\n"
       << "  \"dedicated_vram_mb\": " << g_diag.dedicated_vram_mb << ",\n"
       << "  \"shared_sys_ram_mb\": " << g_diag.shared_sys_ram_mb << ",\n"
       << "  \"rov_supported\": " << (g_diag.rov_supported ? "true" : "false") << ",\n"
       << "  \"max_feature_level\": \"" << g_diag.max_feature_level << "\",\n"
       << "  \"os_info\": \"" << g_diag.os_info << "\",\n"
       << "  \"is_wine_proton\": " << (g_diag.is_wine_proton ? "true" : "false") << ",\n"
       << "  \"wine_version\": \"" << g_diag.wine_version << "\",\n"
       << "  \"xex_exists\": " << (g_diag.xex_exists ? "true" : "false") << ",\n"
       << "  \"xex_size_bytes\": " << g_diag.xex_size_bytes << ",\n"
       << "  \"session_duration_s\": " << static_cast<uint64_t>(g_diag.session_duration_s) << ",\n"
       << "  \"peak_ram_mb\": " << std::format("{:.1f}", g_diag.peak_ram_mb) << ",\n"
       << "  \"last_ram_mb\": " << std::format("{:.1f}", g_diag.last_ram_mb) << ",\n"
       << "  \"stutter_spikes_count\": " << g_diag.stutter_spikes_count << ",\n"
       << "  \"crashed\": " << (is_crash ? "true" : "false") << ",\n"
       << "  \"minidump_path\": \"" << g_diag.minidump_path << "\"\n"
       << "}\n";
  }
}

} // namespace wos::diag
