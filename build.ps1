# Compile le portage. -Codegen relance d'abord la traduction PPC -> C++.
param(
    [switch]$Codegen,
    [ValidateSet("debug", "release", "relwithdebinfo")] [string]$Config = "debug"
)
$ErrorActionPreference = "Stop"
$Port = Join-Path $PSScriptRoot "port"
$Preset = "win-amd64-$Config"
$Rexglue = Join-Path $PSScriptRoot "recomp\rexglue-sdk\out\install\win-amd64\bin\rexglue.exe"

if (-not (Test-Path (Join-Path $Port "assets\default.xex"))) {
    throw "port\assets\default.xex manquant (à extraire de ta copie Xbox 360 du jeu)"
}
. (Join-Path $PSScriptRoot "tools\env.ps1")

Push-Location $Port
try {
    if ($Codegen -or -not (Test-Path "generated\rexglue.cmake")) {
        $out = cmd /c "`"$Rexglue`" codegen wwe12_manifest.toml 2>&1"
        $code = $LASTEXITCODE
        $out | ForEach-Object { Write-Host $_ }
        if ($code -ne 0) { throw "échec du codegen ($code)" }
        # Un saut non résolu compile, mais plante le jeu s'il est exécuté (« Unresolved branch »).
        $unresolved = @($out | Select-String "Unresolved (conditional )?branch" | Sort-Object -Unique)
        if ($unresolved.Count) {
            Write-Warning "$($unresolved.Count) saut(s) non résolu(s) : une fonction de wwe12_config.toml coupe sans doute une vraie fonction (voir tools/find_fptrs.py --check-config, ou fixer sa taille avec { size = 0x... })"
            $unresolved | ForEach-Object { Write-Warning "  $($_.Line)" }
        }
    }
    Invoke-Checked cmake --preset $Preset
    Invoke-Checked cmake --build --preset $Preset
} finally { Pop-Location }
