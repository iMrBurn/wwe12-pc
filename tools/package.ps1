# Assemble le dossier de distribution de WWE '12 PC (dist\WWE12-PC) à partir de la version Release.
#
#   powershell -ExecutionPolicy Bypass -File tools\package.ps1 [-Config release]
#
# Le dossier ne contient QUE le portage : l'exécutable, ses bibliothèques, les réglages par défaut
# et le mode d'emploi. Aucune donnée du jeu (default.xex, archives .pac, vidéos, sons) : chaque
# joueur copie celles de SON jeu dans le sous-dossier game\. Le script refuse de terminer s'il en
# trouve une.
param(
    [ValidateSet("release", "relwithdebinfo")] [string]$Config = "release"
)
$ErrorActionPreference = "Stop"
$Root  = Split-Path -Parent $PSScriptRoot
$Build = Join-Path $Root "port\out\build\win-amd64-$Config"
$Dist  = Join-Path $Root "dist\WWE12-PC"
# Numéro de version (fichier VERSION à la racine), repris dans le nom de l'archive zip.
$Version = (Get-Content (Join-Path $Root "VERSION") -TotalCount 1).Trim()

if (-not (Test-Path (Join-Path $Build "wwe12.exe"))) {
    throw "wwe12.exe introuvable dans $Build : compiler d'abord (build.ps1 -Config $Config)."
}

# Repart d'un dossier propre, en gardant le dossier game\ s'il existe déjà (données du testeur).
if (Test-Path $Dist) {
    Get-ChildItem $Dist -Force | Where-Object { $_.Name -ne "game" } | Remove-Item -Recurse -Force
} else {
    New-Item -ItemType Directory -Path $Dist | Out-Null
}
New-Item -ItemType Directory -Force -Path (Join-Path $Dist "game") | Out-Null

# Exécutable et bibliothèques. Le client du profileur (Tracy) ne sert qu'au développement.
Copy-Item (Join-Path $Build "wwe12.exe") $Dist
Copy-Item (Join-Path $Build "WWE12 Launcher.exe") $Dist
Get-ChildItem $Build -Filter *.dll | Where-Object { $_.Name -notlike "Tracy*" } |
    ForEach-Object { Copy-Item $_.FullName $Dist }

# Pipelines déjà rencontrées sur la machine du développeur (Documents\wwe12\cache\pipelines.bin,
# format W12P de pso_recorder.cpp : empreintes de shaders et états de rendu, aucune donnée du jeu).
# Livrées sous le nom pipelines_seed.w12p, le jeu les prépare dès le premier lancement.
$Seed = Join-Path ([Environment]::GetFolderPath("MyDocuments")) "wwe12\cache\pipelines.bin"
if (Test-Path $Seed) {
    $magic = [BitConverter]::ToUInt32([IO.File]::ReadAllBytes($Seed), 0)
    if ($magic -ne 0x50323157) { throw "$Seed n'est pas un cache de pipelines W12P" }
    Copy-Item $Seed (Join-Path $Dist "pipelines_seed.w12p")
} else {
    Write-Warning "Pas de cache de pipelines ($Seed) : le zip partira sans pipelines_seed.w12p."
}

# Notes de la version (dist\RELEASE_NOTES_v<ver>.md) : le launcher les affiche dans « What's new ».
$Notes = Join-Path $Root "dist\RELEASE_NOTES_v$Version.md"
if (Test-Path $Notes) {
    Copy-Item $Notes (Join-Path $Dist "CHANGELOG.md")
} else {
    Write-Warning "Pas de $Notes : le zip partira sans CHANGELOG.md (rien à afficher dans le launcher)."
}

