# WWE12 — Text-entry analysis report (offline, static recomp sources)

Scope: `port\generated\default\wwe12_recomp.*.cpp` only. No runtime/binary data. All line
numbers are recomp source lines; addresses are guest PPC addresses.

Runtime-verified facts (input, trusted): every-frame reads of `0xA3EE62C0` and `0xA3EE628C`
from inside `sub_824C9228`; entry object at guest `0xA3EE6270` with `[+0x30]`=self-ptr,
`[+0x40]`="20 05 06 3C", `[+0x44]`="00 05 06 26", `[+0x4F]`=0x36, `[+0x50]`=text buffer
(NUL initially, = `0xA3EE62C0`), display copy at `0xA3EE6700` (= entry+0x490);
`sub_82C41BF0` reads `0xA3EE6200` every frame via vtable `[vptr+52]`.

---

## (a) How `sub_824C9228` obtains the text pointer

### Full chain (top → bottom)

```
sub_82C41BF0                          (per-frame screen update; widget state r? = 0xA3EE6200)
  └─ bctrl [vptr+52]                  → virtual dispatch, lands in:
sub_824C6508   (recomp.33.cpp:7872-8132)
  · r27 = [container+112]             — 240-class widget array base   (loop at loc_824C6634, 8065-8082)
  · r28 = [container+76]              — widget count
  · guard  [container+116] != 0
  └─ sub_824CAF58(screen r31, widget_i r4, event r26)   per widget (widget_i += 240/iter)
sub_824CAF58   (recomp.76.cpp:7878-8496)  — 12-case dispatcher
  · r4 = widget-state; r11 = [r4+176]                  (7919)
  · switch on [r11+4] - 1, jump table at 0x821C????    (7929-7939)
  · case 0 (loc_824CAFF4) → sub_824C9228(screen, widget)   (call at 7992)
sub_824C9228   (recomp.194.cpp:7772-9310)  — per-frame text widget draw
  · r24 = r3 (screen/context), r31 = r4 (240-stride text widget)
  · lwz r26, 200(r31)                 (8127, reload 8305)   →  r26 = [widget+200] = TEXT PTR
  · lbz r11, 0(r26)                   (8257, 8597, 9255)    →  the observed reads of 0xA3EE62C0
```

### Who fills `[widget+200]`?

`sub_824C60B0` (recomp.128.cpp:7661-7866) — widget init/zero pass:

- First loop, stride **144**: over `[r29+4]`/count `[r29+20]`; zeroes +116..+140 on 144-class
  widgets (7703-7735).
- Second loop, stride **240**: over `[r29+8]`/count `[r29+24]`; zeroes +180..+208 (f31
  floats), +212/213/214, +215(-1), +216(-1), +220, +228 (7760-7800); also sets `[r30+216]`
  via `[[r30+68] index]` (7829-7846).
- Then, per 240-widget with `[r30+68] > 0`, `[r30+72]` in range, `[r29+16] != 0`:
  - `r3 = [[0x8317B86C]+0]`  ← object held by guest global **0x8317B86C**
  - `lwz r8, 20(r9)`; `bctrl` — **virtual method at vtable slot +20**
  - result stored: `stw r3, 200(r30)`            (7847-7859)

So `[widget+200]` is populated by a vtable-slot-+20 call on the manager object behind
global `0x8317B86C`. For the entry widget, that method returns `widget+0x50`, i.e. the
text is stored **inline inside the 240-widget itself** at +0x50, and `[+200]` is merely a
self-pointer to it: `0xA3EE6270 + 0x50 = 0xA3EE62C0`. This also reconciles the second
observed read `0xA3EE628C = entry+28` (type field, `lwz r10,28(r31)` at 8006/7988/8532/8972).

