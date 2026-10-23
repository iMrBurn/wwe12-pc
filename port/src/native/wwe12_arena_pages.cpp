// WWE '12: more pages on the arena select screen (docs/mod_maker.md, "Nouveaux emplacements d'arènes").
//
// The arena select screen of the match setup (title ARENAS, 21 banners: four big ones on top) is a native class:
// constructor sub_825A67A0(this, arena list, mode, current stage, ?), 12,272 bytes. Its tiles come from a data
// block at 0x82028560 (in the image's read-only section: its host page is made writable first):
//   +0    22 x {u32 stage, u32 flag}: stages 0..20, then 300 (random);
//   +176  a 4 x 6 grid of cells {record pointer, up/down/left/right}: which record each tile shows;
//   +368  21 x {stage, banner texture name "arenaNN"}: sub_825A3CD8 finds each tile's banner by stage.
// The chosen arena is read through sub_825A56C8 (grid cell -> record -> stage); the match setup then stores it
// (u16 at matchSettings+898, sub_826CBFD0). Tabs (this+3260: arenas, brawl areas, ...) change with LB / RB
// (widget vtable 0x820287E0 slots 10 / 13 -> sub_825A5C88(this, 0 = previous / 1 = next)), which only stops on
// tabs whose flag is set: in an exhibition only the arena tab is, and the widget then doesn't even pass LB / RB
// on. So the pages read the pad themselves while the screen draws (sub_825A6A98, every frame): XInputGetState
// (sub_82A8C070, the keyboard's bindings included), LB 0x0100 / RB 0x0200, on the press.
// (A look-alike class, sub_826361A8 with its table at 0x83071ED0, is not this screen.)
// The grid has 21 tiles, so more arenas means pages: on the arena tab, LB / RB go through the pages (the records'
// stages rewritten, the banners bound again, the cursor back on the first tile), then on to the next tab if any.
//
// Step 1 (test): wwe12_arena_pages_test = true adds one page, the 21 arenas in reverse order.

#include <array>
#include <cstdint>
#include <vector>

#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>

#include <rex/cvar.h>
#include <rex/runtime.h>

#include "core/logging.h"
#include "core/memory_helpers.h"
#include "svr_hook.h"

REXCVAR_DEFINE_BOOL(wwe12_arena_pages_test, false, "WWE12",
                    "Test: a second page on the arena select screen (the 21 arenas in reverse order).");

namespace {

constexpr u32 kRecords = 0x82028560;  // 22 x {stage, flag}; the first 21 are the tiles
constexpr int kTiles = 21;

std::array<u32, kTiles> g_original{};
bool g_saved = false;
std::vector<std::array<u32, kTiles>> g_pages;  // stages per page; page 0 = the game's
int g_page = 0;

void SaveOriginal() {
  if (g_saved)
    return;
  // The records sit in read-only data: the host page(s) holding them are made writable once.
  for (u32 va = kRecords & ~0xFFFu; va < kRecords + kTiles * 8; va += 0x1000) {
    DWORD old = 0;
    if (!VirtualProtect(bd::mem::at<u8>(va), 0x1000, PAGE_READWRITE, &old))
      REXLOG_INFO("[arena pages] page {:08X} could not be made writable", va);
  }
  for (int i = 0; i < kTiles; ++i)
    g_original[i] = bd::mem::load<u32>(kRecords + i * 8);
  g_saved = true;
}

void BuildPages() {
  g_pages.clear();
  g_pages.push_back(g_original);
  if (REXCVAR_GET(wwe12_arena_pages_test)) {
    std::array<u32, kTiles> reversed;
    for (int i = 0; i < kTiles; ++i)
      reversed[i] = g_original[kTiles - 1 - i];
    g_pages.push_back(reversed);
  }
}

void WritePage(int page) {
  for (int i = 0; i < kTiles; ++i)
    bd::mem::store<u32>(kRecords + i * 8, g_pages[size_t(page)][i]);
}

u32 Field(u32 screen, u32 offset) { return bd::mem::load<u32>(screen + offset); }

// Any tab but the arenas' that the screen allows (the checks of sub_825A5C88).
bool OtherTabs(u32 screen) {
  return Field(screen, 10868) != 0 || Field(screen, 10856) != 0 || int32_t(Field(screen, 10864)) > 0 ||
         Field(screen, 10872) != 0;
}

}  // namespace

