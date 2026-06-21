param(
    [switch]$Run,
    [string]$Config = 'Debug'
)

$ErrorActionPreference = 'Stop'

# Project root is the folder that contains this script's parent (scripts\..).
$projectRoot = Split-Path -Parent $PSScriptRoot
$buildDir = Join-Path $projectRoot 'build'

# --- Locate vcpkg -----------------------------------------------------------
# vcpkg is used in *manifest mode*: the dependencies in vcpkg.json are installed
# automatically the first time CMake configures the project.
$candidateVcpkgRoots = @(
    $env:VCPKG_ROOT,
    'C:\Users\Mahdi\vcpkg',
    'C:\vcpkg'
) | Where-Object { $_ -and $_.Trim() -ne '' }

$vcpkgRoot = $candidateVcpkgRoots | Where-Object {
    Test-Path (Join-Path $_ 'scripts\buildsystems\vcpkg.cmake')
} | Select-Object -First 1

if (-not $vcpkgRoot) {
    throw 'Could not find vcpkg. Set VCPKG_ROOT or install vcpkg at C:\vcpkg.'
}

$toolchainFile = Join-Path $vcpkgRoot 'scripts\buildsystems\vcpkg.cmake'

# --- Configure --------------------------------------------------------------
$cmakeArgs = @(
    '-S', $projectRoot,
    '-B', $buildDir,
    "-DCMAKE_TOOLCHAIN_FILE=$toolchainFile"
)

Write-Host "Configuring TankGame in $buildDir"
cmake @cmakeArgs

# --- Build ------------------------------------------------------------------
Write-Host "Building TankGame ($Config)"
cmake --build $buildDir --config $Config --target TankGame

# --- Run --------------------------------------------------------------------
if ($Run) {
    $appDir = Join-Path $buildDir 'TankGame'

    # Multi-config generators (Visual Studio) put the exe in a <Config> subfolder;
    # single-config generators (Ninja) put it directly in $appDir.
    $exePath = Join-Path $appDir "$Config\TankGame.exe"
    if (-not (Test-Path $exePath)) {
        $exePath = Join-Path $appDir 'TankGame.exe'
    }
    if (-not (Test-Path $exePath)) {
        throw "Build finished but the executable was not found under $appDir"
    }

    # Run from $appDir so the relative shaders\ and textures\ paths resolve.
    Write-Host "Running $exePath"
    Push-Location $appDir
    try {
        & $exePath
    }
    finally {
        Pop-Location
    }
}
