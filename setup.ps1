# Installe le SDK ReXGlue dans recomp/ et génère le projet port/ avec `rexglue init`.
param(
    [string]$SdkRef = "c94f5eb"   # commit testé (v0.10.0.2-dev) ; nos patches s'appliquent dessus
)
$ErrorActionPreference = "Stop"
$Root = $PSScriptRoot
$Sdk  = Join-Path $Root "recomp\rexglue-sdk"

foreach ($tool in "git", "cmake", "ninja") {
    if (-not (Get-Command $tool -ErrorAction SilentlyContinue)) { throw "$tool introuvable dans le PATH" }
}
. (Join-Path $Root "tools\env.ps1")

# Sans le mode développeur de Windows, git écrit les liens symboliques comme de petits
# fichiers texte contenant le chemin cible : on les remplace par une copie de la cible.
function Repair-GitSymlinks {
    $repos = @(".") + @(git submodule foreach --recursive --quiet 'echo $displaypath' |
        Where-Object { $_ -notlike "*moltenvk*" })  # MoltenVK ne sert que sur macOS
    foreach ($pass in 1..3) {  # plusieurs passes : un lien peut viser un autre lien
        foreach ($repo in $repos) {
            $links = git -C $repo ls-files -s | Where-Object { $_ -like "120000*" }
            foreach ($entry in $links) {
                $rel = ($entry -split "`t", 2)[1]
                $link = Join-Path (Resolve-Path $repo) $rel
                $dest = (git -C $repo cat-file -p ($entry -split " ")[1]) -join "`n"
                if (-not (Test-Path $link -PathType Leaf) -or
                    (Get-Content $link -Raw).Trim() -ne $dest.Trim()) { continue }  # déjà réparé
                $target = Join-Path (Split-Path $link) $dest.Trim()
                if (-not (Test-Path $target)) {
                    if ($pass -eq 3) { Write-Warning "cible introuvable : $repo/$rel" }
                    continue
                }
                Remove-Item $link -Force
                Copy-Item $target $link -Recurse -Force
                git -C $repo update-index --assume-unchanged $rel
            }
        }
    }
}

