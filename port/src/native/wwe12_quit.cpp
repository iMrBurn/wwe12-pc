// WWE '12 PC: a "quit the game" entry in the main menu, in place of the unused "Xbox LIVE" one.
//
// Step 1 (this file): the label. The game's strings are tables (string.pac, one per language)
// loaded into guest memory as-is: a u32 0, a u32 entry count, then 16-byte little-endian entries
// {text offset from the table start, length, string id, 0} and the NUL-terminated UTF-8 texts.
// The main menu's "Xbox LIVE" is string id 41006. Once the table is loaded, its text is
// rewritten in place (shorter, so it fits) with the chosen language's "quit".
//
// Step 2: the action. The main menu's "decide" handler, sub_82422BD8 (a virtual method; this =
// r3 - 28), switches on the state at this+56: 1-3 open sub-menus, 0 and 4 start modes, and 5 or
// more take the Xbox LIVE path (sign-in check, then message 50373 "you must be signed in to an
// Xbox LIVE profile"). On that path the game is closed instead, the way the window's close
// button does it.
//
// Disabled with wwe12_quit_entry = false in wwe12.toml.

#include <algorithm>
#include <atomic>
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <string_view>

#include <fmt/format.h>
#include <thread>

#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>

#include <rex/cvar.h>

#include "core/logging.h"
#include "core/memory_helpers.h"
#include "svr_hook.h"

REXCVAR_DEFINE_BOOL(wwe12_quit_entry, true, "WWE12",
                    "Main menu: the unused \"Xbox LIVE\" entry becomes \"quit the game\".");
REXCVAR_DEFINE_BOOL(wwe12_pc_texts, true, "WWE12",
                    "Texts that mention the Xbox 360 console (\"do not turn off\") say PC.");
REXCVAR_DECLARE(uint32_t, user_language);

