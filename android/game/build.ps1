# Fabrique l'APK du jeu (sans Gradle) : bibliothèques déjà compilées pour Android
# (android/build_sdk.ps1 puis la compilation de port/ avec cmake/android.cmake), partie Java de
# SDL3 + WWE12Activity, réglages par défaut (port/config/wwe12.toml). AUCUNE donnée du jeu :
# le joueur copie son ISO dans Android/data/io.github.imrburn.wwe12/files/game.
# Sortie : D:\Android\build\apk\WWE12-PC-Android-<abi>.apk
param([ValidateSet("x86_64", "arm64-v8a")] [string]$Abi = "x86_64", [string]$Version = "")
$ErrorActionPreference = "Stop"

$Root = Resolve-Path (Join-Path $PSScriptRoot "..\..")
# Même numéro que la version PC (fichier VERSION), suivi de « -android ».
if (-not $Version) { $Version = (Get-Content (Join-Path $Root "VERSION") -TotalCount 1).Trim() + "-android" }
$Sdk = if ($env:ANDROID_HOME) { $env:ANDROID_HOME } else { "D:\Android\Sdk" }
$Ndk = (Get-ChildItem "$Sdk\ndk" -Directory | Sort-Object Name | Select-Object -Last 1).FullName
$Bt = (Get-ChildItem "$Sdk\build-tools" -Directory | Sort-Object Name | Select-Object -Last 1).FullName
$AndroidJar = (Get-ChildItem "$Sdk\platforms" -Directory | Sort-Object Name | Select-Object -Last 1).FullName + "\android.jar"
$env:JAVA_HOME = "C:\Program Files\Android\Android Studio\jbr"
$Java = "$env:JAVA_HOME\bin"
$Llvm = "$Ndk\toolchains\llvm\prebuilt\windows-x86_64"
$short = if ($Abi -eq "arm64-v8a") { "arm64" } else { $Abi }
$triple = if ($Abi -eq "arm64-v8a") { "aarch64-linux-android" } else { "x86_64-linux-android" }
$GameBuild = "D:\Android\build\game-$short"
$SdkOut = "$Root\recomp\rexglue-sdk\out\android-$Abi"
$Out = "D:\Android\build\apk-$short"
$Key = "D:\Android\keys\wwe12-android.jks"   # même clé que l'appli de diagnostic (android/diag/build.ps1)

function Run { $exe, $rest = $args; & $exe @rest; if ($LASTEXITCODE -ne 0) { throw "échec ($LASTEXITCODE) : $exe" } }

Remove-Item -Recurse -Force $Out -ErrorAction SilentlyContinue
New-Item -ItemType Directory -Force "$Out\classes", "$Out\dex", "$Out\lib\$Abi", "$Out\assets" | Out-Null

# 1. Bibliothèques natives (symboles de débogage retirés : l'APK reste raisonnable).
$libs = @("$GameBuild\libwwe12.so", "$GameBuild\librexgpu-xenos.so", "$SdkOut\librexruntime.so",
          "$Llvm\sysroot\usr\lib\$triple\libc++_shared.so")
# Bibliothèques « hook » de libadrenotools (pilotes Turnip, port/cmake/android.cmake), quand le jeu
# a été compilé avec : de vrais fichiers dans le dossier lib de l'appli, chargés par leur chemin.
$hooks = "$GameBuild\adrenotools\src\hook"
foreach ($h in "libhook_impl.so", "libmain_hook.so", "libfile_redirect_hook.so", "libgsl_alloc_hook.so") {
    if (Test-Path "$hooks\$h") { $libs += "$hooks\$h" }
}
foreach ($l in $libs) {
    if (-not (Test-Path $l)) { throw "manquant : $l (compiler d'abord le SDK et le jeu pour $Abi)" }
    Run "$Llvm\bin\llvm-strip.exe" --strip-debug $l -o "$Out\lib\$Abi\$(Split-Path $l -Leaf)"
}

