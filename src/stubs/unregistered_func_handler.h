// wos_recomp - Unregistered Guest Function Fallback & Leaf Interpreter
//
// Spider-Man: Web of Shadows (Xbox 360 -> Win64 Static Recompilation)
//
// Xbox 360 .pdata omits leaf functions that do not allocate stack frames.
// When such functions are only invoked indirectly (via vtables or callbacks),
// static call-graph analysis cannot see their entry points until runtime.
//
// This module hooks all unmapped 4-byte aligned slots in the code section so
// indirect calls to unregistered functions do not crash with [FATAL]. Instead,
// it logs the discovered function boundary and interprets the PowerPC instructions
// (or tail-calls into recompiled code) seamlessly.

#pragma once

#include <bit>
#include <cstdint>
#include <mutex>
#include <unordered_set>

#include <rex/logging.h>
#include <rex/ppc/context.h>
#include <rex/runtime.h>
#include <rex/system/function_dispatcher.h>
#include <rex/system/xmemory.h>

#include "generated/default/wos_recomp_init.h"
#include "hooks/perf_monitor.h"

namespace wos::stubs {

inline rex::Runtime* g_runtime = nullptr;
inline std::mutex g_disc_mutex;
inline std::unordered_set<uint32_t> g_discovered_funcs;

inline PPCRegister& GetGPR(PPCContext& ctx, uint32_t reg) noexcept {
  switch (reg & 31) {
    case 0: return ctx.r0;
    case 1: return ctx.r1;
    case 2: return ctx.r2;
    case 3: return ctx.r3;
    case 4: return ctx.r4;
    case 5: return ctx.r5;
    case 6: return ctx.r6;
    case 7: return ctx.r7;
    case 8: return ctx.r8;
    case 9: return ctx.r9;
    case 10: return ctx.r10;
    case 11: return ctx.r11;
    case 12: return ctx.r12;
    case 13: return ctx.r13;
    case 14: return ctx.r14;
    case 15: return ctx.r15;
    case 16: return ctx.r16;
    case 17: return ctx.r17;
    case 18: return ctx.r18;
    case 19: return ctx.r19;
    case 20: return ctx.r20;
    case 21: return ctx.r21;
    case 22: return ctx.r22;
    case 23: return ctx.r23;
    case 24: return ctx.r24;
    case 25: return ctx.r25;
    case 26: return ctx.r26;
    case 27: return ctx.r27;
    case 28: return ctx.r28;
    case 29: return ctx.r29;
    case 30: return ctx.r30;
    default: return ctx.r31;
  }
}

inline PPCRegister& GetFPR(PPCContext& ctx, uint32_t reg) noexcept {
  return (&ctx.f0)[reg & 31];
}

inline PPCCRRegister& GetCR(PPCContext& ctx, uint32_t field) noexcept {
  return (&ctx.cr0)[field & 7];
}

inline bool CheckCRBit(PPCContext& ctx, uint32_t bi) noexcept {
  const PPCCRRegister& cr = GetCR(ctx, bi >> 2);
  switch (bi & 3) {
    case 0: return cr.lt != 0;
    case 1: return cr.gt != 0;
    case 2: return cr.eq != 0;
    default: return cr.so != 0;
  }
}

inline uint8_t* GuestAddr(uint8_t* base, uint32_t addr) noexcept {
  return base + addr + ((addr >= 0xE0000000u) ? 0x1000u : 0u);
}

inline uint32_t LoadU32BE(uint8_t* base, uint32_t addr) noexcept {
  return __builtin_bswap32(*reinterpret_cast<const uint32_t*>(GuestAddr(base, addr)));
}

inline uint16_t LoadU16BE(uint8_t* base, uint32_t addr) noexcept {
  return __builtin_bswap16(*reinterpret_cast<const uint16_t*>(GuestAddr(base, addr)));
}

inline uint8_t LoadU8(uint8_t* base, uint32_t addr) noexcept {
  return *GuestAddr(base, addr);
}

inline uint64_t LoadU64BE(uint8_t* base, uint32_t addr) noexcept {
  return __builtin_bswap64(*reinterpret_cast<const uint64_t*>(GuestAddr(base, addr)));
}

inline void StoreU32BE(uint8_t* base, uint32_t addr, uint32_t val) noexcept {
  *reinterpret_cast<uint32_t*>(GuestAddr(base, addr)) = __builtin_bswap32(val);
}

inline void StoreU16BE(uint8_t* base, uint32_t addr, uint16_t val) noexcept {
  *reinterpret_cast<uint16_t*>(GuestAddr(base, addr)) = __builtin_bswap16(val);
}

inline void StoreU8(uint8_t* base, uint32_t addr, uint8_t val) noexcept {
  *GuestAddr(base, addr) = val;
}

inline void StoreU64BE(uint8_t* base, uint32_t addr, uint64_t val) noexcept {
  *reinterpret_cast<uint64_t*>(GuestAddr(base, addr)) = __builtin_bswap64(val);
}

inline PPCFunc* LookupCompiledOrThunk(uint8_t* base, uint32_t addr) noexcept {
  if (addr >= REX_CODE_BASE && addr < (REX_CODE_BASE + REX_CODE_SIZE)) {
    PPCFunc* fn = *reinterpret_cast<PPCFunc**>(
        base + REX_IMAGE_BASE + REX_IMAGE_SIZE +
        (static_cast<uint64_t>(addr - REX_CODE_BASE) * 2ULL));
    if (fn) return fn;
  }
  if (g_runtime && g_runtime->function_dispatcher()) {
    return g_runtime->function_dispatcher()->GetFunction(addr);
  }
  return nullptr;
}

inline uint32_t ComputeMask32(uint32_t mb, uint32_t me) noexcept {
  uint32_t mask_mb = ~0u >> mb;
  uint32_t mask_me = ~0u << (31 - me);
  return (mb <= me) ? (mask_mb & mask_me) : (mask_mb | mask_me);
}

inline void UnregisteredGuestFuncHandler(PPCContext& ctx, uint8_t* base) {
  uint32_t entry_pc = ctx.last_indirect_target ? ctx.last_indirect_target : ctx.ctr.u32;
  ctx.last_indirect_target = 0;

  if (entry_pc < REX_CODE_BASE || entry_pc >= (REX_CODE_BASE + REX_CODE_SIZE)) {
    REXLOG_WARN("[WoS-Interp] Ignoring out-of-range guest call target 0x{:08X} (LR=0x{:08X})",
                entry_pc, static_cast<uint32_t>(ctx.lr));
    ctx.r3.u64 = 0;
    return;
  }

  {
    std::lock_guard<std::mutex> lock(g_disc_mutex);
    if (g_discovered_funcs.insert(entry_pc).second) {
      uint32_t scan_end = entry_pc;
      for (int i = 0; i < 64; ++i) {
        uint32_t insn = LoadU32BE(base, scan_end);
        scan_end += 4;
        if (insn == 0x4E800020 || insn == 0x4E800420 || ((insn >> 26) == 18 && (insn & 1) == 0)) {
          break;
        }
      }
      REXLOG_WARN(
          "[WoS-Interp] Discovered unregistered guest function at 0x{:08X}..0x{:08X} "
          "(called from LR=0x{:08X}, r3=0x{:08X}). Manifest entry: \"0x{:08X}\" = {{ end = 0x{:08X} }}",
          entry_pc, scan_end, static_cast<uint32_t>(ctx.lr), ctx.r3.u32, entry_pc, scan_end);
      for (uint32_t p = entry_pc; p < scan_end; p += 4) {
        REXLOG_INFO("  [0x{:08X}] insn = 0x{:08X}", p, LoadU32BE(base, p));
      }
    }
  }

  uint32_t pc = entry_pc;
  for (int step = 0; step < 4096; ++step) {
    // If PC entered a compiled function (and it's not our entry or it got compiled), tail-call it!
    if (pc >= REX_CODE_BASE && pc < (REX_CODE_BASE + REX_CODE_SIZE)) {
      PPCFunc* compiled = *reinterpret_cast<PPCFunc**>(
          base + REX_IMAGE_BASE + REX_IMAGE_SIZE +
          (static_cast<uint64_t>(pc - REX_CODE_BASE) * 2ULL));
      if (compiled) {
        compiled(ctx, base);
        return;
      }
    }

    uint32_t insn = LoadU32BE(base, pc);
    uint32_t op = insn >> 26;
    uint32_t rt = (insn >> 21) & 31;
    uint32_t ra = (insn >> 16) & 31;
    uint32_t rb = (insn >> 11) & 31;
    int32_t simm = static_cast<int16_t>(insn & 0xFFFF);
    uint32_t uimm = insn & 0xFFFF;

    switch (op) {
      case 7: {  // mulli
        GetGPR(ctx, rt).s64 = GetGPR(ctx, ra).s64 * simm;
        pc += 4;
        break;
      }
      case 8: {  // subfic
        uint64_t a = GetGPR(ctx, ra).u32;
        uint64_t b = static_cast<uint32_t>(simm);
        uint64_t res = ~a + b + 1ULL;
        ctx.xer.ca = (res >> 32) != 0;
        GetGPR(ctx, rt).s64 = static_cast<int32_t>(res);
        pc += 4;
        break;
      }
      case 10: {  // cmpli
        uint32_t crf = (rt >> 2) & 7;
        uint32_t l = rt & 1;
        if (l) {
          GetCR(ctx, crf).compare<uint64_t>(GetGPR(ctx, ra).u64, uimm, ctx.xer);
        } else {
          GetCR(ctx, crf).compare<uint32_t>(GetGPR(ctx, ra).u32, uimm, ctx.xer);
        }
        pc += 4;
        break;
      }
      case 11: {  // cmpi
        uint32_t crf = (rt >> 2) & 7;
        uint32_t l = rt & 1;
        if (l) {
          GetCR(ctx, crf).compare<int64_t>(GetGPR(ctx, ra).s64, simm, ctx.xer);
        } else {
          GetCR(ctx, crf).compare<int32_t>(GetGPR(ctx, ra).s32, simm, ctx.xer);
        }
        pc += 4;
        break;
      }
      case 12:    // addic
      case 13: {  // addic.
        uint64_t a = GetGPR(ctx, ra).u32;
        uint64_t b = static_cast<uint32_t>(simm);
        uint64_t res = a + b;
        ctx.xer.ca = (res >> 32) != 0;
        GetGPR(ctx, rt).s64 = static_cast<int32_t>(GetGPR(ctx, ra).s64 + simm);
        if (op == 13) ctx.cr0.compare<int32_t>(GetGPR(ctx, rt).s32, 0, ctx.xer);
        pc += 4;
        break;
      }
      case 14: {  // addi / li
        int64_t base_val = (ra == 0) ? 0 : GetGPR(ctx, ra).s64;
        GetGPR(ctx, rt).s64 = base_val + simm;
        pc += 4;
        break;
      }
      case 15: {  // addis / lis
        int64_t base_val = (ra == 0) ? 0 : GetGPR(ctx, ra).s64;
        GetGPR(ctx, rt).s64 = base_val + (static_cast<int64_t>(simm) << 16);
        pc += 4;
        break;
      }
      case 16: {  // bc / bca / bcl / bcla
        uint32_t bo = rt;
        uint32_t bi = ra;
        int32_t bd = static_cast<int16_t>(insn & 0xFFFC);
        bool aa = (insn & 2) != 0;
        bool lk = (insn & 1) != 0;
        if ((bo & 4) == 0) ctx.ctr.u64 -= 1;
        bool ctr_ok = ((bo & 4) != 0) || (((ctx.ctr.u32 != 0) ^ ((bo >> 1) & 1)) != 0);
        bool cond_ok = ((bo & 16) != 0) || (CheckCRBit(ctx, bi) == ((bo & 8) != 0));
        if (lk) ctx.lr = pc + 4;
        if (ctr_ok && cond_ok) {
          pc = aa ? static_cast<uint32_t>(bd) : (pc + bd);
        } else {
          pc += 4;
        }
        break;
      }
      case 18: {  // b / ba / bl / bla
        int32_t li = static_cast<int32_t>(insn & 0x03FFFFFC);
        if (li & 0x02000000) li |= static_cast<int32_t>(0xFC000000);
        bool aa = (insn & 2) != 0;
        bool lk = (insn & 1) != 0;
        uint32_t target = aa ? static_cast<uint32_t>(li) : (pc + li);
        if (lk) {
          ctx.lr = pc + 4;
          PPCFunc* fn = LookupCompiledOrThunk(base, target);
          if (fn) {
            ctx.last_indirect_target = target;
            fn(ctx, base);
          } else {
            ctx.last_indirect_target = target;
            UnregisteredGuestFuncHandler(ctx, base);
          }
          pc += 4;
        } else {
          pc = target;
        }
        break;
      }
      case 19: {  // bclr / bcctr / cr ops
        uint32_t xo = (insn >> 1) & 0x3FF;
        bool lk = (insn & 1) != 0;
        if (xo == 16) {  // bclr / blr
          uint32_t bo = rt;
          uint32_t bi = ra;
          if ((bo & 4) == 0) ctx.ctr.u64 -= 1;
          bool ctr_ok = ((bo & 4) != 0) || (((ctx.ctr.u32 != 0) ^ ((bo >> 1) & 1)) != 0);
          bool cond_ok = ((bo & 16) != 0) || (CheckCRBit(ctx, bi) == ((bo & 8) != 0));
          if (ctr_ok && cond_ok) {
            uint32_t target = static_cast<uint32_t>(ctx.lr) & ~3u;
            if (lk) ctx.lr = pc + 4;
            return;
          }
          pc += 4;
        } else if (xo == 528) {  // bcctr / bctr / bctrl
          uint32_t bo = rt;
          uint32_t bi = ra;
          bool cond_ok = ((bo & 16) != 0) || (CheckCRBit(ctx, bi) == ((bo & 8) != 0));
          if (cond_ok) {
            uint32_t target = ctx.ctr.u32 & ~3u;
            if (lk) {
              ctx.lr = pc + 4;
              PPCFunc* fn = LookupCompiledOrThunk(base, target);
              ctx.last_indirect_target = target;
              if (fn) {
                fn(ctx, base);
              } else {
                UnregisteredGuestFuncHandler(ctx, base);
              }
              pc += 4;
            } else {
              pc = target;
            }
          } else {
            pc += 4;
          }
        } else {
          pc += 4;
        }
        break;
      }
      case 20: {  // rlwimi
        uint32_t sh = rb;
        uint32_t mb = (insn >> 6) & 31;
        uint32_t me = (insn >> 1) & 31;
        uint32_t r = std::rotl(GetGPR(ctx, rt).u32, static_cast<int>(sh));
        uint32_t m = ComputeMask32(mb, me);
        GetGPR(ctx, ra).u64 = (r & m) | (GetGPR(ctx, ra).u32 & ~m);
        if (insn & 1) ctx.cr0.compare<int32_t>(GetGPR(ctx, ra).s32, 0, ctx.xer);
        pc += 4;
        break;
      }
      case 21: {  // rlwinm
        uint32_t sh = rb;
        uint32_t mb = (insn >> 6) & 31;
        uint32_t me = (insn >> 1) & 31;
        uint32_t r = std::rotl(GetGPR(ctx, rt).u32, static_cast<int>(sh));
        uint32_t m = ComputeMask32(mb, me);
        GetGPR(ctx, ra).u64 = r & m;
        if (insn & 1) ctx.cr0.compare<int32_t>(GetGPR(ctx, ra).s32, 0, ctx.xer);
        pc += 4;
        break;
      }
      case 23: {  // rlwnm
        uint32_t sh = GetGPR(ctx, rb).u32 & 31;
        uint32_t mb = (insn >> 6) & 31;
        uint32_t me = (insn >> 1) & 31;
        uint32_t r = std::rotl(GetGPR(ctx, rt).u32, static_cast<int>(sh));
        uint32_t m = ComputeMask32(mb, me);
        GetGPR(ctx, ra).u64 = r & m;
        if (insn & 1) ctx.cr0.compare<int32_t>(GetGPR(ctx, ra).s32, 0, ctx.xer);
        pc += 4;
        break;
      }
      case 24: {  // ori
        GetGPR(ctx, ra).u64 = GetGPR(ctx, rt).u64 | uimm;
        pc += 4;
        break;
      }
      case 25: {  // oris
        GetGPR(ctx, ra).u64 = GetGPR(ctx, rt).u64 | (static_cast<uint64_t>(uimm) << 16);
        pc += 4;
        break;
      }
      case 26: {  // xori
        GetGPR(ctx, ra).u64 = GetGPR(ctx, rt).u64 ^ uimm;
        pc += 4;
        break;
      }
      case 27: {  // xoris
        GetGPR(ctx, ra).u64 = GetGPR(ctx, rt).u64 ^ (static_cast<uint64_t>(uimm) << 16);
        pc += 4;
        break;
      }
      case 28: {  // andi.
        GetGPR(ctx, ra).u64 = GetGPR(ctx, rt).u64 & uimm;
        ctx.cr0.compare<int32_t>(GetGPR(ctx, ra).s32, 0, ctx.xer);
        pc += 4;
        break;
      }
      case 29: {  // andis.
        GetGPR(ctx, ra).u64 = GetGPR(ctx, rt).u64 & (static_cast<uint64_t>(uimm) << 16);
        ctx.cr0.compare<int32_t>(GetGPR(ctx, ra).s32, 0, ctx.xer);
        pc += 4;
        break;
      }
      case 30: {  // rldicl / rldicr / rldic / rldimi
        uint32_t sh = ((insn >> 11) & 31) | (((insn >> 1) & 1) << 5);
        uint32_t mb = ((insn >> 6) & 31) | ((insn >> 5) & 0x20);
        uint32_t subop = (insn >> 2) & 7;
        uint64_t r = std::rotl(GetGPR(ctx, rt).u64, static_cast<int>(sh));
        if (subop == 0) {  // rldicl
          uint64_t m = (~0ULL) >> mb;
          GetGPR(ctx, ra).u64 = r & m;
        } else if (subop == 1) {  // rldicr
          uint64_t m = (~0ULL) << (63 - mb);
          GetGPR(ctx, ra).u64 = r & m;
        } else {
          GetGPR(ctx, ra).u64 = r;
        }
        if (insn & 1) ctx.cr0.compare<int32_t>(GetGPR(ctx, ra).s32, 0, ctx.xer);
        pc += 4;
        break;
      }
      case 31: {  // Extended ALU / X-form loads & stores / SPRs
        uint32_t xo = (insn >> 1) & 0x3FF;
        bool rc = (insn & 1) != 0;
        switch (xo) {
          case 0: {  // cmp
            uint32_t crf = (rt >> 2) & 7;
            if (rt & 1) {
              GetCR(ctx, crf).compare<int64_t>(GetGPR(ctx, ra).s64, GetGPR(ctx, rb).s64, ctx.xer);
            } else {
              GetCR(ctx, crf).compare<int32_t>(GetGPR(ctx, ra).s32, GetGPR(ctx, rb).s32, ctx.xer);
            }
            break;
          }
          case 32: {  // cmpl
            uint32_t crf = (rt >> 2) & 7;
            if (rt & 1) {
              GetCR(ctx, crf).compare<uint64_t>(GetGPR(ctx, ra).u64, GetGPR(ctx, rb).u64, ctx.xer);
            } else {
              GetCR(ctx, crf).compare<uint32_t>(GetGPR(ctx, ra).u32, GetGPR(ctx, rb).u32, ctx.xer);
            }
            break;
          }
          case 8:     // subfc
          case 40: {  // subf
            GetGPR(ctx, rt).s64 = GetGPR(ctx, rb).s64 - GetGPR(ctx, ra).s64;
            if (rc) ctx.cr0.compare<int32_t>(GetGPR(ctx, rt).s32, 0, ctx.xer);
            break;
          }
          case 10:     // addc
          case 266: {  // add
            GetGPR(ctx, rt).s64 = GetGPR(ctx, ra).s64 + GetGPR(ctx, rb).s64;
            if (rc) ctx.cr0.compare<int32_t>(GetGPR(ctx, rt).s32, 0, ctx.xer);
            break;
          }
          case 23: {  // lwzx
            uint32_t ea = (ra ? GetGPR(ctx, ra).u32 : 0) + GetGPR(ctx, rb).u32;
            GetGPR(ctx, rt).u64 = LoadU32BE(base, ea);
            break;
          }
          case 24: {  // slw
            uint32_t sh = GetGPR(ctx, rb).u32 & 63;
            GetGPR(ctx, ra).u64 = (sh < 32) ? (GetGPR(ctx, rt).u32 << sh) : 0;
            if (rc) ctx.cr0.compare<int32_t>(GetGPR(ctx, ra).s32, 0, ctx.xer);
            break;
          }
          case 26: {  // cntlzw
            GetGPR(ctx, ra).u64 = std::countl_zero(GetGPR(ctx, rt).u32);
            if (rc) ctx.cr0.compare<int32_t>(GetGPR(ctx, ra).s32, 0, ctx.xer);
            break;
          }
          case 28: {  // and
            GetGPR(ctx, ra).u64 = GetGPR(ctx, rt).u64 & GetGPR(ctx, rb).u64;
            if (rc) ctx.cr0.compare<int32_t>(GetGPR(ctx, ra).s32, 0, ctx.xer);
            break;
          }
          case 60: {  // andc
            GetGPR(ctx, ra).u64 = GetGPR(ctx, rt).u64 & ~GetGPR(ctx, rb).u64;
            if (rc) ctx.cr0.compare<int32_t>(GetGPR(ctx, ra).s32, 0, ctx.xer);
            break;
          }
          case 87: {  // lbzx
            uint32_t ea = (ra ? GetGPR(ctx, ra).u32 : 0) + GetGPR(ctx, rb).u32;
            GetGPR(ctx, rt).u64 = LoadU8(base, ea);
            break;
          }
          case 104: {  // neg
            GetGPR(ctx, rt).s64 = -GetGPR(ctx, ra).s64;
            if (rc) ctx.cr0.compare<int32_t>(GetGPR(ctx, rt).s32, 0, ctx.xer);
            break;
          }
          case 124: {  // nor
            GetGPR(ctx, ra).u64 = ~(GetGPR(ctx, rt).u64 | GetGPR(ctx, rb).u64);
            if (rc) ctx.cr0.compare<int32_t>(GetGPR(ctx, ra).s32, 0, ctx.xer);
            break;
          }
          case 136: {  // subfe
            uint64_t a = GetGPR(ctx, ra).u32;
            uint64_t b = GetGPR(ctx, rb).u32;
            uint64_t res = ~a + b + ctx.xer.ca;
            ctx.xer.ca = (res >> 32) != 0;
            GetGPR(ctx, rt).s64 = static_cast<int32_t>(res);
            if (rc) ctx.cr0.compare<int32_t>(GetGPR(ctx, rt).s32, 0, ctx.xer);
            break;
          }
          case 138: {  // adde
            uint64_t a = GetGPR(ctx, ra).u32;
            uint64_t b = GetGPR(ctx, rb).u32;
            uint64_t res = a + b + ctx.xer.ca;
            ctx.xer.ca = (res >> 32) != 0;
            GetGPR(ctx, rt).s64 = static_cast<int32_t>(res);
            if (rc) ctx.cr0.compare<int32_t>(GetGPR(ctx, rt).s32, 0, ctx.xer);
            break;
          }
          case 151: {  // stwx
            uint32_t ea = (ra ? GetGPR(ctx, ra).u32 : 0) + GetGPR(ctx, rb).u32;
            StoreU32BE(base, ea, GetGPR(ctx, rt).u32);
            break;
          }
          case 215: {  // stbx
            uint32_t ea = (ra ? GetGPR(ctx, ra).u32 : 0) + GetGPR(ctx, rb).u32;
            StoreU8(base, ea, GetGPR(ctx, rt).u8);
            break;
          }
          case 235: {  // mullw
            GetGPR(ctx, rt).s64 = static_cast<int64_t>(GetGPR(ctx, ra).s32) * GetGPR(ctx, rb).s32;
            if (rc) ctx.cr0.compare<int32_t>(GetGPR(ctx, rt).s32, 0, ctx.xer);
            break;
          }
          case 279: {  // lhzx
            uint32_t ea = (ra ? GetGPR(ctx, ra).u32 : 0) + GetGPR(ctx, rb).u32;
            GetGPR(ctx, rt).u64 = LoadU16BE(base, ea);
            break;
          }
          case 316: {  // xor
            GetGPR(ctx, ra).u64 = GetGPR(ctx, rt).u64 ^ GetGPR(ctx, rb).u64;
            if (rc) ctx.cr0.compare<int32_t>(GetGPR(ctx, ra).s32, 0, ctx.xer);
            break;
          }
          case 339: {  // mfspr
            uint32_t spr = ((rb & 31) << 5) | (ra & 31);
            if (spr == 8) {
              GetGPR(ctx, rt).u64 = ctx.lr;
            } else if (spr == 9) {
              GetGPR(ctx, rt).u64 = ctx.ctr.u64;
            } else {
              GetGPR(ctx, rt).u64 = 0;
            }
            break;
          }
          case 407: {  // sthx
            uint32_t ea = (ra ? GetGPR(ctx, ra).u32 : 0) + GetGPR(ctx, rb).u32;
            StoreU16BE(base, ea, GetGPR(ctx, rt).u16);
            break;
          }
          case 444: {  // or / mr
            GetGPR(ctx, ra).u64 = GetGPR(ctx, rt).u64 | GetGPR(ctx, rb).u64;
            if (rc) ctx.cr0.compare<int32_t>(GetGPR(ctx, ra).s32, 0, ctx.xer);
            break;
          }
          case 467: {  // mtspr
            uint32_t spr = ((rb & 31) << 5) | (ra & 31);
            if (spr == 8) {
              ctx.lr = GetGPR(ctx, rt).u64;
            } else if (spr == 9) {
              ctx.ctr.u64 = GetGPR(ctx, rt).u64;
            }
            break;
          }
          case 535: {  // lfsx
            uint32_t ea = (ra ? GetGPR(ctx, ra).u32 : 0) + GetGPR(ctx, rb).u32;
            GetFPR(ctx, rt).f64 = static_cast<double>(std::bit_cast<float>(LoadU32BE(base, ea)));
            break;
          }
          case 536: {  // srw
            uint32_t sh = GetGPR(ctx, rb).u32 & 63;
            GetGPR(ctx, ra).u64 = (sh < 32) ? (GetGPR(ctx, rt).u32 >> sh) : 0;
            if (rc) ctx.cr0.compare<int32_t>(GetGPR(ctx, ra).s32, 0, ctx.xer);
            break;
          }
          case 663: {  // stfsx
            uint32_t ea = (ra ? GetGPR(ctx, ra).u32 : 0) + GetGPR(ctx, rb).u32;
            StoreU32BE(base, ea, std::bit_cast<uint32_t>(static_cast<float>(GetFPR(ctx, rt).f64)));
            break;
          }
          case 792: {  // sraw
            uint32_t sh = GetGPR(ctx, rb).u32 & 63;
            int32_t s = GetGPR(ctx, rt).s32;
            if (sh >= 32) {
              GetGPR(ctx, ra).s64 = (s < 0) ? -1 : 0;
              ctx.xer.ca = (s < 0) ? 1 : 0;
            } else {
              GetGPR(ctx, ra).s64 = s >> sh;
              ctx.xer.ca = (s < 0 && (s & ((1u << sh) - 1)) != 0) ? 1 : 0;
            }
            if (rc) ctx.cr0.compare<int32_t>(GetGPR(ctx, ra).s32, 0, ctx.xer);
            break;
          }
          case 824: {  // srawi
            int32_t s = GetGPR(ctx, rt).s32;
            GetGPR(ctx, ra).s64 = s >> rb;
            ctx.xer.ca = (s < 0 && rb > 0 && (s & ((1u << rb) - 1)) != 0) ? 1 : 0;
            if (rc) ctx.cr0.compare<int32_t>(GetGPR(ctx, ra).s32, 0, ctx.xer);
            break;
          }
          case 922: {  // extsh
            GetGPR(ctx, ra).s64 = GetGPR(ctx, rt).s16;
            if (rc) ctx.cr0.compare<int32_t>(GetGPR(ctx, ra).s32, 0, ctx.xer);
            break;
          }
          case 954: {  // extsb
            GetGPR(ctx, ra).s64 = GetGPR(ctx, rt).s8;
            if (rc) ctx.cr0.compare<int32_t>(GetGPR(ctx, ra).s32, 0, ctx.xer);
            break;
          }
          case 986: {  // extsw
            GetGPR(ctx, ra).s64 = GetGPR(ctx, rt).s32;
            if (rc) ctx.cr0.compare<int32_t>(GetGPR(ctx, ra).s32, 0, ctx.xer);
            break;
          }
          default: {
            REXLOG_WARN("[WoS-Interp] Unhandled op31 xo={} at PC=0x{:08X} (insn=0x{:08X})",
                        xo, pc, insn);
            break;
          }
        }
        pc += 4;
        break;
      }
      case 32: {  // lwz
        uint32_t ea = (ra ? GetGPR(ctx, ra).u32 : 0) + simm;
        GetGPR(ctx, rt).u64 = LoadU32BE(base, ea);
        pc += 4;
        break;
      }
      case 33: {  // lwzu
        uint32_t ea = GetGPR(ctx, ra).u32 + simm;
        GetGPR(ctx, rt).u64 = LoadU32BE(base, ea);
        GetGPR(ctx, ra).u64 = ea;
        pc += 4;
        break;
      }
      case 34: {  // lbz
        uint32_t ea = (ra ? GetGPR(ctx, ra).u32 : 0) + simm;
        GetGPR(ctx, rt).u64 = LoadU8(base, ea);
        pc += 4;
        break;
      }
      case 35: {  // lbzu
        uint32_t ea = GetGPR(ctx, ra).u32 + simm;
        GetGPR(ctx, rt).u64 = LoadU8(base, ea);
        GetGPR(ctx, ra).u64 = ea;
        pc += 4;
        break;
      }
      case 36: {  // stw
        uint32_t ea = (ra ? GetGPR(ctx, ra).u32 : 0) + simm;
        StoreU32BE(base, ea, GetGPR(ctx, rt).u32);
        pc += 4;
        break;
      }
      case 37: {  // stwu
        uint32_t ea = GetGPR(ctx, ra).u32 + simm;
        StoreU32BE(base, ea, GetGPR(ctx, rt).u32);
        GetGPR(ctx, ra).u64 = ea;
        pc += 4;
        break;
      }
      case 38: {  // stb
        uint32_t ea = (ra ? GetGPR(ctx, ra).u32 : 0) + simm;
        StoreU8(base, ea, GetGPR(ctx, rt).u8);
        pc += 4;
        break;
      }
      case 39: {  // stbu
        uint32_t ea = GetGPR(ctx, ra).u32 + simm;
        StoreU8(base, ea, GetGPR(ctx, rt).u8);
        GetGPR(ctx, ra).u64 = ea;
        pc += 4;
        break;
      }
      case 40: {  // lhz
        uint32_t ea = (ra ? GetGPR(ctx, ra).u32 : 0) + simm;
        GetGPR(ctx, rt).u64 = LoadU16BE(base, ea);
        pc += 4;
        break;
      }
      case 42: {  // lha
        uint32_t ea = (ra ? GetGPR(ctx, ra).u32 : 0) + simm;
        GetGPR(ctx, rt).s64 = static_cast<int16_t>(LoadU16BE(base, ea));
        pc += 4;
        break;
      }
      case 44: {  // sth
        uint32_t ea = (ra ? GetGPR(ctx, ra).u32 : 0) + simm;
        StoreU16BE(base, ea, GetGPR(ctx, rt).u16);
        pc += 4;
        break;
      }
      case 48: {  // lfs
        uint32_t ea = (ra ? GetGPR(ctx, ra).u32 : 0) + simm;
        GetFPR(ctx, rt).f64 = static_cast<double>(std::bit_cast<float>(LoadU32BE(base, ea)));
        pc += 4;
        break;
      }
      case 50: {  // lfd
        uint32_t ea = (ra ? GetGPR(ctx, ra).u32 : 0) + simm;
        GetFPR(ctx, rt).u64 = LoadU64BE(base, ea);
        pc += 4;
        break;
      }
      case 52: {  // stfs
        uint32_t ea = (ra ? GetGPR(ctx, ra).u32 : 0) + simm;
        StoreU32BE(base, ea, std::bit_cast<uint32_t>(static_cast<float>(GetFPR(ctx, rt).f64)));
        pc += 4;
        break;
      }
      case 54: {  // stfd
        uint32_t ea = (ra ? GetGPR(ctx, ra).u32 : 0) + simm;
        StoreU64BE(base, ea, GetFPR(ctx, rt).u64);
        pc += 4;
        break;
      }
      case 58: {  // ld / ldu / lwa
        int32_t ds = simm & ~3;
        uint32_t ea = (ra ? GetGPR(ctx, ra).u32 : 0) + ds;
        if ((insn & 3) == 2) {
          GetGPR(ctx, rt).s64 = static_cast<int32_t>(LoadU32BE(base, ea));
        } else {
          GetGPR(ctx, rt).u64 = LoadU64BE(base, ea);
          if ((insn & 3) == 1) GetGPR(ctx, ra).u64 = ea;
        }
        pc += 4;
        break;
      }
      case 62: {  // std / stdu
        int32_t ds = simm & ~3;
        uint32_t ea = (ra ? GetGPR(ctx, ra).u32 : 0) + ds;
        StoreU64BE(base, ea, GetGPR(ctx, rt).u64);
        if ((insn & 3) == 1) GetGPR(ctx, ra).u64 = ea;
        pc += 4;
        break;
      }
      case 59:    // single-precision FP ops
      case 63: {  // double-precision FP ops
        uint32_t xo5 = (insn >> 1) & 0x1F;
        uint32_t xo10 = (insn >> 1) & 0x3FF;
        uint32_t rc_bit = (insn >> 6) & 31;
        if (xo5 == 18) {  // fdivs / fdiv
          GetFPR(ctx, rt).f64 = GetFPR(ctx, ra).f64 / GetFPR(ctx, rb).f64;
        } else if (xo5 == 20) {  // fsubs / fsub
          GetFPR(ctx, rt).f64 = GetFPR(ctx, ra).f64 - GetFPR(ctx, rb).f64;
        } else if (xo5 == 21) {  // fadds / fadd
          GetFPR(ctx, rt).f64 = GetFPR(ctx, ra).f64 + GetFPR(ctx, rb).f64;
        } else if (xo5 == 25) {  // fmuls / fmul
          GetFPR(ctx, rt).f64 = GetFPR(ctx, ra).f64 * GetFPR(ctx, rc_bit).f64;
        } else if (xo5 == 28) {  // fmsubs / fmsub
          GetFPR(ctx, rt).f64 = GetFPR(ctx, ra).f64 * GetFPR(ctx, rc_bit).f64 - GetFPR(ctx, rb).f64;
        } else if (xo5 == 29) {  // fmadds / fmadd
          GetFPR(ctx, rt).f64 = GetFPR(ctx, ra).f64 * GetFPR(ctx, rc_bit).f64 + GetFPR(ctx, rb).f64;
        } else if (op == 63 && xo10 == 0) {  // fcmpu
          GetCR(ctx, (rt >> 2) & 7).compare(GetFPR(ctx, ra).f64, GetFPR(ctx, rb).f64);
        } else if (op == 63 && xo10 == 12) {  // frsp
          GetFPR(ctx, rt).f64 = static_cast<double>(static_cast<float>(GetFPR(ctx, rb).f64));
        } else if (op == 63 && xo10 == 40) {  // fneg
          GetFPR(ctx, rt).f64 = -GetFPR(ctx, rb).f64;
        } else if (op == 63 && xo10 == 72) {  // fmr
          GetFPR(ctx, rt).f64 = GetFPR(ctx, rb).f64;
        } else if (op == 63 && xo10 == 264) {  // fabs
          GetFPR(ctx, rt).f64 = std::abs(GetFPR(ctx, rb).f64);
        }
        pc += 4;
        break;
      }
      default: {
        REXLOG_WARN("[WoS-Interp] Unhandled opcode {} at PC=0x{:08X} (insn=0x{:08X}), returning 0",
                    op, pc, insn);
        ctx.r3.u64 = 0;
        return;
      }
    }
  }
}

inline void InstallUnregisteredFuncFallbacks(rex::Runtime* runtime) {
  g_runtime = runtime;
  if (!runtime || !runtime->memory() || !runtime->function_dispatcher()) {
    return;
  }
  uint8_t* base = runtime->memory()->virtual_membase();
  auto* disp = runtime->function_dispatcher();
  uint32_t installed = 0;

  for (uint32_t addr = REX_CODE_BASE; addr < (REX_CODE_BASE + REX_CODE_SIZE); addr += 4) {
    PPCFunc** slot = reinterpret_cast<PPCFunc**>(
        base + REX_IMAGE_BASE + REX_IMAGE_SIZE +
        (static_cast<uint64_t>(addr - REX_CODE_BASE) * 2ULL));
    if (*slot == nullptr) {
      uint32_t insn = LoadU32BE(base, addr);
      if (insn != 0) {
        disp->SetFunction(addr, &UnregisteredGuestFuncHandler);
        // Keep fast-path table slot null so REX_CALL_INDIRECT_FUNC populates
        // ctx.last_indirect_target before calling ResolveIndirectFunction!
        *slot = nullptr;
        ++installed;
      }
    }
  }
  REXLOG_INFO(
      "[WoS-Interp] Installed fallback handler for {} unindexed guest code addresses in [0x{:08X}..0x{:08X})",
      installed, REX_CODE_BASE, REX_CODE_BASE + REX_CODE_SIZE);

  // Hook D3D Swap Internal (sub_82AC4E48) for telemetry
  static PPCFunc* orig_swapint = disp->GetFunction(0x82AC4E48);
  disp->SetFunction(0x82AC4E48, +[](PPCContext& ctx, uint8_t* base) {
    wos::perf::RecordFrame();
    if (orig_swapint) orig_swapint(ctx, base);
  });
}

}  // namespace wos::stubs