# Réglages par défaut et mode d'emploi.
Copy-Item (Join-Path $Root "port\config\wwe12.toml") (Join-Path $Dist "wwe12.toml")
# Modes d'emploi (un par langue du jeu), numéro de version ajouté à leur titre.
foreach ($name in "README.txt", "LISEZMOI.txt", "LIESMICH.txt", "LEEME.txt", "LEGGIMI.txt") {
    $readme = Get-Content (Join-Path $Root "port\dist\$name") -Encoding UTF8
    $readme[0] = "$($readme[0]) - v$Version"
    Set-Content -Path (Join-Path $Dist $name) -Encoding UTF8 -Value $readme
}
# Note dans game\ : en anglais d'abord, puis une ligne par langue. (Remplace l'ancienne note.)
$OldNote = Join-Path $Dist "game\METTRE_LES_FICHIERS_DU_JEU_ICI.txt"
if (Test-Path $OldNote) { Remove-Item $OldNote -Force }
$NoteName = "PUT_GAME_FILES_HERE.txt"
Set-Content -Path (Join-Path $Dist "game\$NoteName") -Encoding UTF8 -Value @(
    "Put YOUR WWE '12 (Xbox 360) disc image here, e.g. WWE12.iso,",
    "or the disc contents (default.xex and the pac, movies, sound... folders). See README.txt.",
    "",
    "FR : mettez ici l'image .iso de VOTRE jeu (ou le contenu du disque). Voir LISEZMOI.txt.",
    "DE : Legen Sie hier das .iso-Image IHRES Spiels ab (oder den Disc-Inhalt). Siehe LIESMICH.txt.",
    "ES : coloca aquí la imagen .iso de TU juego (o el contenido del disco). Ver LEEME.txt.",
    "IT : metti qui l'immagine .iso del TUO gioco (o il contenuto del disco). Vedi LEGGIMI.txt."
)

# Licences : GPL-3.0 du portage et textes des composants tiers (obligatoires avec l'exe).
$Lic = Join-Path $Dist "licenses"
New-Item -ItemType Directory -Force -Path $Lic | Out-Null
$Sdk = Join-Path $Root "recomp\rexglue-sdk\thirdparty"
$licenseFiles = [ordered]@{
    "GPL-3.0.txt"                 = Join-Path $Root "LICENSE"
    "reblue.txt"                  = Join-Path $Root "port\src\reblue\LICENSE.reblue"
    "plume.txt"                   = Join-Path $Root "recomp\plume\LICENSE"
    "XenosRecomp.txt"             = Join-Path $Root "recomp\reblue-XenosRecomp\LICENSE.md"
    "zstd.txt"                    = Join-Path $Root "port\third_party\reblue_thirdparty\zstd\LICENSE"
    "disruptorplus.txt"           = Join-Path $Root "port\third_party\licenses\disruptorplus-LICENSE.txt"
    "renderdoc_app.txt"           = Join-Path $Root "port\third_party\licenses\renderdoc_app-LICENSE.txt"
    "rexglue-sdk.txt"             = Join-Path $Root "recomp\rexglue-sdk\LICENSE"
    "FFmpeg-LGPL-2.1.txt"         = Join-Path $Sdk "FFmpeg\COPYING.LGPLv2.1"
    "libmspack-LGPL-2.1.txt"      = Join-Path $Sdk "libmspack\libmspack\COPYING.LIB"
    "SDL3.txt"                    = Join-Path $Sdk "sdl3\LICENSE.txt"
    "imgui.txt"                   = Join-Path $Sdk "imgui\LICENSE.txt"
    "fmt.txt"                     = Join-Path $Sdk "fmt\LICENSE"
    "spdlog.txt"                  = Join-Path $Sdk "spdlog\LICENSE"
    "tomlplusplus.txt"            = Join-Path $Sdk "tomlplusplus\LICENSE"
    "xxHash.txt"                  = Join-Path $Sdk "xxHash\LICENSE"
    "utfcpp.txt"                  = Join-Path $Sdk "utfcpp\LICENSE"
    "o1heap.txt"                  = Join-Path $Sdk "o1heap\LICENSE"
    "aes_128.txt"                 = Join-Path $Sdk "aes_128\LICENSE"
    "simde.txt"                   = Join-Path $Sdk "simde\COPYING"
    "glslang.txt"                 = Join-Path $Sdk "glslang\LICENSE.txt"
    "VulkanMemoryAllocator.txt"   = Join-Path $Sdk "vulkan-memory-allocator\LICENSE.txt"
    "vulkan-headers.txt"          = Join-Path $Sdk "vulkan-headers\LICENSE.md"
    "vulkan-loader.txt"           = Join-Path $Sdk "vulkan-loader\LICENSE.txt"
    "spirv-headers.txt"           = Join-Path $Sdk "spirv-headers\LICENSE"
    "spirv-tools.txt"             = Join-Path $Sdk "spirv-tools\LICENSE"
}
foreach ($name in $licenseFiles.Keys) {
    Copy-Item $licenseFiles[$name] (Join-Path $Lic $name)
}
# Adresse du code source (GPL) : celle du dépôt GitHub (remote « origin »), sans le « .git ».
$SourceUrl = (git -C $Root remote get-url origin 2>$null)
if ($SourceUrl) {
    $SourceUrl = $SourceUrl.Trim() -replace '\.git$', ''
} else {
    Write-Warning "Pas de dépôt git « origin » : l'adresse du code source manque dans THIRD_PARTY_NOTICES.txt."
    $SourceUrl = "(address to be added)"
}
(Get-Content (Join-Path $Root "port\dist\THIRD_PARTY_NOTICES.txt") -Encoding UTF8) -replace '\{SOURCE_URL\}', $SourceUrl |
    Set-Content -Path (Join-Path $Dist "THIRD_PARTY_NOTICES.txt") -Encoding UTF8

