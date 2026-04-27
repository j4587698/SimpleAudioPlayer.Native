param(
    [string]$Arch = "x86_64",
    [string]$BuildType = "Release",
    [string]$MSYS2Root = "C:\msys64",
    [string]$BuildDir = (Join-Path $PSScriptRoot "..\build\native-win64"),
    [switch]$Clean
)

$ErrorActionPreference = "Stop"

$nativeDir = Join-Path $PSScriptRoot "Native"
$toolchainFile = Join-Path $nativeDir "windows.toolchain.cmake"
$cmake = Join-Path $MSYS2Root "mingw64\bin\cmake.exe"

if (-not (Test-Path $cmake)) {
    throw "CMake was not found at '$cmake'. Install MSYS2 and run: pacman -S --needed mingw-w64-x86_64-cmake mingw-w64-x86_64-gcc make git"
}

if ($Clean -and (Test-Path $BuildDir)) {
    Remove-Item $BuildDir -Recurse -Force
}

$cmakeCache = Join-Path $BuildDir "CMakeCache.txt"

$env:PATH = @(
    (Join-Path $MSYS2Root "mingw64\bin"),
    (Join-Path $MSYS2Root "usr\bin"),
    $env:PATH
) -join [IO.Path]::PathSeparator

$gitCandidates = @(
    (Join-Path ${env:ProgramFiles} "Git\cmd\git.exe"),
    (Join-Path ${env:ProgramFiles} "Git\bin\git.exe"),
    (Join-Path $MSYS2Root "usr\bin\git.exe")
)
$git = $gitCandidates | Where-Object { Test-Path $_ } | Select-Object -First 1
if (-not $git) {
    throw "Git was not found. Install Git for Windows or MSYS2 git."
}

$configureArgs = @(
    "-S", $nativeDir,
    "-B", $BuildDir,
    "-G", "MSYS Makefiles",
    "-DCROSS_ARCH=$Arch",
    "-DCMAKE_BUILD_TYPE=$BuildType",
    "-DGIT_EXECUTABLE:FILEPATH=$git"
)

if (-not (Test-Path $cmakeCache)) {
    $configureArgs += "-DCMAKE_TOOLCHAIN_FILE:FILEPATH=$toolchainFile"
}

& $cmake @configureArgs

if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }

& $cmake --build $BuildDir -- -j $env:NUMBER_OF_PROCESSORS
if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }

$dllPath = Join-Path $BuildDir "bin\libaudio_player.dll"
Write-Host "Native build finished: $dllPath"
