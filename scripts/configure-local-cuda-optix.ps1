param(
  [string]$BuildDir = "build",
  [string]$OptixRoot = $env:OPTIX_ROOT_DIR,
  [string]$VcpkgRoot = $env:VCPKG_ROOT
)

$ErrorActionPreference = "Stop"

function Add-PathEntry {
  param([string]$PathEntry)

  if ($PathEntry -and (Test-Path $PathEntry)) {
    $entries = $env:PATH -split [System.IO.Path]::PathSeparator
    if ($entries -notcontains $PathEntry) {
      $env:PATH = "$PathEntry$([System.IO.Path]::PathSeparator)$env:PATH"
    }
  }
}

function Import-VcVars64 {
  $vcvarsCandidates = @(
    "${env:ProgramFiles(x86)}\Microsoft Visual Studio\2022\BuildTools\VC\Auxiliary\Build\vcvars64.bat",
    "$env:ProgramFiles\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build\vcvars64.bat",
    "${env:ProgramFiles(x86)}\Microsoft Visual Studio\2019\BuildTools\VC\Auxiliary\Build\vcvars64.bat",
    "${env:ProgramFiles(x86)}\Microsoft Visual Studio\2019\Community\VC\Auxiliary\Build\vcvars64.bat"
  )

  $vcvars = $vcvarsCandidates | Where-Object { Test-Path $_ } | Select-Object -First 1
  if (-not $vcvars) {
    throw "Could not find vcvars64.bat. Install Visual Studio Build Tools with the C++ workload."
  }

  cmd /s /c "`"$vcvars`" >nul && set" | ForEach-Object {
    if ($_ -match "^(.*?)=(.*)$") {
      [System.Environment]::SetEnvironmentVariable($matches[1], $matches[2], "Process")
    }
  }
}

function Find-LatestCudaRoot {
  if ($env:CUDA_PATH -and (Test-Path $env:CUDA_PATH)) {
    return $env:CUDA_PATH
  }

  $cudaParent = "C:\Program Files\NVIDIA GPU Computing Toolkit\CUDA"
  if (Test-Path $cudaParent) {
    $cudaRoot = Get-ChildItem $cudaParent -Directory |
      Sort-Object Name -Descending |
      Select-Object -First 1
    if ($cudaRoot) {
      return $cudaRoot.FullName
    }
  }

  return $null
}

function Find-Ninja {
  $ninja = Get-Command ninja -ErrorAction SilentlyContinue
  if ($ninja) {
    return $ninja.Source
  }

  $wingetPackages = Join-Path $env:LOCALAPPDATA "Microsoft\WinGet\Packages"
  if (Test-Path $wingetPackages) {
    $ninja = Get-ChildItem $wingetPackages -Recurse -Filter ninja.exe -ErrorAction SilentlyContinue |
      Select-Object -First 1
    if ($ninja) {
      return $ninja.FullName
    }
  }

  return $null
}

function Find-VcpkgRoot {
  if ($VcpkgRoot -and (Test-Path (Join-Path $VcpkgRoot "scripts\buildsystems\vcpkg.cmake"))) {
    return $VcpkgRoot
  }

  $vcpkgCandidates = @(
    "${env:ProgramFiles(x86)}\Microsoft Visual Studio\2022\BuildTools\VC\vcpkg",
    "$env:ProgramFiles\Microsoft Visual Studio\2022\Community\VC\vcpkg",
    "${env:ProgramFiles(x86)}\Microsoft Visual Studio\2019\Community\VC\vcpkg"
  )

  return $vcpkgCandidates |
    Where-Object { Test-Path (Join-Path $_ "scripts\buildsystems\vcpkg.cmake") } |
    Select-Object -First 1
}

Import-VcVars64

$cudaRoot = Find-LatestCudaRoot
if (-not $cudaRoot) {
  throw "Could not find CUDA Toolkit. Install NVIDIA CUDA Toolkit 11 or newer."
}

$env:CUDA_PATH = $cudaRoot
Add-PathEntry (Join-Path $cudaRoot "bin")

$ninja = Find-Ninja
if (-not $ninja) {
  throw "Could not find ninja.exe. Install Ninja or restart the shell after installing it."
}

$resolvedVcpkgRoot = Find-VcpkgRoot
if (-not $resolvedVcpkgRoot) {
  throw "Could not find vcpkg. Install Visual Studio vcpkg or pass -VcpkgRoot pointing to a vcpkg checkout."
}

if ($OptixRoot) {
  if (-not (Test-Path (Join-Path $OptixRoot "include\optix.h"))) {
    throw "OptiX SDK not found at '$OptixRoot'. Pass -OptixRoot pointing to the SDK root containing include\optix.h."
  }
  $env:OPTIX_ROOT_DIR = $OptixRoot
}
elseif (-not $env:OPTIX_ROOT_DIR) {
  Write-Warning "OPTIX_ROOT_DIR is not set. Configure may disable OptiX until the NVIDIA OptiX SDK is installed and -OptixRoot is provided."
}

if (Test-Path $BuildDir) {
  Remove-Item -Recurse -Force $BuildDir
}

$buildRoot = Join-Path (Get-Location) $BuildDir
$vcpkgInstalled = Join-Path $buildRoot "vcpkg_installed\x64-windows"

$cmakeArgs = @(
  "-S", ".",
  "-B", $BuildDir,
  "-G", "Ninja",
  "-DCMAKE_MAKE_PROGRAM=$ninja",
  "-DCMAKE_TOOLCHAIN_FILE=$(Join-Path $resolvedVcpkgRoot "scripts\buildsystems\vcpkg.cmake")",
  "-DVCPKG_TARGET_TRIPLET=x64-windows",
  "-DTBB_INCLUDE_DIR=$(Join-Path $vcpkgInstalled "include")",
  "-DTBB_LIBRARY=$(Join-Path $vcpkgInstalled "lib\tbb12.lib")",
  "-DCMAKE_BUILD_TYPE=Release",
  "-DWITH_LIBS_PRECOMPILED=OFF",
  "-DWITH_CYCLES_ALEMBIC=OFF",
  "-DWITH_CYCLES_EMBREE=OFF",
  "-DWITH_CYCLES_OPENIMAGEDENOISE=OFF",
  "-DWITH_CYCLES_OPENSUBDIV=OFF",
  "-DWITH_CYCLES_OPENVDB=OFF",
  "-DWITH_CYCLES_NANOVDB=OFF",
  "-DWITH_CYCLES_OSL=OFF",
  "-DWITH_USD=OFF",
  "-DWITH_CYCLES_USD=OFF",
  "-DWITH_CYCLES_HYDRA_RENDER_DELEGATE=OFF",
  "-DWITH_STRICT_BUILD_OPTIONS=ON",
  "-DWITH_CYCLES_DEVICE_CUDA=ON",
  "-DWITH_CYCLES_DEVICE_OPTIX=ON",
  "-DWITH_CYCLES_CUDA_BINARIES=ON",
  "-DWITH_CYCLES_CUDA_BUILD_SERIAL=ON",
  "-DCYCLES_CUDA_BINARIES_ARCH=sm_86",
  "-DWITH_CYCLES_DEVICE_HIP=OFF",
  "-DWITH_CYCLES_DEVICE_ONEAPI=OFF"
)

if ($OptixRoot) {
  $cmakeArgs += "-DOPTIX_ROOT_DIR=$OptixRoot"
  $cmakeArgs += "-DCYCLES_RUNTIME_OPTIX_ROOT_DIR=$OptixRoot"
}

cmake @cmakeArgs
if ($LASTEXITCODE -ne 0) {
  exit $LASTEXITCODE
}

$cachePath = Join-Path $BuildDir "CMakeCache.txt"
if (Test-Path $cachePath) {
  Select-String -Path $cachePath -Pattern "WITH_(USD|CYCLES_USD|CYCLES_HYDRA_RENDER_DELEGATE|CYCLES_DEVICE_CUDA|CYCLES_DEVICE_OPTIX|CYCLES_CUDA_BINARIES|CYCLES_CUDA_BUILD_SERIAL|STRICT_BUILD_OPTIONS)|CYCLES_CUDA_BINARIES_ARCH|OPTIX_ROOT_DIR|CYCLES_RUNTIME_OPTIX_ROOT_DIR" |
    ForEach-Object { $_.Line }
}
