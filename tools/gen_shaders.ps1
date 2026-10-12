# Génère port/generated/native/shader_cache.cpp (et shader_keys.cpp), exigés par le rendu natif :
#   1. compile reblue-XenosRecomp (recomp\, cloné et patché par setup.ps1) ;
#   2. extrait les shaders des fichiers du jeu (tools/find_shaders.py --bpe) dans work\shaders\xsc ;
#   3. les convertit avec XenosRecomp (HLSL -> SPIR-V/DXIL) dans shader_cache.cpp ;
#   4. écrit leurs clés secondaires (tools/gen_shader_keys.py) dans shader_keys.cpp.
#
#   ./tools/gen_shaders.ps1 -GameDir C:\chemin\vers\le\contenu\du\disque
#
# -GameDir : le dossier où le disque a été extrait (default.xex, pac\, movies\...). Une image .iso
# ne marche pas ici : il faut les fichiers. Rien de ce qui est produit n'est versionné (.gitignore).
param(
    [Parameter(Mandatory = $true)] [string]$GameDir,
    [switch]$Force   # refaire l'extraction même si work\shaders\xsc existe déjà
)
$ErrorActionPreference = "Stop"
$Root = Split-Path $PSScriptRoot -Parent
. (Join-Path $PSScriptRoot "env.ps1")

$GameDir = (Resolve-Path $GameDir).Path
if (-not (Test-Path (Join-Path $GameDir "default.xex"))) {
    throw "$GameDir ne contient pas default.xex : indique le dossier du disque extrait"
}
# (pas d'opérateur ?? : Windows PowerShell 5.1)
$python = Get-Command python -ErrorAction SilentlyContinue
if (-not $python) { $python = Get-Command py -ErrorAction SilentlyContinue }
if (-not $python) { throw "Python introuvable dans le PATH" }

# 1. XenosRecomp, version re:Blue (REBLUE_RECOMP : en-tête et constantes du rendu natif)
$Xr = Join-Path $Root "recomp\reblue-XenosRecomp"
if (-not (Test-Path $Xr)) { throw "recomp\reblue-XenosRecomp absent : lance d'abord ./setup.ps1" }
$XrBuild = Join-Path $Xr "build"
$XrExe = Join-Path $XrBuild "XenosRecomp\XenosRecomp.exe"
if (-not (Test-Path $XrExe)) {
    Write-Host "== Compilation de XenosRecomp"
    # Par CXXFLAGS (ajouté aux options par défaut, /EHsc compris) plutôt que CMAKE_CXX_FLAGS,
    # qui les remplacerait. Sans REBLUE_RECOMP, les corrections WWE '12 du patch sont ignorées.
    $env:CXXFLAGS = "-DREBLUE_RECOMP"
    try {
        Invoke-Checked cmake -S $Xr -B $XrBuild -G Ninja -DCMAKE_BUILD_TYPE=Release -Wno-dev
    } finally { Remove-Item Env:CXXFLAGS -ErrorAction SilentlyContinue }
    Invoke-Checked cmake --build $XrBuild --target XenosRecomp
}

# 2. Extraction des conteneurs .xsc (les archives .pac de Yuke's sont compressées en BPE)
$Xsc = Join-Path $Root "work\shaders\xsc"
if ($Force -or -not (Test-Path $Xsc) -or -not (Get-ChildItem $Xsc -Filter *.xsc -ErrorAction SilentlyContinue)) {
    Write-Host "== Extraction des shaders de $GameDir (quelques minutes)"
    New-Item -ItemType Directory -Force $Xsc | Out-Null
    Invoke-Checked $python.Source (Join-Path $PSScriptRoot "find_shaders.py") $GameDir --bpe --extract $Xsc
}
$count = (Get-ChildItem $Xsc -Filter *.xsc).Count
if ($count -eq 0) { throw "aucun shader trouvé dans $GameDir" }
Write-Host "$count shaders extraits"

# 3. Conversion -> shader_cache.cpp
$Gen = Join-Path $Root "port\generated\native"
New-Item -ItemType Directory -Force $Gen | Out-Null
Write-Host "== Conversion des shaders avec XenosRecomp"
Invoke-Checked $XrExe $Xsc (Join-Path $Gen "shader_cache.cpp") (Join-Path $Xr "XenosRecomp\shader_common.h")

# 4. Clés secondaires -> shader_keys.cpp (module Python xxhash)
& $python.Source -c "import xxhash" 2>$null
if ($LASTEXITCODE -ne 0) { Invoke-Checked $python.Source -m pip install --user xxhash }
Invoke-Checked $python.Source (Join-Path $PSScriptRoot "gen_shader_keys.py") $Xsc (Join-Path $Gen "shader_keys.cpp")

Write-Host "Shaders prêts. Lance ./build.ps1 -Config release"
