"""Builds the OPTIONS > GRAPHICS pages (wwe12_graphics_native, port/src/native/wwe12_graphics_menu.cpp).

The game's option pages are Lua 5.1 scripts compiled with 4-byte float numbers and a 4-byte size_t
(header 1B 4C 75 61 51 00 01 04 04 04 04 00). Its Lua can only load compiled scripts (its source
parser crashes), so the pages are written here as Lua source, compiled with a luac built for that
format, and embedded in the exe as byte arrays.

The scripts follow the game's IN-GAME OPTIONS script ("/MENS/0014" of MScript.pac): same calls,
same states. Texts are fixed ids, served in the player's language by the exe:
  title of tab t           43100 + t
  label of row r of tab t  43110 + t * 10 + r
  value v of row r         43150 + t * 50 + r * 8 + v
The rows (and their number of values) must match BuildSettings() in wwe12_graphics_menu.cpp; the
exe checks the counts before using a page.

luac for that format: Lua 5.1.5 (lua.org) with, in luaconf.h, LUA_NUMBER float (LUA_NUMBER_SCAN
"%f", LUA_NUMBER_FMT "%.7g", lua_str2number strtof), DumpString in ldump.c writing the size as an
unsigned int and luaU_header in lundump.c writing 4 for sizeof(size_t); built with
`clang -O1 -D_CRT_SECURE_NO_WARNINGS -o luac_wwe12.exe <src/*.c except lua.c>`.

The OPTIONS > JUKEBOX page (wwe12_jukebox.cpp) is built from the same script: 4 rows, and the
values' texts are a base id per row + the value (the themes' row
has as many values as the game has themes, its maximum comes from the exe):
  title 43103, label of row r 43400 + r, value v of row r JUKEBOX_VALUES[r] + v

Usage: python -I tools/graphics_pages.py <luac_wwe12.exe>
"""
import os
import subprocess
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
OUT_DIR = os.path.join(HERE, '..', 'port', 'src', 'native', 'graphics_pages')
INC = os.path.join(HERE, '..', 'port', 'src', 'native', 'wwe12_graphics_pages.inc')
JUKEBOX_INC = os.path.join(HERE, '..', 'port', 'src', 'native', 'wwe12_jukebox_page.inc')
JUKEBOX_VALUES = [43410, 43420, 44000, 43430]  # JUKEBOX off/on, ORDER, THEME, GAME MENU MUSIC no/yes

# Number of values of each row, per tab (DISPLAY, GRAPHICS, ADVANCED), in BuildSettings() order.
TABS = [
    [3, 6, 8, 2, 2],  # display mode, window size, internal resolution, frame limit, vsync
    [5, 2, 2, 2],     # texture sharpness, crowd, shadows, depth of field
    [3, 3, 2, 2, 2],  # graphics API, CPU priority, 60 fps entrances, EXIT entry, FPS counter
]