Static callers of `sub_824C60B0` (call chain one level up):
- `recomp.99.cpp:9878 / 9904 / 9930` in `sub_82576688` (9629+). That function gates calls
  with `[global 0x8201DAD4-ish]` +72 / +68 tests (9906-9908), then
  `sub_824C5618(r30=[r11+68], r5=1, r4=13)` (9944), `sub_824C5458(...r4=30)`,
  `stw r31,180(r3)`. r27 base `0x82040000`, r30 base `0x8200xxxx`.
- `recomp.182.cpp:20545 / 20564`, `recomp.78.cpp:20949 / 20968`, `recomp.199.cpp:21520`.

### Hook recommendation (a)

- Hook `sub_824CAF58` (0x824CAF58) at entry: `widget = r4`; `textPtr = *(u32*)(widget+200)`.
  For the on-screen keyboard entry widget this is `0xA3EE62C0`; text is NUL-terminated
  inline at `widget+0x50`.
- Alternate hook: `sub_824C9228` (0x824C9228) entry, same read of `[r4+200]`.
- Widget fields confirmed: +76 pool id, +131 align type, +184..208 geometry floats,
  +212/213/214/215/216 flags, +220, +228 flags (bits 0x8/0x10/0x20/0x40/0x80/0x100/0x200).
- Pool indirection (draw objects): `lwz r11,76(r31)` (7824/8762); index math
  `(r11+11)*4` / `(r11+19)*4` against screen r24; `lwzx r11,r7,r24` (7850) / `lwzx r10,r6,r24`
  (7852); `[pool+8]`=count, `[pool+4]`=array of 1072-byte entries; `r29=[entry+1048]`
  (7866) = string draw object (vptr at +0, methods at vtable +152/+148/+56/+20/+72/+80/+64/+48/+4;
  bctrls at 8446/8827/8859/8891/9041/9100/9173). Others walk `[widget+200]` only.

---

## (b) The function that writes a typed char

### `sub_82BFD248` (recomp.143.cpp:25426-25487) — the only append-then-NUL primitive

Evidence: every `stbx` in the port was audited. The remaining hits (recomp.177.cpp:32704,
225.cpp:22180, 71.cpp:21429, 50.cpp:37323) are pixel blend/format code; the `stbx r20,r11,r30`
in `sub_824C9228` (194.cpp:8717) writes the draw-time scratch copy at `r24+240`, not the entry.
**`sub_82BFD248` is the one genuine char-append primitive.**

```c
sub_82BFD248(r3 = obj, r4 = char (low byte), r5 = second byte)
  r31 = r3 + 284;                       // buffer struct at obj+284: [+0]=ptr, [+4]=len, [+8]=cap
  sub_82BA48D8(...);                    // 25444
  if (len == (cap & 0x3FFFFFFF))        // 25447-25455 (cap mask 0x3FFFFFFF => upper 2 bits = flags)
      sub_82BA7058([obj+44], &struct, 2)   // realloc, growth factor 2 (25457-25465)
  r10 = len * 2 (rlwinm r10,r11,1)      // 25471   — 2 bytes per char
  r6  = len + 1;  [struct+4] = r6       // 25473-25478 — len++
  stbx r30, r10, r9;                    // 25479-25480  buf[len*2]   = (u8)char
  stb  r29, 1(r11);                     // 25481-25482  buf[len*2+1] = second byte (r5)
```

- **Zero static callers** in the entire port (grep `sub_82BFD248`: only
  `wwe12_init.cpp:47469`, `wwe12_funcs.h:47451`, `wwe12_register.cpp:47451`, its own file).
  It is therefore invoked exclusively through virtual dispatch (`bctrl`), consistent with
  it having no direct call sites.
- Belongs to the APP "text field" middleware class region `0x82BFxxxx`, whose objects are
  known to hold tagged 64-bit pointers (`"A3EE6270 00000001"` = ptr | tag 1). Corroboration:
  recomp.63.cpp near `loc_824C7160` — when `[r25+36] >= 2` and `r21 != 0`, `sub_824C6A38`
  copies two 8-byte tagged slots into a widget at +192/+200 (`std r9,192(r28)`,
  `std r8,200(r28)`), i.e. the 0x824Cxxxx screen objects hold tagged references to 0x82BFxxxx
  text-field objects.