if (-not (Test-Path $Sdk)) {
    Invoke-Checked git clone --recursive https://github.com/rexglue/rexglue-sdk.git $Sdk
}
Push-Location $Sdk
try {
    Invoke-Checked git fetch --all
    Invoke-Checked git checkout $SdkRef
    Invoke-Checked git submodule update --init --recursive
    Repair-GitSymlinks
    # Appliquer les correctifs SDK (port/patches/rexglue-sdk-*.patch ; rexglue-sdk-reblue.patch
    # vient des portages SvR de nefariousjosiah, GPL-3.0)
    Get-ChildItem (Join-Path $Root "port\patches") -Filter "rexglue-sdk-*.patch" -ErrorAction SilentlyContinue |
        ForEach-Object {
            cmd /c "git apply --reverse --check `"$($_.FullName)`" 2>nul"
            if ($LASTEXITCODE -eq 0) { return }  # déjà appliqué
            Invoke-Checked git apply --whitespace=nowarn $_.FullName
        }
    # Vulkan en plus de D3D12 : sur les jeux WWE de ce moteur, D3D12 bloque le GPU en match
    # (DEVICE_HUNG), Vulkan est la configuration qui marche sous Xenia (cf. WWE 2K14).
    Invoke-Checked cmake --preset win-amd64 -DREXGLUE_USE_VULKAN=ON
    Invoke-Checked cmake --build out/build/win-amd64 --target install
} finally { Pop-Location }

# Dépendances du rendu natif (repris de re:Blue via les portages SvR de nefariousjosiah),
# épinglées sur les mêmes commits que lui pour que ses patches s'appliquent.
function Get-PinnedRepo($Url, $Dir, $Commit, $Patch) {
    if (-not (Test-Path $Dir)) { Invoke-Checked git clone $Url $Dir }
    Invoke-Checked git -C $Dir fetch --all
    Invoke-Checked git -C $Dir checkout $Commit
    Invoke-Checked git -C $Dir submodule update --init --recursive
    cmd /c "git -C `"$Dir`" apply --reverse --check `"$Patch`" 2>nul"
    if ($LASTEXITCODE -ne 0) { Invoke-Checked git -C $Dir apply --whitespace=nowarn $Patch }
}
Get-PinnedRepo "https://github.com/zolaware/plume.git" (Join-Path $Root "recomp\plume") `
    "e0c8871" (Join-Path $Root "port\patches\plume-reblue.patch")
# Par-dessus : fonctions de Vulkan 1.2 reconnues sans leur ancien nom d'extension (Android).
$plumeAndroid = Join-Path $Root "port\patches\plume-android.patch"
cmd /c "git -C `"$(Join-Path $Root 'recomp\plume')`" apply --reverse --check `"$plumeAndroid`" 2>nul"
if ($LASTEXITCODE -ne 0) { Invoke-Checked git -C (Join-Path $Root "recomp\plume") apply --whitespace=nowarn $plumeAndroid }
# Puis : pilote Vulkan personnalisé (Turnip) choisi dans le launcher Android (port/src/android/wwe12_driver.cpp).
$plumeDriver = Join-Path $Root "port\patches\plume-android-driver.patch"
cmd /c "git -C `"$(Join-Path $Root 'recomp\plume')`" apply --reverse --check `"$plumeDriver`" 2>nul"
if ($LASTEXITCODE -ne 0) { Invoke-Checked git -C (Join-Path $Root "recomp\plume") apply --whitespace=nowarn $plumeDriver }
# Puis : cache de pipelines Vulkan gardé sur disque (Android ; port/src/android/wwe12_android.cpp le sauvegarde).
$plumeCache = Join-Path $Root "port\patches\plume-android-cache.patch"
cmd /c "git -C `"$(Join-Path $Root 'recomp\plume')`" apply --reverse --check `"$plumeCache`" 2>nul"
if ($LASTEXITCODE -ne 0) { Invoke-Checked git -C (Join-Path $Root "recomp\plume") apply --whitespace=nowarn $plumeCache }
# Puis : D3D12 (wwe12_d3d12.exe), copie texture -> tampon des read-backs sans texture de destination.
$plumeD3d12 = Join-Path $Root "port\patches\plume-d3d12.patch"
cmd /c "git -C `"$(Join-Path $Root 'recomp\plume')`" apply --reverse --check `"$plumeD3d12`" 2>nul"
if ($LASTEXITCODE -ne 0) { Invoke-Checked git -C (Join-Path $Root "recomp\plume") apply --whitespace=nowarn $plumeD3d12 }
# Puis : Direct3D 11 (wwe12_d3d11.exe, en cours) : le backend D3D11 du portage SvR 2011 de Kaiko
# (KaikoClanworth1, adapté avec son accord) + swap chain, carte graphique, constantes et textures par
# emplacement pour notre rendu.
$plumeD3d11 = Join-Path $Root "port\patches\plume-d3d11.patch"
cmd /c "git -C `"$(Join-Path $Root 'recomp\plume')`" apply --reverse --check `"$plumeD3d11`" 2>nul"
if ($LASTEXITCODE -ne 0) { Invoke-Checked git -C (Join-Path $Root "recomp\plume") apply --whitespace=nowarn $plumeD3d11 }
Get-PinnedRepo "https://github.com/zolaware/reblue-XenosRecomp.git" `
    (Join-Path $Root "recomp\reblue-XenosRecomp") "339af41" `
    (Join-Path $Root "port\patches\xenosrecomp-reblue-wwe12.patch")  # patch reblue + adaptations WWE '12

$rexglue = Get-ChildItem $Sdk -Recurse -Filter "rexglue.exe" | Select-Object -First 1
if (-not $rexglue) { throw "rexglue.exe introuvable après la compilation du SDK" }

$Port = Join-Path $Root "port"
New-Item -ItemType Directory -Force (Join-Path $Port "assets") | Out-Null
if (-not (Test-Path (Join-Path $Port "assets\default.xex"))) {
    # `rexglue init` lit le XEX pour créer le projet : impossible sans lui.
    Write-Host "SDK prêt. Copie le default.xex de ton disque WWE '12 Xbox 360 dans port\assets\ puis relance ./setup.ps1"
    return
}
if (-not (Test-Path (Join-Path $Port "CMakeLists.txt"))) {
    Push-Location $Port
    try {
        Invoke-Checked $rexglue.FullName init --project-name "wwe12" --xex-path "assets/default.xex" `
            --game-root "assets" --project-root $Port
    } finally { Pop-Location }
}
Write-Host "Projet prêt. Lance ./build.ps1 -Codegen"