# Dossier mods (vide) : ses fichiers remplacent ceux du jeu au même chemin.
New-Item -ItemType Directory -Force -Path (Join-Path $Dist "mods") | Out-Null
Set-Content -Path (Join-Path $Dist "mods\HOW_TO_USE_MODS.txt") -Encoding UTF8 -Value @(
    "Files put here replace the game's own, at the same path as on the disc.",
    "Example: mods\movies\titantron\100.bik replaces the disc's movies\titantron\100.bik",
    "(HD titantron videos, textures...). Remove the file to get the original back.",
    "",
    "FR : les fichiers posés ici remplacent ceux du jeu, au même chemin que sur le disque.",
    "DE : Dateien hier ersetzen die des Spiels, unter demselben Pfad wie auf der Disc.",
    "ES : los archivos puestos aquí sustituyen a los del juego, con la misma ruta que en el disco.",
    "IT : i file messi qui sostituiscono quelli del gioco, con lo stesso percorso del disco."
)

# Dossier dlc (vide) : les paquets Xbox 360 posés ici sont installés au démarrage.
New-Item -ItemType Directory -Force -Path (Join-Path $Dist "dlc") | Out-Null
Set-Content -Path (Join-Path $Dist "dlc\HOW_TO_INSTALL_DLC.txt") -Encoding UTF8 -Value @(
    "Put the DLC packages of YOUR WWE '12 (Xbox 360) here, as copied from the console: the files",
    "with long hexadecimal names from Content\0000000000000000\54510890\00000002\ (sub-folders",
    "are fine, you can copy the whole Content folder). They are installed the next time you start",
    "wwe12.exe, into Documents\wwe12\0000000000000000\54510890\00000002\. The log",
    "(logs\wwe12_*.log, lines starting with 'DLC') says what was installed or skipped.",
    "To reinstall a DLC, delete its folder in Documents\wwe12\0000000000000000\54510890\00000002\.",
    "",
    "FR : mettez ici les paquets DLC de VOTRE jeu (Xbox 360) ; ils sont installés au prochain lancement.",
    "DE : Legen Sie hier die DLC-Pakete IHRES Spiels (Xbox 360) ab; sie werden beim nächsten Start installiert.",
    "ES : ponga aquí los paquetes DLC de SU juego (Xbox 360); se instalan en el próximo inicio.",
    "IT : mettete qui i pacchetti DLC del VOSTRO gioco (Xbox 360); vengono installati al prossimo avvio."
)