### 2-byte vs 1-byte discrepancy — status

`sub_82BFD248` writes 2 bytes/char (UTF-16-style). The observed entry text at
`0xA3EE62C0` is the widget's **inline copy** at +0x50; runtime samples of it looked
stride-1 ASCII. Two explanations remain (documented, not fully resolved offline):

1. The middleware string is 2-byte internally, and a sync/convert step copies it into
   the widget inline buffer at +0x50 (the display copy at entry+0x490 = 0xA3EE6700
   suggests exactly such a conversion/copy stage);
2. There is another, still-unfound 1-byte append helper writing the inline buffer directly
   (would be a plain `stb` into +0x50-range, likely buried inside a generic string-copy
   called on keystroke; only stride-1 significance was visible in the runtime dump).

Either way: hooking `sub_82BFD248` gives byte0 (typed char) + byte1 of every character the
APP text-field class appends — byte1=0 for ASCII. Hooking it does NOT necessarily show the
same bytes landing at 0xA3EE62C0 directly; the sync path (if any) is one further hop.

Ruled out as inserters:
- `sub_824CD798` (recomp.230.cpp:7927), `sub_824C7180`, `sub_824C66A0` — cursor-move/layout handlers.
- `sub_824CCEA0` (recomp.46.cpp:7895+), `sub_824CE278` (51.cpp:7972-7983) — cell highlight/cursor set.

---

## (c) A-press virtual-method resolution

### The dispatch frame around the letter grid

- `sub_824CAF58` case 2/3 → `sub_824C6A38(screen r31, state r11)` (recomp.76.cpp:8090).
- `sub_824C6A38` (recomp.63.cpp, ~8167-9100): navigates grid cells via
  `[r25+40]` / `[r11(=[r25+140])+40]` then `sub_824CC7B8(r3, r4=[r29+76])` (8167-8199 region),
  flag test `[r29+228] & 0x2` (8209-8218), and two virtual calls on the active widget/object:
  - **bctrl at 8972**: `r11=[r28+0]` (vptr), `r10=[vptr+48]` → draw/geometry call,
    with scale floats from `[r28+32]/[r28+212]/[r28+36]` (8904-8970).
  - **bctrl at 9064**: vtable slot **+4** on fallback object (`r23`, else `r22`, else `r28`)
    — per-widget state refresh.
  Both are **state/draw dispatches, not the char inserter** (re-verified this session).
- Then `sub_824C6A38` copies tagged 16-byte refs to widget +192/+200 (see (b) above) —
  this is the bridge holding the 0x82BFxxxx text-field object the A-press ultimately appends into.

### Virtual-method resolution status (blocked offline)

Resolving `[vptr+N]` slots statically requires the guest vtables (.rdata), which are not in
the port (only generated recomp sources, native src, build dirs, patches, res). Slots observed
in the hot path: +4, +20, +48, +52, +56, +64, +72, +80, +148, +152. Slot **+20** on the
object at global **0x8317B86C** fills `[widget+200]` (text ptr); slot **+52** routes
`sub_82C41BF0` → `sub_824C6508`; slot **+4 / +48** are the `sub_824C6A38` state/draw calls.
The A-press insert itself is a vtable-dispatched call to `sub_82BFD248` (slot unknown).

**Runtime probe plan** (no binary needed): in the native module, hook `sub_82BFD248` by
address (it is a real function in the image) and log `r3` (obj), `r4`, `r5` — this both
identifies the calling object instance and the typed char without needing the vtable slot.
If the slot is still wanted, log the caller LR (`ctx.lr` ≈ `0x82BFD250`) and walk back with
one frame of the caller's LR capture, or snapshot `ctr` at the dispatching `bctrl` while
single-skipping frames in `sub_824C6A38`'s case-2 window.

