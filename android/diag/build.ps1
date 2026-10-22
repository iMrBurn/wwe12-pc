# Fabrique l'APK de diagnostic Android (sans Gradle) : NDK + javac + d8 + aapt2 + apksigner.
# Sortie : D:\Android\build\diag\WWE12-DeviceCheck-<version>.apk (arm64-v8a + x86_64 pour l'émulateur).
# La clé de signature (D:\Android\keys) reste hors du dépôt : la garder, les mises à jour doivent
# être signées avec la même clé.
param([string]$Version = "1.0")
$ErrorActionPreference = "Stop"

$Sdk = if ($env:ANDROID_HOME) { $env:ANDROID_HOME } else { "D:\Android\Sdk" }
$Ndk = Get-ChildItem "$Sdk\ndk" -Directory | Sort-Object Name | Select-Object -Last 1
$Bt = Get-ChildItem "$Sdk\build-tools" -Directory | Sort-Object Name | Select-Object -Last 1
$AndroidJar = (Get-ChildItem "$Sdk\platforms" -Directory | Sort-Object Name | Select-Object -Last 1).FullName + "\android.jar"
$env:JAVA_HOME = "C:\Program Files\Android\Android Studio\jbr"
$Java = "$env:JAVA_HOME\bin"
$Src = $PSScriptRoot
$Out = "D:\Android\build\diag"
$KeyDir = "D:\Android\keys"
$Key = "$KeyDir\wwe12-android.jks"

function Run { $exe, $rest = $args; & $exe @rest; if ($LASTEXITCODE -ne 0) { throw "échec ($LASTEXITCODE) : $exe" } }

Remove-Item -Recurse -Force $Out -ErrorAction SilentlyContinue
New-Item -ItemType Directory -Force "$Out\classes", "$Out\dex", "$Out\apk" | Out-Null

# 1. Code natif, une bibliothèque par architecture.
$Clang = "$($Ndk.FullName)\toolchains\llvm\prebuilt\windows-x86_64\bin\clang++.exe"
foreach ($abi in @(@{ Name = "arm64-v8a"; Target = "aarch64-linux-android26" },
                   @{ Name = "x86_64"; Target = "x86_64-linux-android26" })) {
    $dir = "$Out\apk\lib\$($abi.Name)"
    New-Item -ItemType Directory -Force $dir | Out-Null
    Run $Clang --target=$($abi.Target) -O2 -std=c++17 -fPIC -shared -static-libstdc++ `
        -Wall -Wno-unused-function "-Wl,-z,max-page-size=16384" "-Wl,--gc-sections" -s `
        "$Src\jni\diag.cpp" -o "$dir\libwwe12diag.so" -lvulkan -landroid -llog
}

# 2. Java -> .class -> classes.dex
$javaFiles = Get-ChildItem "$Src\java" -Recurse -Filter *.java | ForEach-Object FullName
& "$Java\javac.exe" -source 8 -target 8 -nowarn -encoding UTF-8 -bootclasspath $AndroidJar -d "$Out\classes" $javaFiles
if ($LASTEXITCODE -ne 0) { throw "échec de javac" }
$classes = Get-ChildItem "$Out\classes" -Recurse -Filter *.class | ForEach-Object FullName
Run "$($Bt.FullName)\d8.bat" --release --min-api 26 --lib $AndroidJar --output "$Out\dex" $classes

# 3. Manifeste -> APK de base, puis ajout du dex et des .so. Les .so sont extraits à l'installation
#    (extractNativeLibs="true") : ZipArchive ne sait pas écrire d'entrée « stockée » sans compression.
$manifest = (Get-Content "$Src\AndroidManifest.xml" -Raw) -replace 'android:versionName="[^"]*"', "android:versionName=`"$Version`""
Set-Content "$Out\AndroidManifest.xml" $manifest -Encoding utf8
Run "$($Bt.FullName)\aapt2.exe" link --manifest "$Out\AndroidManifest.xml" -I $AndroidJar -o "$Out\base.apk"
Add-Type -AssemblyName System.IO.Compression, System.IO.Compression.FileSystem
$zip = [System.IO.Compression.ZipFile]::Open("$Out\base.apk", "Update")
try {
    [void][System.IO.Compression.ZipFileExtensions]::CreateEntryFromFile($zip, "$Out\dex\classes.dex", "classes.dex", "Optimal")
    Get-ChildItem "$Out\apk\lib" -Recurse -Filter *.so | ForEach-Object {
        $name = "lib/" + $_.Directory.Name + "/" + $_.Name
        [void][System.IO.Compression.ZipFileExtensions]::CreateEntryFromFile($zip, $_.FullName, $name, "Optimal")
    }
} finally { $zip.Dispose() }
Run "$($Bt.FullName)\zipalign.exe" -f -P 16 4 "$Out\base.apk" "$Out\aligned.apk"

# 4. Signature (clé créée une fois, hors du dépôt).
if (-not (Test-Path $Key)) {
    New-Item -ItemType Directory -Force $KeyDir | Out-Null
    Run "$Java\keytool.exe" -genkeypair -keystore $Key -storepass android -keypass android -alias wwe12 `
        -keyalg RSA -keysize 2048 -validity 10000 -dname "CN=WWE12 PC Android, O=iMrBurn"
}
$apk = "$Out\WWE12-DeviceCheck-$Version.apk"
Run "$($Bt.FullName)\apksigner.bat" sign --ks $Key --ks-pass pass:android --ks-key-alias wwe12 --out $apk "$Out\aligned.apk"
Run "$($Bt.FullName)\apksigner.bat" verify $apk
Write-Host "APK : $apk ($([math]::Round((Get-Item $apk).Length / 1KB)) Ko)"