# Garde-fou : aucune donnée du jeu ne doit partir avec le portage (hors game\, laissé au joueur).
$forbidden = Get-ChildItem $Dist -Recurse -File |
    Where-Object { $_.FullName -notlike "$Dist\game\*" -and
                   $_.Extension -in ".xex", ".pac", ".arc", ".bik", ".xma", ".iso", ".xsc", ".bin" }
# Les paquets DLC n'ont pas d'extension : rien d'autre que la note dans dlc\.
$forbidden = @($forbidden) + @(Get-ChildItem (Join-Path $Dist "dlc") -Recurse -File |
    Where-Object { $_.Name -ne "HOW_TO_INSTALL_DLC.txt" }) | Where-Object { $_ }
if ($forbidden) {
    throw "Données du jeu trouvées dans la distribution : $($forbidden.FullName -join ', ')"
}
$inGame = Get-ChildItem (Join-Path $Dist "game") -Recurse -File |
    Where-Object { $_.Name -ne $NoteName }
if ($inGame) {
    Write-Warning "dist\WWE12-PC\game contient des fichiers du jeu (test local) : ne pas les publier."
}

$size = (Get-ChildItem $Dist -Recurse -File | Where-Object { $_.FullName -notlike "$Dist\game\*" } |
         Measure-Object Length -Sum).Sum
"Distribution prête : $Dist ({0:N1} Mo hors game\)" -f ($size / 1MB)

# Archive à joindre à la release GitHub : tout sauf le contenu de game\ (le dossier vide et sa
# note y sont).
$Zip = Join-Path $Root "dist\WWE12-PC-v$Version.zip"
if (Test-Path $Zip) { Remove-Item $Zip -Force }
$items = Get-ChildItem $Dist -Recurse -File |
    Where-Object { $_.FullName -notlike "$Dist\game\*" -or $_.Name -eq $NoteName }
Add-Type -AssemblyName System.IO.Compression, System.IO.Compression.FileSystem
$archive = [System.IO.Compression.ZipFile]::Open($Zip, "Create")
try {
    foreach ($f in $items) {
        $rel = "WWE12-PC/" + $f.FullName.Substring($Dist.Length + 1).Replace("\", "/")
        [System.IO.Compression.ZipFileExtensions]::CreateEntryFromFile($archive, $f.FullName, $rel) | Out-Null
    }
} finally { $archive.Dispose() }
"Archive : $Zip"

# Empreintes SHA-256 à coller dans les notes de la release : un joueur vérifie ainsi que ses fichiers
# sont bien ceux publiés (utile quand un antivirus signale l'exe, non signé, à tort).
$Sums = Join-Path $Root "dist\SHA256-v$Version.txt"
@($Zip, (Join-Path $Dist "wwe12.exe"), (Join-Path $Dist "WWE12 Launcher.exe")) | ForEach-Object {
    "{0}  {1}" -f (Get-FileHash $_ -Algorithm SHA256).Hash.ToLower(), (Split-Path $_ -Leaf)
} | Set-Content -Path $Sums -Encoding ascii
"Empreintes : $Sums"

# Carte des symboles de cet exe (jamais publiée) : sans elle, les adresses d'un hang_trace.txt ou
# crash_trace.txt envoyé par un joueur ne se traduisent plus en fonctions dès la compilation suivante.
$Symbols = Join-Path $Root "dist\symbols-v$Version"
New-Item -ItemType Directory -Force -Path $Symbols | Out-Null
foreach ($f in "wwe12.map", "wwe12.exe") {
    $src = Join-Path $Build $f
    if (Test-Path $src) { Copy-Item $src $Symbols -Force }
}
"Symboles : $Symbols"