# 2. Java : SDL3 + WWE12Activity -> classes.dex. Les classes java.* viennent du JDK (--release),
#    android.* d'android.jar (il n'a pas LambdaMetafactory, utile aux lambdas de SDL).
$SdlJava = "$Root\recomp\rexglue-sdk\thirdparty\sdl3\android-project\app\src\main\java"
$javaFiles = @(Get-ChildItem $SdlJava, "$PSScriptRoot\java" -Recurse -Filter *.java | ForEach-Object FullName)
& "$Java\javac.exe" --release 11 -nowarn -encoding UTF-8 -classpath $AndroidJar -d "$Out\classes" $javaFiles
if ($LASTEXITCODE -ne 0) { throw "échec de javac" }
# Classes regroupées dans un .jar : passées une par une, la ligne de commande de d8 devenait trop longue.
Remove-Item "$Out\classes.jar" -ErrorAction SilentlyContinue
& "$Java\jar.exe" cf "$Out\classes.jar" -C "$Out\classes" .
if ($LASTEXITCODE -ne 0) { throw "échec de jar" }
& "$Bt\d8.bat" --release --min-api 28 --lib $AndroidJar --output "$Out\dex" "$Out\classes.jar"
if ($LASTEXITCODE -ne 0) { throw "échec de d8" }

# 3. Réglages par défaut livrés (copiés au premier lancement s'il n'y en a pas).
Copy-Item "$Root\port\config\wwe12.toml" "$Out\assets\wwe12.toml"
# Pipelines déjà rencontrées (comme tools/package.ps1 pour le zip PC) : préparées dès le premier
# lancement, moins de gels au premier match. Empreintes et états de rendu, aucune donnée du jeu.
$Seed = Join-Path ([Environment]::GetFolderPath("MyDocuments")) "wwe12\cache\pipelines.bin"
Remove-Item "$Out\assets\pipelines_seed.w12p" -ErrorAction SilentlyContinue
if ((Test-Path $Seed) -and [BitConverter]::ToUInt32([IO.File]::ReadAllBytes($Seed), 0) -eq 0x50323157) {
    Copy-Item $Seed "$Out\assets\pipelines_seed.w12p"
} else {
    Write-Warning "Pas de cache de pipelines ($Seed) : l'APK partira sans pipelines_seed.w12p."
}

# 4. APK : manifeste + assets, puis dex et .so, alignement, signature.
$manifest = (Get-Content "$PSScriptRoot\AndroidManifest.xml" -Raw) -replace 'android:versionName="[^"]*"', "android:versionName=`"$Version`""
Set-Content "$Out\AndroidManifest.xml" $manifest -Encoding utf8
Run "$Bt\aapt2.exe" link --manifest "$Out\AndroidManifest.xml" -I $AndroidJar -A "$Out\assets" -o "$Out\base.apk"
Add-Type -AssemblyName System.IO.Compression, System.IO.Compression.FileSystem
$zip = [System.IO.Compression.ZipFile]::Open("$Out\base.apk", "Update")
try {
    [void][System.IO.Compression.ZipFileExtensions]::CreateEntryFromFile($zip, "$Out\dex\classes.dex", "classes.dex", "Optimal")
    Get-ChildItem "$Out\lib\$Abi" -Filter *.so | ForEach-Object {
        [void][System.IO.Compression.ZipFileExtensions]::CreateEntryFromFile($zip, $_.FullName, "lib/$Abi/$($_.Name)", "Optimal")
    }
} finally { $zip.Dispose() }
Run "$Bt\zipalign.exe" -f -P 16 4 "$Out\base.apk" "$Out\aligned.apk"
if (-not (Test-Path $Key)) { throw "clé absente : $Key (lancer d'abord android/diag/build.ps1, qui la crée)" }
$apk = "$Out\WWE12-PC-Android-$Abi.apk"
& "$Bt\apksigner.bat" sign --ks $Key --ks-pass pass:android --ks-key-alias wwe12 --out $apk "$Out\aligned.apk"
if ($LASTEXITCODE -ne 0) { throw "échec de la signature" }
Write-Host "APK : $apk ($([math]::Round((Get-Item $apk).Length / 1MB)) Mo)"
