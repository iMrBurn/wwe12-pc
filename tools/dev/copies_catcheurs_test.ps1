# Test "how many added superstars fit" (1.4): N copies of one installed added superstar mod (NEW SUPERSTAR, in the
# dev build's mods\.svrmods\Superstars), named "TEST COPY 001"..., to load the game with many added superstars.
# Only for the test copy of the save (tools\dev\lancer_test_catcheurs.bat, Documents\wwe12_test).
#   powershell -ExecutionPolicy Bypass -File tools\dev\copies_catcheurs_test.ps1 -N 60
#   powershell -ExecutionPolicy Bypass -File tools\dev\copies_catcheurs_test.ps1 -Remove
# The copies share the source's model, portraits and theme (hard links: no disk space); their entrance, victory
# scene and videos are left out (the base's are used), so they cost what an added superstar costs in memory.
# -Source: the mod folder to copy (default: the first that isn't a copy).
param(
  [int]$N = 0,
  [string]$Source = "",
  [switch]$Remove
)
$ErrorActionPreference = "Stop"
$root = Split-Path -Parent (Split-Path -Parent $PSScriptRoot)
$stars = Join-Path $root "port\out\build\win-amd64-release\mods\.svrmods\Superstars"
if (-not (Test-Path $stars)) { Write-Host "No added superstars folder: $stars"; exit 1 }

$old = Get-ChildItem $stars -Directory | Where-Object { $_.Name -like "zz_test_copy_*" }
if ($old) {
  $old | Remove-Item -Recurse -Force
  Write-Host ("{0} old test copies removed." -f $old.Count)
}
if ($Remove) { exit 0 }
if ($N -le 0) { Write-Host "Give -N <count> (or -Remove)."; exit 1 }
if ($N -gt 180) { Write-Host "At most 180 (the game has 193 free superstar numbers, some are taken)."; exit 1 }

$src = if ($Source) { Get-Item (Join-Path $stars $Source) } else {
  Get-ChildItem $stars -Directory | Where-Object { $_.Name -notlike "zz_test_copy_*" -and -not (Test-Path (Join-Path $_.FullName "disabled")) } | Select-Object -First 1
}
if (-not $src -or -not (Test-Path (Join-Path $src.FullName "manifest.txt"))) { Write-Host "No added superstar mod to copy."; exit 1 }
Write-Host ("Copying {0} x {1}..." -f $N, $src.Name)

$manifest = Get-Content (Join-Path $src.FullName "manifest.txt") -Encoding UTF8
$keep = @("SSFA_HD.dds", "SSFA_SD.dds", "SSFB_HD.dds", "SSFB_SD.dds", "SSFC_HD.dds", "SSFC_SD.dds", "ch.pac", "theme.wem")
for ($i = 1; $i -le $N; $i++) {
  $tag = "{0:000}" -f $i
  $dest = Join-Path $stars ("zz_test_copy_" + $tag)
  New-Item -ItemType Directory -Path (Join-Path $dest "superstar") -Force | Out-Null
  $lines = foreach ($l in $manifest) {
    if ($l -like "id=*") { "id=zz_test_copy_$tag" }
    elseif ($l -like "name=*") { "name=TEST COPY $tag" }
    elseif ($l -like "entrance_own=*" -or $l -like "entrance_champ=*") { }
    else { $l }
  }
  [IO.File]::WriteAllLines((Join-Path $dest "manifest.txt"), [string[]]$lines, (New-Object Text.UTF8Encoding $false))  # (no BOM)
  foreach ($f in $keep) {
    $from = Join-Path $src.FullName ("superstar\" + $f)
    if (Test-Path $from) { New-Item -ItemType HardLink -Path (Join-Path $dest ("superstar\" + $f)) -Target $from | Out-Null }
  }
}
Write-Host ("{0} test copies made. Start the game with tools\dev\lancer_test_catcheurs.bat." -f $N)
