"""Builds the BALANCING > DAMAGE page with the match modes' rows (port/src/native/wwe12_damage_page.cpp).

The game's DAMAGE page ("/MENS/DMGB" of MScript.pac, frame "DrawDamage" of the layout LOPN) has two
sliders; no frame of LOPN has more slider rows than DrawAIReversal (6, AI REVERSAL's) apart from
GENERAL's 9 (all used). So this page is the game's DAMAGE script rewritten (same calls, same states,
read with tools_local's disassembly on 2026-11-15) on the frame DrawAIReversal: rows 1-2 the game's
two damage sliders (yOption.InitDamageBalance / UpdateDamageBalance, values / 5 as the game's), rows
3-5 ours: SQUASH CHANCE (0-100 % by 5), SQUASH HEALTH (5-90 % by 5), STARTING FINISHERS (game's
rules, 1, 2, 3, infinite). The 6th row of the frame is hidden.

Our rows' values: yOption.InitInGameOptions(130) / UpdateInGameOptions(130) (hooked: the exe fills /
reads the page array's words 20-22 = values, 23-25 = maxima). Labels: fixed text ids 44100-44104,
served by the exe (rows 3-5 show their value, refreshed on each change).

luac: the WWE '12 build described in tools/graphics_pages.py.
Usage: python -I tools/damage_page.py <luac_wwe12.exe>
"""
import os
import subprocess
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
SRC = os.path.join(HERE, '..', 'port', 'src', 'native', 'graphics_pages', 'damage.lua')
INC = os.path.join(HERE, '..', 'port', 'src', 'native', 'wwe12_damage_page.inc')