---

## One-level caller chain of `sub_824CAF58`

- Direct: widget-loop callers, e.g. `sub_824C6A38` users; primary:
- `sub_824C6508` (recomp.33.cpp:7872-8132): scale math on screen fields +144..+204, then
  `sub_824CAF58(r31=screen, r4=widget_i, r5=event r26)` at 8065-8082 (case8/9 route).
- Upstream: `sub_82C41BF0` → `[vptr+52]` → `sub_824C6508` (matches the every-frame fact).

## Global constants used (for the report's completeness)

| Global | Use |
|---|---|
| 0x8317B86C | manager object; `[obj+0]` vtable; slot +20 fills `[widget+200]` |
| 0x821C9A98 | alignment switch table (12 cases, sub_824C9228 at 8184-8219) |
| 0x8207A9E8 | string/format constant base (8331) |
| 0x82147F50 | format pool table via lis r9,-32252 +32592 |
| 0x82000000 | f31@+2776, f32@+2784, f26@+10416, f0@+16724, f28@0x82140000-20428 |
| 0x821C???? | case jump tables (sub_824CAF58 at 7929-7939 — page from lis r12,-32179, addi -20540) |

## Suggested native-module integration (port\src\native\wwe12_kbd*.cpp)

```cpp
// On A press:
//   1) read current text: u8 *t = g(u32)(widget_0xA3EE6270 + 200);  // == 0xA3EE62C0
//      len = strlen(t); container = widget; display copy at widget+0x490 (0xA3EE6700)
//   2) append detection: hook 0x82BFD248 — obj=r3, ch=(u8)r4, hi=(u8)r5
//      (apps append into middleware string; widget inline buffer is the authoritative copy)
//   3) to observe a specific widget across frames: hook 0x824CAF58 entry, keep r4, then
//      u32 textPtr = *(u32*)(r4 + 200);
```

## Full file:line index

- sub_824C9228 (text draw; text ptr read + pool indirection) — recomp.194.cpp:7772, 7824, 8127, 8257, 8305, 8597, 9255; bctrls 8446/8827/8859/8891/9041/9100/9173; scratch stbx 8717
- sub_824C60B0 (widget init; text-ptr fill via 0x8317B86C vtable+20) — recomp.128.cpp:7661-7866 (fill 7847-7859)
- sub_824CAF58 (12-case widget dispatcher) — recomp.76.cpp:7878-8496 (disp 7919-7939; case0 call 7992; case2/3 8090; case1 8026; case11→sub_824D3A10; case4/5→sub_824C7180 at 8108; case6/7→sub_824C75E0 8126; case8/9→sub_824C6508 8437)
- sub_824C6508 (240-widget loop → sub_824CAF58) — recomp.33.cpp:7872-8132 (call 8065-8082)
- sub_824C6A38 (A-press/nav case handler; bctrls +48/+4; tagged refs to +192/+200) — recomp.63.cpp:~8167-9100 (bctrls 8972, 9064; std pairs ~8237)
- sub_82576688 (caller of sub_824C60B0; gates via globals, sub_824C5618/sub_824C5458) — recomp.99.cpp:9629-9957 (calls 9878/9904/9930; gates 9906-9908; 9944)
- sub_824CD798 (grid cursor, 112-byte cells, +84/+88/+92, cell label via sub_82875840) — recomp.230.cpp:7927-8259
- sub_824CCEA0 (cell highlight/cursor set) — recomp.46.cpp:7895+; caller with r5=-1 — recomp.34.cpp:8805
- sub_824CE278 (thin wrapper → sub_824CD798) — recomp.51.cpp:7972-7983; caller recomp.108.cpp:8413
- sub_82BFD248 (char-append virtual; obj+284 struct; 2 bytes/char; realloc sub_82BA7058) — recomp.143.cpp:25426-25487
- sub_82C41BF0 → [vptr+52] → sub_824C6508 (per-frame chain) — as verified at runtime