TEMPLATE = r'''
local inst, pad = -1, -1
local NAME = "%(frame)s"
local WIN, FONT0 = "OptionWindow", "font000"
local N = %(n)d
local LBL = {%(lbl)s}
local VAL = {%(val)s}
local LINE = {%(line)s}
local CUR = {%(cur)s}
local MAXV = {%(maxv)s}
local TITLE = %(title)d
local LABELS = {%(labels)s}
local TEXTS = {%(texts)s}
local PAGE = %(page)d
local val, en, mx, bak = {}, {}, {}, {}
local ST_LOAD, ST_FADEIN, ST_RUN, ST_OUT, ST_WARN = 0, 1, 2, 3, 6
local state = ST_LOAD
local show = 1
-- The rows' colours, as the game's RTWM OPTIONS script ("/MENS/0015") sets them: COL_WHITE and
-- COL_GRAY come from yMenu.OptLib(); 0 / 2 (the labels' normal / disabled colours) otherwise.
local function White() return COL_WHITE or 0 end
local function Gray() return COL_GRAY or 2 end

local function Cursor()
  local c = yFrame.GetCursorV() + 1
  for i = 1, N do
    yFrame.SetLayoutHide(NAME, LINE[i], true)
    yFrame.SetLayoutHide(NAME, CUR[i], true)
    if i == c then
      yFrame.SetLayoutHide(NAME, LINE[i], false)
      yFrame.SetLayoutHide(NAME, CUR[i], false)
    end
  end
end

local function Cross()
  local l = yFrame.IsCrossLeft()
  local r = yFrame.IsCrossRight()
  if l or r then
    local c = yFrame.GetCursorV() + 1
    if l == true then
      val[c] = val[c] - 1
      if val[c] < 0 then val[c] = MAXV[c] end
    elseif r == true then
      val[c] = val[c] + 1
      if MAXV[c] < val[c] then val[c] = 0 end
    end
    yFrame.SetLayoutStr(NAME, VAL[c], TEXTS[c][val[c] + 1])
  end
  local u = yFrame.IsCrossUp()
  local d = yFrame.IsCrossDown()
  if u or d then
    Cursor()
    yFrame.ResetBlink(0)
  end
end

local function Setup()
  yFrame.SetPadNo(yOption.GetEnablePadNo())
  yFrame.SetStickType(4)
  yFrame.SetItemV(0, N, true)
  yFrame.SetItemH(0, 1, true)
  yFrame.SetSeFlag(7)
  yOption.InitInGameOptions(PAGE)
  for i = 1, N do
    val[i] = yFrame.Get32BitArray(i - 1)
    en[i] = yFrame.Get32BitArray(i - 1 + N)
    mx[i] = yFrame.Get32BitArray(i - 1 + N * 2)
    bak[i] = val[i]
  end
  for i = 1, N do
    sColNo = 0
    if en[i] == 0 then sColNo = 2 end
    yFrame.SetLayoutColor(NAME, LBL[i], sColNo)
    yFrame.SetLayoutColor(NAME, VAL[i], sColNo)
  end
  Cursor()
  yFrame.SetLayoutStr(WIN, FONT0, TITLE)
  for i = 1, N do
    yFrame.SetLayoutStr(NAME, LBL[i], LABELS[i])
    yFrame.SetLayoutStr(NAME, VAL[i], TEXTS[i][val[i] + 1])
  end
  yFrame.ResetSeFlag(7)
end

function Init(a, b)
  inst = a
  pad = b
  yFrame.SetInstanceNo(inst)
  yFrame.LayoutLoad("LOPN")
  yFrame.MakeBlink(1)
  yMenu.OptLib()
end

function Action()
  if state == ST_LOAD then
    if not yFrame.IsLayoutLoadEnd() then return 0 end
    Setup()
    yFrame.FadeIn()
    yMenu.SetHelp(20, 10, 1)
    state = ST_FADEIN
  elseif state == ST_FADEIN then
    if yFrame.IsFadeEnd() then
      yFrame.PadActive()
      show = 3
      state = ST_RUN
    end
  elseif state == ST_OUT then
    if not yFrame.IsFadeEnd() then return 0 end
    return 1
  elseif state == ST_WARN then
    if yMenu.IsWarningYes() == true then
      yFrame.PadPause()
      yFrame.FadeOut()
      show = 2
      state = ST_OUT
    else
      state = ST_RUN
    end
  end
  local c = yFrame.GetCursorV() + 1
  for i = 1, N do
    if en[i] == 0 then
      yFrame.SetLayoutColor(NAME, LINE[i], Gray())
      yFrame.SetLayoutColor(NAME, VAL[i], Gray())
    elseif c == i then
      yFrame.SetLayoutColorD(NAME, LINE[i], 185, 0, 0, 255)
      yFrame.SetLayoutColorD(NAME, VAL[i], 185, 0, 0, 255)
    else
      yFrame.SetLayoutColor(NAME, LINE[i], White())
      yFrame.SetLayoutColor(NAME, VAL[i], White())
    end
  end
  yFrame.SetBlinkAlpha(0, NAME, CUR[c])
  return 0
end

function Draw()
  if yFrame.IsLayoutLoadEnd() then
    yFrame.LayoutShow(NAME, show)
  end
end

function FromDecision()
  yFrame.PadPause()
  yFrame.FadeOut()
  for i = 1, N do
    yFrame.Set32BitArray(i - 1, val[i])
  end
  yOption.UpdateInGameOptions(PAGE)
  show = 2
  state = ST_OUT
end

function FromCancel()
  if state == ST_WARN then return end
  local changed = false
  for i = 1, N do
    if val[i] ~= bak[i] then
      changed = true
      break
    end
  end
  if changed == true then
    yMenu.MakeWarningYesNo(51121)
    state = ST_WARN
    return
  end
  yFrame.PadPause()
  yFrame.FadeOut()
  show = 2
  state = ST_OUT
end

function FromCross()
  Cross()
end

function FromSub2()
  local changed = false
  for i = 1, N do
    if val[i] ~= bak[i] then changed = true end
    val[i] = bak[i]
    yFrame.SetLayoutStr(NAME, VAL[i], TEXTS[i][val[i] + 1])
  end
  if changed == true then yFrame.PlaySe_NoCheck(7) end
end
'''