LUA = r'''
local inst, pad = -1, -1
local NAME = "DrawAIReversal"
local WIN, FONT0 = "OptionWindow", "font000"
local WARN, TITLE = 51121, 41143
local N, GN, OURS, PAGE = 5, 2, 20, 130
local LBL = {"font001_R01", "font001_R02", "font001_R03", "font001_R04", "font001_R05"}
local LINE = {"option_line01_01", "option_line01_02", "option_line01_03", "option_line01_04", "option_line01_05"}
local CUR = {"CorsorSet04_01", "CorsorSet04_02", "CorsorSet04_03", "CorsorSet04_04", "CorsorSet04_05"}
local GAUGE = {"AI_gauge_01", "AI_gauge_02", "AI_gauge_03", "AI_gauge_04", "AI_gauge_05"}
local SIXTH = {"font001_R06", "option_line01_06", "CorsorSet04_06", "AI_gauge_06"}
local PARTS = {"slider_base_L", "slider_base_C", "slider_base_R", "slider_bar_L", "slider_bar_C", "slider_bar_R",
               "slider_waku_L", "slider_waku_C", "slider_waku_R"}
local LABELS = {44103, 44104, 44100, 44101, 44102}
local val, en, mx, bak = {}, {}, {}, {}
local ST_LOAD, ST_FADEIN, ST_RUN, ST_OUT, ST_WARN = 0, 1, 2, 3, 6
local state = ST_LOAD
local show = 1

local function Gauge(i)
  local g = GAUGE[i]
  local v = val[i]
  if v == 0 then
    yFrame.SetLayoutHide(g, "slider_bar_L", true)
    yFrame.SetLayoutHide(g, "slider_bar_C", true)
    yFrame.SetLayoutHide(g, "slider_bar_R", true)
    return
  end
  yFrame.SetLayoutHide(g, "slider_bar_L", false)
  yFrame.SetLayoutHide(g, "slider_bar_C", false)
  if v == mx[i] then
    yFrame.SetLayoutHide(g, "slider_bar_R", false)
  else
    yFrame.SetLayoutHide(g, "slider_bar_R", true)
  end
  yFrame.SetLayoutUV(g, "slider_bar_C", 8, 0, v * 112 / mx[i], 8)
  yFrame.SetLayoutWH(g, "slider_bar_C", v * 86 / mx[i], 8)
end

local function Row(i, off)
  yFrame.SetLayoutHide(NAME, LINE[i], off)
  yFrame.SetLayoutHide(NAME, CUR[i], off)
  local c = 255
  if off == true then c = 85 end
  for k = 1, 9 do
    yFrame.SetLayoutColorRateD(GAUGE[i], PARTS[k], c, c, c, 255)
  end
end

local function Cursor()
  local c = yFrame.GetCursorV() + 1
  for i = 1, N do
    if i == c then Row(i, false) else Row(i, true) end
  end
end

local function Ours(i)
  if GN < i then
    yFrame.Set32BitArray(OURS + i - GN - 1, val[i])
    yFrame.SetLayoutStr(NAME, LBL[i], LABELS[i])
  end
end

local function Cross()
  local l = yFrame.IsCrossLeft()
  local r = yFrame.IsCrossRight()
  if l or r then
    local c = yFrame.GetCursorV() + 1
    if l == true then
      if 0 < val[c] then
        val[c] = val[c] - 1
        yFrame.PlaySe_NoCheck(2)
      end
    elseif r == true then
      if val[c] < mx[c] then
        val[c] = val[c] + 1
        yFrame.PlaySe_NoCheck(3)
      end
    end
    Gauge(c)
    Ours(c)
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
  yFrame.SetItemH(0, 1, true)
  yFrame.SetItemV(0, N, true)
  yFrame.SetSeFlag(7)
  yOption.InitDamageBalance()
  for i = 1, GN do
    val[i] = yFrame.Get32BitArray(i - 1) / 5
    en[i] = yFrame.Get32BitArray(i - 1 + GN)
    mx[i] = yFrame.Get32BitArray(i - 1 + GN * 2) / 5
  end
  yOption.InitInGameOptions(PAGE)
  for i = GN + 1, N do
    val[i] = yFrame.Get32BitArray(OURS + i - GN - 1)
    en[i] = 1
    mx[i] = yFrame.Get32BitArray(OURS + 3 + i - GN - 1)
  end
  for i = 1, N do
    bak[i] = val[i]
    local col = 0
    if en[i] == 0 then col = 2 end
    yFrame.SetLayoutColor(NAME, LBL[i], col)
    yFrame.SetLayoutStr(NAME, LBL[i], LABELS[i])
    Gauge(i)
  end
  for k = 1, 4 do
    yFrame.SetLayoutHide(NAME, SIXTH[k], true)
  end
  Cursor()
  yFrame.SetLayoutStr(WIN, FONT0, TITLE)
  yFrame.ResetSeFlag(2)
  yFrame.ResetSeFlag(3)
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
    if not yFrame.IsFadeEnd() then return 0 end
    yFrame.PadActive()
    show = 3
    state = ST_RUN
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
      yFrame.SetLayoutColor(NAME, LBL[i], 2)
    elseif c == i then
      yFrame.SetLayoutColorD(NAME, LBL[i], 185, 0, 0, 255)
    else
      yFrame.SetLayoutColor(NAME, LBL[i], 0)
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
  for i = 1, GN do
    yFrame.Set32BitArray(i - 1, val[i] * 5)
  end
  yOption.UpdateDamageBalance()
  for i = GN + 1, N do
    yFrame.Set32BitArray(OURS + i - GN - 1, val[i])
  end
  yOption.UpdateInGameOptions(PAGE)
  yFrame.PadPause()
  yFrame.FadeOut()
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
    yMenu.MakeWarningYesNo(WARN)
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
    Gauge(i)
    Ours(i)
  end
  if changed == true then yFrame.PlaySe_NoCheck(7) end
end
'''


def main():
    luac = sys.argv[1]
    os.makedirs(os.path.dirname(SRC), exist_ok=True)
    with open(SRC, 'w', newline='\n') as f:
        f.write(LUA)
    binary = SRC + '.out'
    subprocess.run([luac, '-s', '-o', binary, SRC], check=True)
    data = open(binary, 'rb').read()
    os.remove(binary)
    out = ['// Generated by tools/damage_page.py: the BALANCING > DAMAGE page with the match modes\' rows',
           '// (compiled Lua 5.1, float numbers), our own code. Do not edit; edit and rerun the script.', '',
           'constexpr uint8_t kDamagePage[] = {']
    for i in range(0, len(data), 16):
        out.append('    ' + ', '.join('0x%02X' % b for b in data[i:i + 16]) + ',')
    out += ['};', '']
    with open(INC, 'w', newline='\n') as f:
        f.write('\n'.join(out))
    print('wrote', os.path.normpath(INC), len(data), 'bytes')


if __name__ == '__main__':
    main()
