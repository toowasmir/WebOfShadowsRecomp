// Safe replacement hook for sub_82A1ACF0 (hash table lookup)
// Wrapped in SEH (__try / __except) to guarantee zero crashes on dangling or uncommitted memory pages.

#pragma once

#include <windows.h>
#include <rex/ppc.h>
#include <rex/logging.h>
#include <rex/runtime.h>
#include <rex/system/xmemory.h>

#include "indirect_call_hooks.h"

extern "C" void __imp__RtlEnterCriticalSection(PPCContext& __restrict, uint8_t*);
extern "C" void __imp__RtlLeaveCriticalSection(PPCContext& __restrict, uint8_t*);

#define REX_LOAD_U32_SAFE(addr) __builtin_bswap32(*(volatile uint32_t*)(base + (uint32_t)(addr) + (((uint32_t)(addr) >= 0xE0000000u) ? 0x1000u : 0u)))
#define REX_STORE_U32_SAFE(addr, val) (*(volatile uint32_t*)(base + (uint32_t)(addr) + (((uint32_t)(addr) >= 0xE0000000u) ? 0x1000u : 0u)) = __builtin_bswap32(val))

// Ultra-fast inline sanity check for guest addresses (1 CPU instruction):
inline bool IsFastLikelyValidGuestAddr(uint32_t addr) noexcept {
  return (addr >= 0x00010000u && addr < 0xF0000000u && (addr & 3u) == 0u);
}

extern "C" void sub_82A1ACF0(PPCContext& __restrict ctx, uint8_t* base) {
  uint32_t table = ctx.r3.u32;
  uint32_t key = ctx.r4.u32;

  if (!IsFastLikelyValidGuestAddr(table)) {
    ctx.r3.u64 = 0;
    return;
  }

  // Key modulo 521 matching the game's assembly
  uint32_t bucket_idx = key % 521;
  uint32_t bucket_offset = (bucket_idx + 7) * 4;

  bool cs_entered = false;

  __try {
    // Enter critical section
    ctx.r3.u64 = table;
    __imp__RtlEnterCriticalSection(ctx, base);
    cs_entered = true;

    uint32_t node = REX_LOAD_U32_SAFE(table + bucket_offset);
    int loop_guard = 0;

    while (IsFastLikelyValidGuestAddr(node) && ++loop_guard < 1000) {
      uint32_t node_key = REX_LOAD_U32_SAFE(node + 12);
      if (node_key == key) {
        uint32_t ref = REX_LOAD_U32_SAFE(node + 4) + 1;
        REX_STORE_U32_SAFE(node + 4, ref);

        ctx.r3.u64 = table;
        __imp__RtlLeaveCriticalSection(ctx, base);
        cs_entered = false;
        ctx.r3.u64 = node;
        return;
      }
      node = REX_LOAD_U32_SAFE(node + 8); // next pointer
    }
  } __except (EXCEPTION_EXECUTE_HANDLER) {
    // Gracefully catch any unmapped / uncommitted guest memory page faults
  }

  // Not found or hit invalid/faulting pointer
  if (cs_entered) {
    __try {
      ctx.r3.u64 = table;
      __imp__RtlLeaveCriticalSection(ctx, base);
    } __except (EXCEPTION_EXECUTE_HANDLER) {}
  }
  ctx.r3.u64 = 0;
}

