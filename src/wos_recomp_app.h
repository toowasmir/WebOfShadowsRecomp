// wos_recomp - ReXGlue Recompiled Project
//
// Spider-Man: Web of Shadows (Xbox 360 -> Win64 Static Recompilation)

#pragma once

#include <filesystem>
#include <rex/logging.h>
#include <rex/rex_app.h>
#include <rex/system/xthread.h>

#include "stubs/unregistered_func_handler.h"
#include "hooks/indirect_call_hooks.h"
#include "hooks/perf_monitor.h"
#include "hooks/diagnostic_sentinel.h"

#include <chrono>
#include <fstream>
#include <thread>
#include <rex/input/device_assignment.h>
#include <rex/input/input_system.h>
#include <rex/input/mnk/mnk_input_driver.h>
#include <rex/input/sdl/sdl_input_driver.h>
#include <rex/input/xinput/xinput_input_driver.h>
#include <rex/ui/presenter.h>
#include <rex/ui/window.h>

using rex::X_STATUS;
using rex::X_RESULT;

class WosRecompApp : public rex::ReXApp {
 public:
  using rex::ReXApp::ReXApp;

  static std::unique_ptr<rex::ui::WindowedApp> Create(
      rex::ui::WindowedAppContext& ctx) {
    return std::unique_ptr<WosRecompApp>(
        new WosRecompApp(ctx, "wos_recomp", PPCImageConfig));
  }