def page_source(tab, counts):
    n = len(counts)
    five = n > 4
    cursor = 'CorsorSet06_0' if five else 'CorsorSet07_0'
    quoted = lambda fmt: ', '.join('"%s"' % (fmt % (r + 1)) for r in range(n))
    return TEMPLATE % {
        'frame': 'DrawRTWMOptions' if five else 'DrawIngameOptions',
        'n': n,
        'lbl': quoted('font001_R0%d'),
        'val': quoted('font001_L0%d'),
        'line': quoted('option_line01_0%d'),
        'cur': quoted(cursor + '%d'),
        'maxv': ', '.join(str(c - 1) for c in counts),
        'title': 43100 + tab,
        'labels': ', '.join(str(43110 + tab * 10 + r) for r in range(n)),
        'texts': ', '.join('{%s}' % ', '.join(str(43150 + tab * 50 + r * 8 + v) for v in range(c))
                           for r, c in enumerate(counts)),
        'page': 100 + tab,
    }


def jukebox_source():
    """The JUKEBOX page: values' texts = base id + value, maximum from the exe
    (yFrame.Get32BitArray)."""
    n = len(JUKEBOX_VALUES)
    src = TEMPLATE % {
        'frame': 'DrawIngameOptions',
        'n': n,
        'lbl': ', '.join('"font001_R0%d"' % (r + 1) for r in range(n)),
        'val': ', '.join('"font001_L0%d"' % (r + 1) for r in range(n)),
        'line': ', '.join('"option_line01_0%d"' % (r + 1) for r in range(n)),
        'cur': ', '.join('"CorsorSet07_0%d"' % (r + 1) for r in range(n)),
        'maxv': ', '.join('0' for _ in range(n)),
        'title': 43103,
        'labels': ', '.join(str(43400 + r) for r in range(n)),
        'texts': ', '.join(str(b) for b in JUKEBOX_VALUES),
        'page': 103,
    }
    for old, new in (
            ('local TEXTS = {', 'local VBASE = {'),
            ('TEXTS[c][val[c] + 1]', 'VBASE[c] + val[c]'),
            ('TEXTS[i][val[i] + 1]', 'VBASE[i] + val[i]'),
            ('if val[c] < 0 then val[c] = MAXV[c] end', 'if val[c] < 0 then val[c] = mx[c] end'),
            ('if MAXV[c] < val[c] then val[c] = 0 end', 'if mx[c] < val[c] then val[c] = 0 end')):
        assert old in src, old
        src = src.replace(old, new)
    return src


def compile_lua(luac, src_path, source):
    with open(src_path, 'w', newline='\n') as f:
        f.write(source)
    binary = src_path + '.out'
    subprocess.run([luac, '-s', '-o', binary, src_path], check=True)
    data = open(binary, 'rb').read()
    os.remove(binary)
    return data


def byte_lines(name, data):
    out = ['constexpr uint8_t %s[] = {' % name]
    for i in range(0, len(data), 16):
        out.append('    ' + ', '.join('0x%02X' % b for b in data[i:i + 16]) + ',')
    out.append('};')
    return out


def main():
    luac = sys.argv[1]
    os.makedirs(OUT_DIR, exist_ok=True)
    out = ['// Generated by tools/graphics_pages.py: the OPTIONS > GRAPHICS page scripts (compiled Lua',
           '// 5.1, float numbers), our own code. Do not edit; edit and rerun the script instead.', '']
    for tab, counts in enumerate(TABS):
        src = os.path.join(OUT_DIR, 'page%d.lua' % tab)
        with open(src, 'w', newline='\n') as f:
            f.write(page_source(tab, counts))
        binary = src + '.out'
        subprocess.run([luac, '-s', '-o', binary, src], check=True)
        data = open(binary, 'rb').read()
        os.remove(binary)
        out.append('constexpr uint8_t kGraphicsPage%d[] = {' % tab)
        for i in range(0, len(data), 16):
            out.append('    ' + ', '.join('0x%02X' % b for b in data[i:i + 16]) + ',')
        out.append('};')
        out.append('constexpr int kGraphicsPage%dValues[] = {%s};' % (tab, ', '.join(map(str, counts))))
        out.append('')
    with open(INC, 'w', newline='\n') as f:
        f.write('\n'.join(out))
    print('wrote', os.path.normpath(INC))
    data = compile_lua(luac, os.path.join(OUT_DIR, 'jukebox.lua'), jukebox_source())
    out = ['// Generated by tools/graphics_pages.py: the OPTIONS > JUKEBOX page script (compiled Lua 5.1,',
           '// float numbers), our own code. Do not edit; edit and rerun the script instead.', '']
    out += byte_lines('kJukeboxPage', data)
    out.append('constexpr int kJukeboxPageRows = %d;' % len(JUKEBOX_VALUES))
    out.append('')
    with open(JUKEBOX_INC, 'w', newline='\n') as f:
        f.write('\n'.join(out))
    print('wrote', os.path.normpath(JUKEBOX_INC))


if __name__ == '__main__':
    main()