REX_EXTERN(__imp__sub_825A67A0);  // arena select: constructor
REX_EXTERN(__imp__sub_825A5C88);  // arena select: LB / RB (this, 0 previous / 1 next)
REX_EXTERN(__imp__sub_824C3000);  // (this + 3444, 0): what a tab change does first
REX_EXTERN(__imp__sub_825A3CD8);  // arena select: bind the tiles' banners (this)
REX_EXTERN(__imp__sub_825A3AB8);  // arena select: cursor on (this, row, column)
REX_EXTERN(__imp__sub_828BF398);  // grid widget (this + 44): vtable 0x820287E0 slot 4
REX_EXTERN(__imp__sub_824C3220);  // (this + 3368)
REX_EXTERN(__imp__sub_824D19E8);  // arena select: redraw (this)
REX_EXTERN(__imp__sub_825A6A98);  // arena select: draw (every frame)
REX_EXTERN(sub_82A8C070);         // XInputGetState(user, state) (hooked by wwe12_intros.cpp: call the hooked one)

namespace {
u32 g_screen = 0;     // the arena select screen (its constructor's this)
u32 g_pad_state = 0;  // guest X_INPUT_STATE for our own reads
u16 g_last_buttons = 0;
}  // namespace

extern "C" REX_FUNC(sub_825A67A0) {
  g_screen = ctx.r3.u32;
  g_last_buttons = 0xFFFF;  // (a button still held from the screen before doesn't count)
  SaveOriginal();
  BuildPages();
  g_page = 0;
  WritePage(0);
  REXLOG_INFO("[arena pages] arena select opened: {} page(s)", g_pages.size());
  __imp__sub_825A67A0(ctx, base);
}

namespace {

// What sub_825A5C88 does on a tab change, for a page change: banners bound again, cursor on the first tile.
void Refresh(const PPCContext &in, u8 *base, u32 screen) {
  PPCContext x = in;
  auto call = [&](void (*fn)(PPCContext &, u8 *), u32 a, u32 b = 0, u32 c = 0) {
    x = in;
    x.r3.u64 = a;
    x.r4.u64 = b;
    x.r5.u64 = c;
    fn(x, base);
  };
  call(__imp__sub_824C3000, screen + 3444, 0);
  call(__imp__sub_825A3CD8, screen);
  call(__imp__sub_825A3AB8, screen, 0, 0);
  call(__imp__sub_828BF398, screen + 44);
  call(__imp__sub_824C3220, screen + 3368);
  call(__imp__sub_824D19E8, screen);
}

}  // namespace

namespace {

// LB / RB on the arena tab: true when it was a page change (else the tab change goes on).
bool TurnPage(const PPCContext &in, u8 *base, u32 screen, bool next) {
  const int pages = int(g_pages.size());
  if (!g_saved || pages < 2 || Field(screen, 3260) != 0)
    return false;
  const int page = g_page + (next ? 1 : -1);
  if (page < 0 || page >= pages) {
    if (OtherTabs(screen)) {
      g_page = next ? 0 : pages - 1;  // on to another tab; the arena tab comes back on the page next to it
      WritePage(g_page);
      return false;
    }
  }
  g_page = (page + pages) % pages;
  WritePage(g_page);
  Refresh(in, base, screen);
  REXLOG_INFO("[arena pages] page {} of {}", g_page + 1, pages);
  return true;
}

}  // namespace

// The tab buttons, when the widget passes them on (screens with more than one tab).
extern "C" REX_FUNC(sub_825A5C88) {
  if (TurnPage(ctx, base, ctx.r3.u32, ctx.r4.u32 != 0))
    return;
  __imp__sub_825A5C88(ctx, base);
}

// Every frame the screen draws: LB / RB read from the pad (one tab only: the widget drops them).
extern "C" REX_FUNC(sub_825A6A98) {
  const u32 screen = g_screen;
  const bool ours = screen && (ctx.r3.u32 == screen || ctx.r3.u32 == screen + 28);
  if (ours && g_pages.size() > 1 && !OtherTabs(screen)) {
    if (!g_pad_state)
      g_pad_state = REX_KERNEL_MEMORY()->SystemHeapAlloc(16, 0x10);
    PPCContext x = ctx;
    x.r3.u64 = 0;
    x.r4.u64 = g_pad_state;
    sub_82A8C070(x, base);
    const u16 buttons = x.r3.u32 == 0 ? bd::mem::load<u16>(g_pad_state + 4) : 0;
    const u16 pressed = u16(buttons & ~g_last_buttons);
    g_last_buttons = buttons;
    if (pressed & 0x0200)
      TurnPage(ctx, base, screen, true);
    else if (pressed & 0x0100)
      TurnPage(ctx, base, screen, false);
  }
  __imp__sub_825A6A98(ctx, base);
}
