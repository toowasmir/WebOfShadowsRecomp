// wos_recomp - Indirect Call Protection & Null Pointer Interceptor
//
// Spider-Man: Web of Shadows (Xbox 360 -> Win64 Static Recompilation)
//
// Intercepts rex::runtime::ResolveIndirectFunction via IAT to safely catch
// null function pointers (0x00000000) and unmapped targets without crashing or aborting.

#pragma once

#include <windows.h>
#include <rex/logging.h>
#include <rex/ppc.h>
#include <rex/runtime.h>
#include <rex/system/function_dispatcher.h>

#include "perf_monitor.h"

namespace wos::hooks {

using ResolveIndirectFn = PPCFunc* (*)(uint32_t);
inline ResolveIndirectFn g_orig_resolve = nullptr;
inline PPCFunc* g_orig_vdswap = nullptr;
inline rex::Runtime* g_runtime = nullptr;

inline void SafeNullFunc(PPCContext& ctx, uint8_t* base) {
  REXLOG_WARN("[WoS-NULL] Safe null indirect call intercepted! Target=0x{:08X}, LR=0x{:08X}, r3=0x{:08X}",
              ctx.last_indirect_target, static_cast<uint32_t>(ctx.lr), ctx.r3.u32);
  ctx.r3.u64 = 0;
}

inline void HookedVdSwap(PPCContext& ctx, uint8_t* base) {
  wos::perf::RecordFrame();
  if (g_orig_vdswap) {
    g_orig_vdswap(ctx, base);
  }
}

inline PPCFunc* HookedResolveIndirectFunction(uint32_t guest_addr) {
  if (guest_addr == 0 || guest_addr < 0x10000 || guest_addr >= 0xFF000000) {
    REXLOG_WARN("[WoS-RESOLVE] Intercepted invalid target 0x{:08X}! Returning safe no-op", guest_addr);
    return &SafeNullFunc;
  }
  return g_orig_resolve ? g_orig_resolve(guest_addr) : &SafeNullFunc;
}

inline bool InstallResolveIndirectHook(rex::Runtime* runtime) {
  g_runtime = runtime;
  HMODULE hMod = GetModuleHandleA(NULL);
  if (!hMod) return false;

  auto* dos = (PIMAGE_DOS_HEADER)hMod;
  if (dos->e_magic != IMAGE_DOS_SIGNATURE) return false;
  auto* nt = (PIMAGE_NT_HEADERS)((uint8_t*)hMod + dos->e_lfanew);
  if (nt->Signature != IMAGE_NT_SIGNATURE) return false;

  DWORD importRva = nt->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_IMPORT].VirtualAddress;
  if (!importRva) return false;

  bool hooked_resolve = false;
  bool hooked_vdswap = false;

  auto* importDesc = (PIMAGE_IMPORT_DESCRIPTOR)((uint8_t*)hMod + importRva);
  for (; importDesc->Name; importDesc++) {
    const char* dllName = (const char*)hMod + importDesc->Name;
    if (_stricmp(dllName, "rexruntime.dll") == 0) {
      auto* thunk = (PIMAGE_THUNK_DATA)((uint8_t*)hMod + importDesc->FirstThunk);
      auto* origThunk = (PIMAGE_THUNK_DATA)((uint8_t*)hMod + (importDesc->OriginalFirstThunk ? importDesc->OriginalFirstThunk : importDesc->FirstThunk));
      for (; origThunk->u1.Function; thunk++, origThunk++) {
        if (IMAGE_SNAP_BY_ORDINAL(origThunk->u1.Ordinal)) continue;
        auto* importByName = (PIMAGE_IMPORT_BY_NAME)((uint8_t*)hMod + origThunk->u1.AddressOfData);
        if (strstr(importByName->Name, "ResolveIndirectFunction")) {
          DWORD oldProtect;
          VirtualProtect(&thunk->u1.Function, sizeof(void*), PAGE_READWRITE, &oldProtect);
          g_orig_resolve = (ResolveIndirectFn)thunk->u1.Function;
          thunk->u1.Function = (uintptr_t)&HookedResolveIndirectFunction;
          VirtualProtect(&thunk->u1.Function, sizeof(void*), oldProtect, &oldProtect);
          REXLOG_INFO("[WoS] Hooked IAT ResolveIndirectFunction: orig={:p}, new={:p}",
                      (void*)g_orig_resolve, (void*)&HookedResolveIndirectFunction);
          hooked_resolve = true;
        } else if (strstr(importByName->Name, "VdSwap")) {
          DWORD oldProtect;
          VirtualProtect(&thunk->u1.Function, sizeof(void*), PAGE_READWRITE, &oldProtect);
          g_orig_vdswap = (PPCFunc*)thunk->u1.Function;
          thunk->u1.Function = (uintptr_t)&HookedVdSwap;
          VirtualProtect(&thunk->u1.Function, sizeof(void*), oldProtect, &oldProtect);
          REXLOG_INFO("[WoS] Hooked IAT VdSwap: orig={:p}, new={:p}",
                      (void*)g_orig_vdswap, (void*)&HookedVdSwap);
          hooked_vdswap = true;
        }
      }
    }
  }
  return hooked_resolve;
}

}  // namespace wos::hooks