 protected:
  void OnConfigurePaths(rex::PathConfig& paths) override {
    if (paths.game_data_root.empty()) {
      if (std::filesystem::is_directory("game")) {
        paths.game_data_root = std::filesystem::canonical("game");
      } else if (std::filesystem::is_directory("../../game")) {
        paths.game_data_root = std::filesystem::canonical("../../game");
      } else if (std::filesystem::is_directory("../game")) {
        paths.game_data_root = std::filesystem::canonical("../game");
      } else {
        std::filesystem::path repo_game =
            "C:/Users/msbas/OneDrive/Desktop/reComp/game";
        if (std::filesystem::is_directory(repo_game)) {
          paths.game_data_root = repo_game;
        }
      }
    }
  }

class AutoSkipDriver final : public rex::input::InputDriver {
 public:
  explicit AutoSkipDriver(rex::ui::Window* window)
      : rex::input::InputDriver(window, 3) {}
  X_STATUS Setup() override { return X_STATUS_SUCCESS; }
  void EnumerateDevices(std::vector<rex::input::DeviceInfo>& out) override {
    rex::input::DeviceInfo dev;
    dev.id = static_cast<rex::input::DeviceId>(99);
    dev.name = "AutoSkip";
    dev.synthetic = true;
    out.push_back(dev);
  }
  rex::X_RESULT GetDeviceCapabilities(rex::input::DeviceId id, uint32_t flags,
                                      rex::input::X_INPUT_CAPABILITIES* out_caps) override {
    if (out_caps) {
      out_caps->type = 1;
      out_caps->sub_type = 1;
      out_caps->flags = 0;
      out_caps->gamepad.buttons = 0xFFFF;
    }
    return X_ERROR_SUCCESS;
  }
  rex::X_RESULT GetDeviceState(rex::input::DeviceId id, rex::input::X_INPUT_STATE* out_state) override {
    if (!out_state) return X_ERROR_BAD_ARGUMENTS;
    static uint32_t pkt = 0;
    out_state->packet_number = ++pkt;
    static auto start_time = std::chrono::steady_clock::now();
    auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::steady_clock::now() - start_time).count();
    // Pulse 'A' only during opening publisher logos (3.5s - 7.5s).
    // Never press START (which pauses cutscenes and freezes videos).
    if (elapsed > 3500 && elapsed < 7500 && ((elapsed / 400) % 2 == 1)) {
      out_state->gamepad.buttons = 0x1000; // A button only
    } else {
      out_state->gamepad.buttons = 0;
    }
    return X_ERROR_SUCCESS;
  }
  rex::X_RESULT SetDeviceVibration(rex::input::DeviceId id, rex::input::X_INPUT_VIBRATION* v) override {
    return X_ERROR_SUCCESS;
  }
  rex::X_RESULT GetDeviceKeystroke(rex::input::DeviceId id, uint32_t flags,
                                   rex::input::X_INPUT_KEYSTROKE* out_k) override {
    return X_ERROR_EMPTY;
  }
};

  void OnPreSetup(rex::RuntimeConfig& config) override {
    wos::diag::ProbeHardwareAndEnvironment();
    wos::diag::InstallCrashSentinel();
    atexit([] { wos::diag::WriteDiagnosticSummaryFiles(false); });
    if (config.gpu_plugin.empty()) {
      config.gpu_plugin = "xenos";
    }
    HMODULE gpu_mod = LoadLibraryA("rexgpu-xenos.dll");
    bool rov_set = rex::cvar::SetFlagByName("render_target_path_d3d12", "rov");
    bool fetch_set = rex::cvar::SetFlagByName("gpu_allow_invalid_fetch_constants", "true");
    bool d3d12_rb_set = rex::cvar::SetFlagByName("d3d12_readback_resolve", "false");
    bool rb_set = rex::cvar::SetFlagByName("readback_resolve", "none");
    bool clr_set = rex::cvar::SetFlagByName("clear_memory_page_state", "false");
    rex::cvar::SetFlagByName("readback_memexport", "false");
    rex::cvar::SetFlagByName("d3d12_readback_memexport", "false");
    rex::cvar::SetFlagByName("fullscreen", "false");
    rex::cvar::SetFlagByName("mnk_mode", "true");
    rex::cvar::SetFlagByName("mnk_mouse", "true");
    rex::cvar::SetFlagByName("vsync", "false");

    // Boost process priority for smooth emulator & D3D12 worker thread scheduling:
    SetPriorityClass(GetCurrentProcess(), HIGH_PRIORITY_CLASS);

    // Boost Windows timer resolution to 1ms to eliminate scheduler latency jitter:
    HMODULE hWinmm = LoadLibraryA("winmm.dll");
    if (hWinmm) {
      typedef UINT (WINAPI *timeBeginPeriodFn)(UINT);
      auto pTimeBeginPeriod = (timeBeginPeriodFn)GetProcAddress(hWinmm, "timeBeginPeriod");
      if (pTimeBeginPeriod) pTimeBeginPeriod(1);
    }

    // Disable Windows 10/11 EcoQoS / power throttling so all CPU threads run at full frequency:
    HMODULE hKernel32 = GetModuleHandleA("kernel32.dll");
    if (hKernel32) {
      typedef BOOL (WINAPI *SetProcessInformationFn)(HANDLE, int, LPVOID, DWORD);
      auto pSetProcessInfo = (SetProcessInformationFn)GetProcAddress(hKernel32, "SetProcessInformation");
      if (pSetProcessInfo) {
        struct { ULONG Version; ULONG ControlMask; ULONG StateMask; } throttleState{1, 0x1, 0};
        pSetProcessInfo(GetCurrentProcess(), 4 /* ProcessPowerThrottling */, &throttleState, sizeof(throttleState));
      }
    }

    // 1. Hardware Occlusion Queries - ENABLED for Manhattan City Culling:
    // Allows the engine to cull hidden city blocks, props, and NPCs behind skyscrapers.
    rex::cvar::SetFlagByName("occlusion_query_enable", "true");

    // 2. Keep Vertex Shader execution on the GPU (do not burden the 4-core CPU with scissored VS emulation):
    rex::cvar::SetFlagByName("execute_unclipped_draw_vs_on_cpu", "false");
    rex::cvar::SetFlagByName("execute_unclipped_draw_vs_on_cpu_with_scissor", "false");

    // 3. Primitive processor cache - avoid excessive lookups on tiny particle batches:
    rex::cvar::SetFlagByName("primitive_processor_cache_min_indices", "4096");

    // 4. Submit primary buffers immediately to keep GPU pipeline full and minimize frame latency:
    rex::cvar::SetFlagByName("d3d12_submit_on_primary_buffer_end", "true");
    rex::cvar::SetFlagByName("d3d12_allow_variable_refresh_rate_and_tearing", "true");

    // 5. Disable ROV 2x MSAA to cut pixel shader interlocks and fillrate pressure in half:
    rex::cvar::SetFlagByName("native_2x_msaa", "false");

    // 6. Fast alpha test comparisons for ROV pixel shaders (drastically improves web ribbons & combat particles):
    rex::cvar::SetFlagByName("use_fuzzy_alpha_epsilon", "true");

    // 7. Reduce texture fetch bandwidth on GTX 1650 Ti (disable forced 8x anisotropic override):
    rex::cvar::SetFlagByName("anisotropic_override", "0");

    // 8. Optimized VRAM texture cache for 4GB GPU:
    // Prevents VRAM exhaustion and PCIe paging spikes during Manhattan web-swinging traversal:
    rex::cvar::SetFlagByName("texture_cache_memory_limit_soft", "768");
    rex::cvar::SetFlagByName("texture_cache_memory_limit_hard", "1280");
    rex::cvar::SetFlagByName("texture_cache_memory_limit_render_to_texture", "64");
    rex::cvar::SetFlagByName("texture_cache_memory_limit_soft_lifetime", "15");

    // 9. High D3D12 command queue scheduling priority and async pipeline creation:
    rex::cvar::SetFlagByName("d3d12_queue_priority", "2");
    rex::cvar::SetFlagByName("d3d12_pipeline_creation_threads", "-1");
    rex::cvar::SetFlagByName("async_shader_compilation", "true");

    // 10. Load user configuration (recomp_config.json) for AMD FidelityFX (FSR 1/2/3 + CAS),
    // Built-in 2x Frame Generation, Resolution, Fullscreen, V-Sync, VRAM Profile, and Keybindings:
    wos::fg::ApplyConfigToCvars();


    REXLOG_INFO("[WoS] OnPreSetup: gpu_mod={}, d3d12_rb={}, rb_set={}, clr={}",
                (void*)gpu_mod, d3d12_rb_set, rb_set, clr_set);
  }

  void OnLoadXexImage(std::string& xex_image) override {
    REXLOG_INFO("[WoS] OnLoadXexImage: loading '{}'", xex_image);
  }

  void OnPostLoadXexImage() override {
    REXLOG_INFO("[WoS] OnPostLoadXexImage: XEX mapped into 4GB guest arena");
    wos::stubs::InstallUnregisteredFuncFallbacks(runtime());
    wos::hooks::InstallResolveIndirectHook(runtime());
  }

  void OnPostSetup() override {
    REXLOG_INFO("[WoS] OnPostSetup: Runtime and subsystems ready");
    wos::diag::LogEffectiveEngineState();
    for (const auto& flag : rex::cvar::ListFlags()) {
      const auto* info = rex::cvar::GetFlagInfo(flag);
      if (info) {
        REXLOG_INFO("[WoS-CVAR] {} = '{}' (cat: {}, default: '{}')",
                    flag, info->getter ? info->getter() : "", info->category, info->default_value);
      }
    }
    auto* input = dynamic_cast<rex::input::InputSystem*>(runtime()->input_system());
    if (input) {
      if (window()) {
        auto mnk = std::make_unique<rex::input::mnk::MnkInputDriver>(window(), 1);
        mnk->Setup();
        input->AddDriver(std::move(mnk));

        auto xinput = std::make_unique<rex::input::xinput::XinputInputDriver>(window(), 2);
        xinput->Setup();
        input->AddDriver(std::move(xinput));

        auto sdl = std::make_unique<rex::input::sdl::SDLInputDriver>(window(), 3);
        sdl->Setup();
        input->AddDriver(std::move(sdl));

        char skip_val[8] = {0};
        bool auto_skip_enabled = (GetEnvironmentVariableA("WOS_AUTOSKIP", skip_val, sizeof(skip_val)) > 0 && skip_val[0] == '1');
        if (auto_skip_enabled) {
          auto autoskip = std::make_unique<AutoSkipDriver>(window());
          autoskip->Setup();
          input->AddDriver(std::move(autoskip));
          REXLOG_INFO("[WoS] Added AutoSkipDriver (test mode: WOS_AUTOSKIP=1)");
        } else {
          REXLOG_INFO("[WoS] User play mode: MnkInputDriver, XinputInputDriver, SDLInputDriver active");
        }
        wos::perf::StartTelemetryThread(window());
      }
      input->SetDeviceAssignment(std::make_unique<rex::input::SharedAssignment>());
      REXLOG_INFO("[WoS] Applied SharedAssignment (all devices -> user 0)");
    }
  }

  void OnPreLaunchModule() override {
    REXLOG_INFO("[WoS] OnPreLaunchModule: Preparing main guest thread...");
  }

  void OnPostLaunchModule(rex::system::XThread* thread) override {
    REXLOG_INFO("[WoS] OnPostLaunchModule: Main guest thread created (handle={:08X}, thid={})",
                thread ? thread->handle() : 0, thread ? thread->thread_id() : 0);
    char cap_val[8] = {0};
    if (GetEnvironmentVariableA("WOS_CAPTURE_TEST", cap_val, sizeof(cap_val)) > 0 && cap_val[0] == '1') {
      StartFrameCaptureMonitor();
    }
  }

 private:
  static bool SafeScanRam(const uint8_t* p, size_t size, uint64_t& nonzero_out, uint8_t& max_out) {
    nonzero_out = 0;
    max_out = 0;
    __try {
      for (size_t i = 0; i < size; ++i) {
        if (p[i] != 0) {
          nonzero_out++;
          if (p[i] > max_out) max_out = p[i];
        }
      }
      return true;
    } __except (EXCEPTION_EXECUTE_HANDLER) {
      return false;
    }
  }

  void StartFrameCaptureMonitor() {
    std::thread([this]() {
      REXLOG_INFO("[WoS-CAPTURE] Frame capture monitor thread started");
      struct WindowAccess : public rex::ui::Window {
        static rex::ui::Presenter* GetPresenter(rex::ui::Window* w) {
          return static_cast<WindowAccess*>(w)->presenter();
        }
      };
      int dumps_saved = 0;
      for (int sec = 1; sec <= 60; ++sec) {
        std::this_thread::sleep_for(std::chrono::seconds(1));
        if (!window()) continue;
        rex::ui::Presenter* pres = WindowAccess::GetPresenter(window());
        if (!pres) continue;

        rex::ui::RawImage img;
        if (pres->CaptureGuestOutput(img) && !img.data.empty()) {
          uint64_t non_black = 0;
          uint8_t max_r = 0, max_g = 0, max_b = 0;
          for (size_t y = 0; y < img.height; ++y) {
            const uint8_t* row = img.data.data() + y * img.stride;
            for (size_t x = 0; x < img.width; ++x) {
              uint8_t r = row[x * 4 + 0];
              uint8_t g = row[x * 4 + 1];
              uint8_t b = row[x * 4 + 2];
              if (r > 16 || g > 16 || b > 16) {
                non_black++;
                if (r > max_r) max_r = r;
                if (g > max_g) max_g = g;
                if (b > max_b) max_b = b;
              }
            }
          }

          // Check guest RAM at candidate frontbuffer addresses
          uint64_t ram_1df60 = 0, ram_1df61 = 0, ram_fdf60_virt = 0;
          uint8_t max_1df60 = 0, max_1df61 = 0, max_fdf60_virt = 0;
          uint64_t bink_nonzero_A = 0, bink_nonzero_B = 0;
          uint8_t bink_max_A = 0, bink_max_B = 0;
          if (runtime() && runtime()->memory()) {
            const uint8_t* p0 = runtime()->memory()->TranslatePhysical(0x1DF60000);
            SafeScanRam(p0, 1280 * 720 * 4, ram_1df60, max_1df60);

            const uint8_t* p1 = runtime()->memory()->TranslatePhysical(0x1DF61000);
            SafeScanRam(p1, 1280 * 720 * 4, ram_1df61, max_1df61);

            const uint8_t* pv = runtime()->memory()->TranslateVirtual(0xFDF60000);
            SafeScanRam(pv, 1280 * 720 * 4, ram_fdf60_virt, max_fdf60_virt);

            const uint8_t* binkA = runtime()->memory()->TranslatePhysical(0x07861000);
            SafeScanRam(binkA, 1280 * 720, bink_nonzero_A, bink_max_A);

            const uint8_t* binkB = runtime()->memory()->TranslatePhysical(0x07A11000);
            SafeScanRam(binkB, 1280 * 720, bink_nonzero_B, bink_max_B);
          }

          REXLOG_INFO("[WoS-CAPTURE] Sec {:02d}: presented non-black: {}/{} | Phys@1DF60: {} (max={}), Phys@1DF61: {} (max={}), Virt@FDF60: {} (max={}) | BinkY_A: {} (max={})",
                      sec, non_black, img.width * img.height,
                      ram_1df60, max_1df60,
                      ram_1df61, max_1df61,
                      ram_fdf60_virt, max_fdf60_virt,
                      bink_nonzero_A, bink_max_A);

          static bool bink_dumped = false;
          if (!bink_dumped && bink_max_A > 200 && runtime() && runtime()->memory()) {
            const uint8_t* binkA = runtime()->memory()->TranslatePhysical(0x07861000);
            std::ofstream f("out/build/win-amd64-release/bink_y_dump.pgm", std::ios::binary);
            if (f) {
              f << "P5\n1280 720\n255\n";
              f.write(reinterpret_cast<const char*>(binkA), 1280 * 720);
              bink_dumped = true;
              REXLOG_INFO("[WoS-CAPTURE] Saved Bink Y texture to out/build/win-amd64-release/bink_y_dump.pgm!");
            }
          }

          if ((non_black > (img.width * img.height / 100)) || (sec >= 13 && sec <= 25) || (sec % 10 == 0)) {
            std::string filename = "out/build/win-amd64-release/frame_" + std::to_string(sec) + "s.ppm";
            std::ofstream f(filename, std::ios::binary);
            if (f) {
              f << "P6\n" << img.width << " " << img.height << "\n255\n";
              for (size_t y = 0; y < img.height; ++y) {
                const uint8_t* row = img.data.data() + y * img.stride;
                for (size_t x = 0; x < img.width; ++x) {
                  f.put(row[x * 4 + 0]);
                  f.put(row[x * 4 + 1]);
                  f.put(row[x * 4 + 2]);
                }
              }
              dumps_saved++;
              REXLOG_INFO("[WoS-CAPTURE] Saved frame dump to '{}'!", filename);
            }
          }
        } else {
          REXLOG_INFO("[WoS-CAPTURE] Sec {:02d}: CaptureGuestOutput returned false or empty", sec);
        }
      }
      REXLOG_INFO("[WoS-CAPTURE] Frame capture monitor thread finished");
    }).detach();
  }
};