namespace bd::gpu {
namespace {

constexpr u32 kXboxLiveStringId = 41006;

const char *QuitLabel() {
  switch (REXCVAR_GET(user_language)) {
  case 3: return "BEENDEN";   // German
  case 4: return "QUITTER";   // French
  case 5: return "SALIR";     // Spanish
  case 6: return "ESCI";      // Italian
  default: return "QUIT";     // English
  }
}

// "Do not turn off your Xbox 360 console" (save messages), in each language, says PC instead.
// The phrase around an "Xbox 360" hit is replaced in place; the text only gets shorter, so the
// rest of the string moves left and the freed bytes become NULs.
struct ConsolePhrase {
  const char *before;  // text right before "Xbox 360"
  const char *after;   // text right after it
  const char *pc;      // replacement for before + "Xbox 360" + after
};
constexpr ConsolePhrase kConsolePhrases[] = {
    {"votre Console ", "", "votre PC"},   // French
    {"your ", " console", "your PC"},     // English
    {"la console ", "", "il PC"},         // Italian
    {"la Consola ", "", "el PC"},         // Spanish
    {"tu Consola ", "", "tu PC"},         // Spanish
    {"deine ", " Konsole", "deinen PC"},  // German
};

u32 RewriteConsolePhrase(u8 *hit, u8 *region_start, u8 *region_end) {
  for (const ConsolePhrase &ph : kConsolePhrases) {
    const size_t nb = std::strlen(ph.before), na = std::strlen(ph.after);
    u8 *start = hit - nb;
    u8 *end = hit + 8 + na;
    if (start < region_start || end > region_end || std::memcmp(start, ph.before, nb) != 0 ||
        std::memcmp(hit + 8, ph.after, na) != 0)
      continue;
    // The string's end (its NUL), at most 1 KB on.
    u8 *nul = end;
    while (nul < region_end && nul < end + 1024 && *nul)
      ++nul;
    if (nul >= region_end || *nul)
      return 0;
    const size_t np = std::strlen(ph.pc);
    const size_t tail = size_t(nul - end) + 1;  // with the NUL
    std::memcpy(start, ph.pc, np);
    std::memmove(start + np, end, tail);
    std::memset(start + np + tail, 0, size_t(end - start) - np);
    return 1;
  }
  return 0;
}

// Scans committed guest memory for "Xbox LIVE\0" texts that a string table entry of id 41006
// points at (entry stored little- or big-endian: the file's layout, or byte-swapped at load), and
// rewrites them. Returns the number of texts patched; `candidates` counts the raw text hits.
u32 PatchLoadedTables(u32 &candidates, u32 &pc_texts) {
  u8 *const base = bd::mem::at<u8>(0x40000000) - 0x40000000;
  const char *label = QuitLabel();
  const size_t label_len = std::strlen(label);
  static const char kText[] = "Xbox LIVE";  // 9 chars + NUL = the entry length, 10
  u32 patched = 0;
  candidates = 0;
  pc_texts = 0;
  for (const auto [lo, hi] : {std::pair<u32, u32>{0x00010000u, 0x9FFF0000u},
                              std::pair<u32, u32>{0xA0000000u, 0xC0000000u}}) {
    u8 *p = base + lo;
    u8 *const end = base + hi;
    while (p < end) {
      MEMORY_BASIC_INFORMATION mbi;
      if (!VirtualQuery(p, &mbi, sizeof(mbi)))
        break;
      u8 *region_start = static_cast<u8 *>(mbi.BaseAddress);
      u8 *region_end = region_start + mbi.RegionSize;
      if (region_end > end)
        region_end = end;
      const bool writable = mbi.State == MEM_COMMIT &&
                            !(mbi.Protect & (PAGE_NOACCESS | PAGE_GUARD)) &&
                            (mbi.Protect & (PAGE_READWRITE | PAGE_EXECUTE_READWRITE));
      if (writable) {
        for (u8 *t = p; t + sizeof(kText) <= region_end;) {
          t = static_cast<u8 *>(std::memchr(t, 'X', size_t(region_end - t)));
          if (!t || t + sizeof(kText) > region_end)
            break;
          if (REXCVAR_GET(wwe12_pc_texts) && std::memcmp(t, "Xbox 360", 8) == 0) {
            pc_texts += RewriteConsolePhrase(t, region_start, region_end);
            ++t;
            continue;
          }
          if (!REXCVAR_GET(wwe12_quit_entry) || std::memcmp(t, kText, sizeof(kText)) != 0 ||
              (t > region_start && t[-1] != 0)) {
            ++t;
            continue;
          }
          ++candidates;
          // Look back for the entry {offset, 10, 41006, 0} whose offset lands on this text.
          bool hit = false;
          // Entries are 4-byte aligned; the text need not be.
          u8 *const first = reinterpret_cast<u8 *>((uintptr_t(t) - 16) & ~uintptr_t(3));
          for (u8 *e = first; e >= region_start && e > t - 0x40000 && !hit; e -= 4) {
            const u32 *w = reinterpret_cast<const u32 *>(e);
            for (int big = 0; big < 2 && !hit; ++big) {
              const auto rd = [&](int i) { return big ? _byteswap_ulong(w[i]) : w[i]; };
              if (rd(2) != kXboxLiveStringId || rd(1) != 10 || rd(3) != 0)
                continue;
              // The table starts where offset == t - table; it holds u32 0 then the count.
              u8 *table = t - rd(0);
              if (table < region_start || table > e)
                continue;
              if (*reinterpret_cast<const u32 *>(table) != 0)
                continue;
              hit = true;
            }
          }
          if (hit) {
            std::memset(t, 0, 10);
            std::memcpy(t, label, std::min<size_t>(label_len, 9));
            ++patched;
          }
          t += sizeof(kText);
        }
      }
      p = region_end;
    }
  }
  return patched;
}

std::atomic<bool> g_scanning{false};
std::atomic<u32> g_total_patched{0};

}  // namespace

// Called once per presented frame: a background scan rewrites any newly loaded string table.
// Dev aid: WWE12_MENU_DUMP=<file> writes the menu entry database (the object the menus look
// entries up in, pointer at 0x8324DEF4, records array at +16) to that file once, 30 s in.
static void DumpMenuDatabase(u64 frame) {
  static const char *path = std::getenv("WWE12_MENU_DB_DUMP");
  static bool done = false;
  if (!path || done || frame < 1800)
    return;
  done = true;
  const u32 db = bd::mem::load<u32>(0x8324DEF4);
  const u32 records = db ? bd::mem::load<u32>(db + 16) : 0;
  BD_INFO("WWE12 menu dump: db 0x{:08X} records 0x{:08X} | db words {:08X} {:08X} {:08X} {:08X} "
          "{:08X} {:08X} {:08X} {:08X}",
          db, records, bd::mem::load<u32>(db), bd::mem::load<u32>(db + 4),
          bd::mem::load<u32>(db + 8), bd::mem::load<u32>(db + 12), bd::mem::load<u32>(db + 16),
          bd::mem::load<u32>(db + 20), bd::mem::load<u32>(db + 24), bd::mem::load<u32>(db + 28));
  // The database's own pointers (+4, +12, +16), 64 KB each, one after the other.
  if (FILE *f = std::fopen(path, "wb")) {
    for (const u32 field : {4u, 12u, 16u}) {
      const u32 at = db ? bd::mem::load<u32>(db + field) : 0;
      for (u32 off = 0; off < 0x10000; off += 4) {
        const u32 w = at ? bd::mem::load<u32>(at + off) : 0;
        std::fwrite(&w, 4, 1, f);  // the guest's big-endian word, as a host value
      }
    }
    std::fclose(f);
  }
}

void Wwe12QuitEntryTick(u64 frame) {
  DumpMenuDatabase(frame);
  // Every second for the first 30 s (the save-loading message shows early), then every 2 s: the
  // game reloads its tables (after a match) and rebuilds the main menu right away.
  const u64 period = frame < 1800 ? 60 : 120;
  if ((!REXCVAR_GET(wwe12_quit_entry) && !REXCVAR_GET(wwe12_pc_texts)) || frame % period != 0 ||
      g_scanning.exchange(true))
    return;
  std::thread([] {
    u32 candidates = 0, pc_texts = 0;
    const auto t0 = std::chrono::steady_clock::now();
    const u32 n = PatchLoadedTables(candidates, pc_texts);
    static std::atomic<u32> s_timed{0};
    if (s_timed.fetch_add(1) < 3)
      BD_INFO("WWE12 texts: scan took {} ms",
              std::chrono::duration_cast<std::chrono::milliseconds>(
                  std::chrono::steady_clock::now() - t0).count());
    if (pc_texts)
      BD_INFO("WWE12 texts: {} \"Xbox 360 console\" message(s) now say PC", pc_texts);
    static std::atomic<u32> s_reports{0};
    if (n || s_reports.fetch_add(1) < 6)
      BD_INFO("WWE12 menu: \"Xbox LIVE\" texts found {}, rewritten as \"{}\": {}", candidates,
              QuitLabel(), n);
    g_total_patched += n;
    g_scanning = false;
  }).detach();
}

}  // namespace bd::gpu

