
local inst, pad = -1, -1
local NAME = "DrawIngameOptions"
local WIN, FONT0 = "OptionWindow", "font000"
local N = 4
local LBL = {"font001_R01", "font001_R02", "font001_R03", "font001_R04"}
local VAL = {"font001_L01", "font001_L02", "font001_L03", "font001_L04"}
local LINE = {"option_line01_01", "option_line01_02", "option_line01_03", "option_line01_04"}
local CUR = {"CorsorSet07_01", "CorsorSet07_02", "CorsorSet07_03", "CorsorSet07_04"}
local MAXV = {4, 1, 1, 1}
local TITLE = 43101
local LABELS = {43120, 43121, 43122, 43123}
local TEXTS = {{43200, 43201, 43202, 43203, 43204}, {43208, 43209}, {43216, 43217}, {43224, 43225}}
local PAGE = 101
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
