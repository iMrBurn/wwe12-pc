# Prépare l'environnement de compilation pour la session PowerShell courante
# (à charger avec « . $PSScriptRoot\env.ps1 ») : outils MSVC/Windows SDK + LLVM récent.
$vswhere = "${env:ProgramFiles(x86)}\Microsoft Visual Studio\Installer\vswhere.exe"
if (-not $env:VSINSTALLDIR -and (Test-Path $vswhere)) {
    $vs = & $vswhere -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath
    if ($vs) {
        $env:PATH = "$(Split-Path $vswhere);$env:PATH"  # VsDevCmd appelle vswhere en interne
        Import-Module (Join-Path $vs "Common7\Tools\Microsoft.VisualStudio.DevShell.dll")
        Enter-VsDevShell -VsInstallPath $vs -SkipAutomaticLocation -DevCmdArguments "-arch=x64 -host_arch=x64" | Out-Null
    }
}

# Le clang fourni par Visual Studio 2022 (19.x) est trop ancien pour ReXGlue (20+) :
# on place le LLVM installé séparément devant dans le PATH.
$llvm = "$env:ProgramFiles\LLVM\bin"
if (Test-Path "$llvm\clang-cl.exe") {
    $env:PATH = "$llvm;$env:PATH"
}

$clangVersion = (& clang-cl --version | Select-Object -First 1)
if ($clangVersion -notmatch "clang version (\d+)" -or [int]$Matches[1] -lt 20) {
    throw "Clang 20+ requis (trouvé : $clangVersion). Installe LLVM : winget install LLVM.LLVM"
}
$env:CC = "clang-cl"
$env:CXX = "clang-cl"

# Lance une commande externe et arrête le script si elle échoue
# (PowerShell 5.1 ignore les codes de sortie, même avec ErrorActionPreference=Stop).
function Invoke-Checked {
    $exe, $rest = $args
    & $exe @rest
    if ($LASTEXITCODE -ne 0) { throw "échec ($LASTEXITCODE) : $($args -join ' ')" }
}