namespace {

BOOL CALLBACK CloseOwnWindow(HWND hwnd, LPARAM) {
  DWORD pid = 0;
  GetWindowThreadProcessId(hwnd, &pid);
  if (pid == GetCurrentProcessId() && IsWindowVisible(hwnd) && !GetWindow(hwnd, GW_OWNER)) {
    PostMessageW(hwnd, WM_CLOSE, 0, 0);
    return FALSE;
  }
  return TRUE;
}

}  // namespace

REX_EXTERN(__imp__sub_8253C3F0);
REX_EXTERN(__imp__sub_8253A028);
// The menus' "decide" handler: it asks sub_8253A028(this) for the selected entry's record, and
// an entry flagged 0x100 at +40 needs Xbox LIVE (sign-in check, then message 50373). The main
// menu's "Xbox LIVE" entry is the one whose record holds its label id, 41006: that one quits.
extern "C" REX_FUNC(sub_8253C3F0) {
  if (REXCVAR_GET(wwe12_quit_entry)) {
    PPCContext probe = ctx;
    __imp__sub_8253A028(probe, base);
    const u32 entry = probe.r3.u32;
    if (entry && entry != 0xFFFFFFFFu) {
      bool is_quit = false;
      std::string words;
      for (u32 i = 0; i < 16; ++i) {
        const u32 w = bd::mem::load<u32>(entry + i * 4);
        words += fmt::format(" {:08X}", w);
        is_quit |= (w == bd::gpu::kXboxLiveStringId);
      }
      static std::atomic<u32> s_logged{0};
      if (s_logged.fetch_add(1) < 64)
        BD_INFO("WWE12 menu: decide, menu 0x{:08X} entry 0x{:08X}:{}", ctx.r3.u32, entry, words);
      // Dev aid (WWE12_MENU_DUMP): the menu object and the records around the chosen one.
      static bool s_dumped = false;
      if (const char *path = std::getenv("WWE12_MENU_DUMP"); path && !s_dumped) {
        s_dumped = true;
        if (FILE *f = std::fopen(path, "wb")) {
          for (const u32 at : {ctx.r3.u32, entry - 0x1000u}) {
            for (u32 off = 0; off < 0x4000; off += 4) {
              const u32 w = bd::mem::load<u32>(at + off);
              std::fwrite(&w, 4, 1, f);
            }
          }
          std::fclose(f);
          BD_INFO("WWE12 menu: dumped menu 0x{:08X} and records from 0x{:08X}", ctx.r3.u32,
                  entry - 0x1000u);
        }
      }
      const u32 flags = bd::mem::load<u32>(entry + 40);
      if (is_quit && (flags & 0x100u)) {
        BD_INFO("WWE12 menu: \"quit\" chosen, closing the game");
        EnumWindows(CloseOwnWindow, 0);
        return;
      }
    }
  }
  __imp__sub_8253C3F0(ctx, base);
}
