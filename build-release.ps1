[CmdletBinding()]
param(
    [switch]$SkipTests,
    [switch]$BuildInstaller
)

$ErrorActionPreference = "Stop"

$ProjectRoot = Split-Path -Parent $MyInvocation.MyCommand.Path
Set-Location $ProjectRoot

Write-Host "== StrafeHelper Remake Release Builder ==" -ForegroundColor Cyan

# Find a usable VS 2022 developer environment.
$vswhere = "${env:ProgramFiles(x86)}\Microsoft Visual Studio\Installer\vswhere.exe"
if (-not (Test-Path $vswhere)) {
    throw "Visual Studio 2022 was not found. Install 'Desktop development with C++'."
}

$vsInstall = & $vswhere -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath
if (-not $vsInstall) {
    throw "MSVC C++ tools were not found."
}

$devCmd = Join-Path $vsInstall "Common7\Tools\Launch-VsDevShell.ps1"
if (-not (Test-Path $devCmd)) {
    throw "Visual Studio developer shell was not found."
}

& $devCmd -Arch amd64 -HostArch amd64

$cmake = Get-Command cmake -ErrorAction SilentlyContinue
if (-not $cmake) {
    throw "CMake was not found. Install CMake tools for Windows through Visual Studio."
}

$buildDir = Join-Path $ProjectRoot "out\release"
$stageDir = Join-Path $ProjectRoot "dist\StrafeHelper"
$distDir = Join-Path $ProjectRoot "dist"

if (Test-Path $buildDir) { Remove-Item $buildDir -Recurse -Force }
if (Test-Path $stageDir) { Remove-Item $stageDir -Recurse -Force }
New-Item -ItemType Directory -Path $stageDir -Force | Out-Null

# Use Ninja so the script works from the VS developer shell without
# depending on the Visual Studio CMake generator.
& cmake -S $ProjectRoot -B $buildDir -G Ninja -DCMAKE_BUILD_TYPE=Release
if ($LASTEXITCODE -ne 0) { throw "CMake configure failed." }

& cmake --build $buildDir --parallel
if ($LASTEXITCODE -ne 0) { throw "Build failed." }

if (-not $SkipTests) {
    & ctest --test-dir $buildDir --output-on-failure
    if ($LASTEXITCODE -ne 0) { throw "Tests failed." }
}

$exe = Join-Path $buildDir "StrafeHelper.exe"
if (-not (Test-Path $exe)) {
    throw "StrafeHelper.exe was not produced."
}

Copy-Item $exe (Join-Path $stageDir "StrafeHelper.exe")

@"
StrafeHelper Remake
===================

Portable release build.

Run:
  StrafeHelper.exe

The application stores its configuration under:
  %APPDATA%\StrafeHelperRemake

This build is a standalone training utility. It does not inject
keyboard input into games, bypass anti-cheat, or use a kernel driver.
"@ | Set-Content -Path (Join-Path $stageDir "README.txt") -Encoding UTF8

$version = "1.0.0"
$zip = Join-Path $distDir "StrafeHelper-Portable-v$version-win-x64.zip"
if (Test-Path $zip) { Remove-Item $zip -Force }

Compress-Archive -Path (Join-Path $stageDir "*") -DestinationPath $zip -CompressionLevel Optimal

Write-Host ""
Write-Host "Portable release created:" -ForegroundColor Green
Write-Host "  $zip" -ForegroundColor Green

if ($BuildInstaller) {
    $iscc = Get-Command iscc.exe -ErrorAction SilentlyContinue
    if (-not $iscc) {
        Write-Warning "Inno Setup compiler (iscc.exe) was not found. Portable ZIP was still created."
    } else {
        & $iscc (Join-Path $ProjectRoot "installer.iss")
        if ($LASTEXITCODE -ne 0) { throw "Installer build failed." }
        Write-Host "Installer created under dist\installer" -ForegroundColor Green
    }
}
