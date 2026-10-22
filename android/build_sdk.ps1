# Compile et installe le SDK ReXGlue pour Android (NDK), sans toucher à la version PC.
# Prérequis : patchs appliqués au SDK (setup.ps1), dont port/patches/rexglue-sdk-android.patch.
# Sorties : D:\Android\build\sdk-<abi> (compilation), D:\Android\build\sdk-install-<abi> (installé),
# recomp\rexglue-sdk\out\android-<abi> (bibliothèques). Les installations Android ne sont jamais
# inscrites dans le registre CMake (le jeu PC pourrait sinon les prendre).
param([ValidateSet("x86_64", "arm64-v8a", "all")] [string]$Abi = "all")
$ErrorActionPreference = "Stop"

$Sdk = if ($env:ANDROID_HOME) { $env:ANDROID_HOME } else { "D:\Android\Sdk" }
$Ndk = (Get-ChildItem "$Sdk\ndk" -Directory | Sort-Object Name | Select-Object -Last 1).FullName
$Src = Join-Path $PSScriptRoot "..\recomp\rexglue-sdk"
$abis = if ($Abi -eq "all") { @("x86_64", "arm64-v8a") } else { @($Abi) }

foreach ($a in $abis) {
    $short = if ($a -eq "arm64-v8a") { "arm64" } else { $a }
    $build = "D:\Android\build\sdk-$short"
    & cmake -S $Src -B $build -G Ninja "-DCMAKE_TOOLCHAIN_FILE=$Ndk\build\cmake\android.toolchain.cmake" `
        "-DANDROID_ABI=$a" -DANDROID_PLATFORM=android-28 -DANDROID_STL=c++_shared `
        -DCMAKE_BUILD_TYPE=Release "-DCMAKE_INSTALL_PREFIX=D:\Android\build\sdk-install-$short" `
        -DCMAKE_CXX_STANDARD=23 -DREXGLUE_ENABLE_TRACY=OFF -Wno-dev
    if ($LASTEXITCODE -ne 0) { throw "échec de la configuration ($a)" }
    & cmake --build $build
    if ($LASTEXITCODE -ne 0) { throw "échec de la compilation ($a)" }
    & cmake --install $build
    if ($LASTEXITCODE -ne 0) { throw "échec de l'installation ($a)" }
}
