<#
.SYNOPSIS
    Prepare l'archive de release (build + signature + installateur + zip) dans dist\. Ne publie rien.

.DESCRIPTION
    1. compile le plugin (preset windows-x64, RelWithDebInfo) et signe la DLL
    2. compile l'installateur autonome (DLL embarquee) et signe l'exe
    3. cree dist\obs-elgato-lights-<version>-windows-x64.zip contenant :
         Install-Elgato-Lights.exe                      installation automatique
         plugin\obs-elgato-lights\bin\64bit\*.dll       fichiers pour une installation manuelle
         LISEZMOI.txt, LICENSE
    La version est lue dans buildspec.json.

.EXAMPLE
    .\tools\Package-Release.ps1
    .\tools\Package-Release.ps1 -SkipSign
#>
param(
    [switch]$SkipSign
)

$ErrorActionPreference = "Stop"
$root = Split-Path -Parent $PSScriptRoot
Set-Location $root

$cmake = (Get-Command cmake -ErrorAction SilentlyContinue).Source
if (-not $cmake) { $cmake = "C:\Program Files\CMake\bin\cmake.exe" }

function Invoke-Native([string]$Exe, [string[]]$Arguments) {
    & $Exe @Arguments
    if ($LASTEXITCODE -ne 0) { throw "Echec ($LASTEXITCODE) : $Exe $($Arguments -join ' ')" }
}

$version = (Get-Content (Join-Path $root "buildspec.json") -Raw | ConvertFrom-Json).version
Write-Host "== Version $version =="

# 1. Plugin
# Toujours reconfigurer : la version (buildspec.json) est figee dans plugin-support.c a la configuration.
Invoke-Native $cmake @("--preset", "windows-x64")
Invoke-Native $cmake @("--build", "--preset", "windows-x64", "--config", "RelWithDebInfo")
$dll = Join-Path $root "build_x64\RelWithDebInfo\obs-elgato-lights.dll"
if (-not (Test-Path $dll)) { throw "DLL introuvable : $dll" }

$dist = Join-Path $root "dist"
$stage = Join-Path $dist "stage"
if (Test-Path $stage) { Remove-Item -Recurse -Force $stage }
New-Item -ItemType Directory -Force (Join-Path $stage "plugin\obs-elgato-lights\bin\64bit") | Out-Null
$stagedDll = Join-Path $stage "plugin\obs-elgato-lights\bin\64bit\obs-elgato-lights.dll"
Copy-Item $dll $stagedDll

if (-not $SkipSign) {
    & (Join-Path $PSScriptRoot "Sign-File.ps1") -Path $stagedDll
}

# 2. Installateur (la DLL signee est embarquee)
$ib = Join-Path $root "build_installer"
Invoke-Native $cmake @("-S", "installer", "-B", $ib, "-G", "Visual Studio 17 2022", "-A", "x64",
    "-DPLUGIN_DLL=$stagedDll", "-DAPP_VERSION=$version")
Invoke-Native $cmake @("--build", $ib, "--config", "Release")
$setup = Join-Path $ib "Release\Install-Elgato-Lights.exe"
if (-not (Test-Path $setup)) { throw "Installateur introuvable : $setup" }
Copy-Item $setup (Join-Path $stage "Install-Elgato-Lights.exe")
if (-not $SkipSign) {
    & (Join-Path $PSScriptRoot "Sign-File.ps1") -Path (Join-Path $stage "Install-Elgato-Lights.exe")
}

# 3. Zip
Copy-Item (Join-Path $root "LICENSE") $stage
@"
Elgato Lights Control - plugin OBS Studio $version
==================================================

INSTALLATION AUTOMATIQUE
  1. Fermez completement OBS Studio.
  2. Double-cliquez sur Install-Elgato-Lights.exe.
  3. Lancez OBS puis ouvrez le menu Docks > Elgato Lights.
  (Options : /S installation silencieuse, /uninstall desinstallation.)

INSTALLATION MANUELLE
  Copiez le dossier "plugin\obs-elgato-lights" dans :
    C:\ProgramData\obs-studio\plugins\
  Le fichier doit se trouver a :
    C:\ProgramData\obs-studio\plugins\obs-elgato-lights\bin\64bit\obs-elgato-lights.dll

DESINSTALLATION
  Parametres Windows > Applications > Applications installees > "Elgato Lights Control (plugin OBS)".
  (ou lancez Install-Elgato-Lights.exe /uninstall ; en manuel, supprimez le dossier
    C:\ProgramData\obs-studio\plugins\obs-elgato-lights
  apres avoir ferme OBS.)

Prerequis : OBS Studio 31 ou superieur (64 bits), lampes Elgato sur le meme reseau local.
Configuration enregistree dans %APPDATA%\obs-studio\plugin_config\obs-elgato-lights\config.json
"@ | Set-Content -Path (Join-Path $stage "LISEZMOI.txt") -Encoding UTF8

$zip = Join-Path $dist "obs-elgato-lights-$version-windows-x64.zip"
if (Test-Path $zip) { Remove-Item $zip -Force }
Compress-Archive -Path (Join-Path $stage "*") -DestinationPath $zip
Copy-Item (Join-Path $stage "Install-Elgato-Lights.exe") (Join-Path $dist "Install-Elgato-Lights-$version.exe") -Force
Remove-Item -Recurse -Force $stage

$hash = (Get-FileHash $zip -Algorithm SHA256).Hash
Write-Host ""
Write-Host "Archive : $zip"
Write-Host "SHA256  : $hash"
Write-Host "Installateur seul (asset utilise par la mise a jour automatique) : dist\Install-Elgato-Lights-$version.exe"


