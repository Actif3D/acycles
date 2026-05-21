param(
  [string]$BuildDir = "build",
  [string]$OptixRoot = $env:OPTIX_ROOT_DIR,
  [string]$OidnRoot = $env:OPENIMAGEDENOISE_ROOT_DIR,
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

function Add-EnvEntries {
  param(
    [string]$Name,
    [string[]]$Entries
  )

  $separator = [System.IO.Path]::PathSeparator
  $existing = [System.Environment]::GetEnvironmentVariable($Name, "Process")
  $values = @()
  foreach ($entry in $Entries) {
    if ($entry -and (Test-Path $entry) -and ($values -notcontains $entry)) {
      $values += $entry
    }
  }
  if ($existing) {
    foreach ($entry in ($existing -split $separator)) {
      if ($entry -and ($values -notcontains $entry)) {
        $values += $entry
      }
    }
  }

  [System.Environment]::SetEnvironmentVariable($Name, ($values -join $separator), "Process")
}

function Import-DotEnv {
  param([string]$Path)

  if (-not (Test-Path $Path)) {
    return @{}
  }

  $values = @{}
  foreach ($line in Get-Content $Path) {
    $trimmed = $line.Trim()
    if (-not $trimmed -or $trimmed.StartsWith("#")) {
      continue
    }

    $separator = $trimmed.IndexOf("=")
    if ($separator -lt 1) {
      continue
    }

    $name = $trimmed.Substring(0, $separator).Trim()
    $value = $trimmed.Substring($separator + 1).Trim()
    if ($value.Length -ge 2) {
      $quote = $value[0]
      if (($quote -eq '"' -or $quote -eq "'") -and $value[$value.Length - 1] -eq $quote) {
        $value = $value.Substring(1, $value.Length - 2)
      }
    }

    if ($name -match '^[A-Za-z_][A-Za-z0-9_]*$') {
      $values[$name] = $value
    }
  }

  return $values
}

function Import-VcVars64 {
  $msvcRootCandidates = @(
    "${env:ProgramFiles(x86)}\Microsoft Visual Studio\2022\BuildTools\VC\Tools\MSVC",
    "$env:ProgramFiles\Microsoft Visual Studio\2022\Community\VC\Tools\MSVC",
    "${env:ProgramFiles(x86)}\Microsoft Visual Studio\2019\BuildTools\VC\Tools\MSVC",
    "${env:ProgramFiles(x86)}\Microsoft Visual Studio\2019\Community\VC\Tools\MSVC"
  )

  $msvcRoot = $msvcRootCandidates | Where-Object { Test-Path $_ } | Select-Object -First 1
  if (-not $msvcRoot) {
    throw "Could not find MSVC tools. Install Visual Studio Build Tools with the C++ workload."
  }

  $msvcTools = Get-ChildItem $msvcRoot -Directory |
    Sort-Object Name -Descending |
    Select-Object -First 1
  if (-not $msvcTools) {
    throw "Could not find an installed MSVC toolset under $msvcRoot."
  }

  $sdkRoot = "${env:ProgramFiles(x86)}\Windows Kits\10"
  $sdkIncludeRoot = Join-Path $sdkRoot "Include"
  $sdkLibRoot = Join-Path $sdkRoot "Lib"
  $sdkBinRoot = Join-Path $sdkRoot "bin"
  $sdkVersion = Get-ChildItem $sdkIncludeRoot -Directory -ErrorAction SilentlyContinue |
    Where-Object {
      (Test-Path (Join-Path $_.FullName "ucrt")) -and
      (Test-Path (Join-Path $sdkLibRoot (Join-Path $_.Name "ucrt\x64"))) -and
      (Test-Path (Join-Path $sdkLibRoot (Join-Path $_.Name "um\x64")))
    } |
    Sort-Object Name -Descending |
    Select-Object -First 1
  if (-not $sdkVersion) {
    throw "Could not find a Windows 10 SDK with x64 libraries."
  }

  $env:VCINSTALLDIR = (Resolve-Path (Join-Path $msvcRoot "..\..\")).Path
  $env:VCToolsInstallDir = $msvcTools.FullName + "\"
  $env:WindowsSdkDir = $sdkRoot + "\"
  $env:WindowsSDKVersion = $sdkVersion.Name + "\"

  Add-PathEntry (Join-Path $msvcTools.FullName "bin\Hostx64\x64")
  Add-PathEntry (Join-Path $sdkBinRoot (Join-Path $sdkVersion.Name "x64"))
  Add-PathEntry (Join-Path $sdkBinRoot "x64")

  Add-EnvEntries "INCLUDE" @(
    (Join-Path $msvcTools.FullName "include"),
    (Join-Path $sdkVersion.FullName "ucrt"),
    (Join-Path $sdkVersion.FullName "shared"),
    (Join-Path $sdkVersion.FullName "um"),
    (Join-Path $sdkVersion.FullName "winrt"),
    (Join-Path $sdkVersion.FullName "cppwinrt")
  )

  Add-EnvEntries "LIB" @(
    (Join-Path $msvcTools.FullName "lib\x64"),
    (Join-Path $sdkLibRoot (Join-Path $sdkVersion.Name "ucrt\x64")),
    (Join-Path $sdkLibRoot (Join-Path $sdkVersion.Name "um\x64"))
  )

  Add-EnvEntries "LIBPATH" @(
    (Join-Path $msvcTools.FullName "lib\x64"),
    (Join-Path $sdkLibRoot (Join-Path $sdkVersion.Name "um\x64"))
  )
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

function Find-OidnRoot {
  if ($OidnRoot -and (Test-Path (Join-Path $OidnRoot "include\OpenImageDenoise\oidn.h"))) {
    return $OidnRoot
  }

  $oidnCandidates = @(
    "F:\Tmp\oidn-2.4.1.x64.windows",
    "F:\Tmp\oidn-2.3.3.x64.windows",
    "$env:ProgramFiles\Intel\Open Image Denoise",
    "$env:ProgramFiles\Intel\oneAPI\oidn\latest"
  )

  $oidnCandidates += Get-ChildItem "F:\Tmp" -Directory -Filter "oidn-*.x64.windows" -ErrorAction SilentlyContinue |
    Sort-Object Name -Descending |
    Select-Object -ExpandProperty FullName

  return $oidnCandidates |
    Where-Object { $_ -and (Test-Path (Join-Path $_ "include\OpenImageDenoise\oidn.h")) } |
    Select-Object -First 1
}

$repositoryRoot = (Resolve-Path (Join-Path $PSScriptRoot "..")).Path
$dotEnvValues = Import-DotEnv (Join-Path $repositoryRoot ".env")
if (-not $PSBoundParameters.ContainsKey("OptixRoot") -and $dotEnvValues.ContainsKey("OPTIX_ROOT_DIR")) {
  $OptixRoot = $dotEnvValues["OPTIX_ROOT_DIR"]
  $env:OPTIX_ROOT_DIR = $OptixRoot
}
if (-not $PSBoundParameters.ContainsKey("OidnRoot") -and $dotEnvValues.ContainsKey("OPENIMAGEDENOISE_ROOT_DIR")) {
  $OidnRoot = $dotEnvValues["OPENIMAGEDENOISE_ROOT_DIR"]
  $env:OPENIMAGEDENOISE_ROOT_DIR = $OidnRoot
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

$resolvedOidnRoot = Find-OidnRoot
if (-not $resolvedOidnRoot) {
  throw "Could not find OpenImageDenoise. Pass -OidnRoot pointing to the SDK root containing include\OpenImageDenoise\oidn.h."
}
$env:OPENIMAGEDENOISE_ROOT_DIR = $resolvedOidnRoot
Add-PathEntry (Join-Path $resolvedOidnRoot "bin")

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
  "-DWITH_CYCLES_OPENIMAGEDENOISE=ON",
  "-DOPENIMAGEDENOISE_ROOT_DIR=$resolvedOidnRoot",
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
  Select-String -Path $cachePath -Pattern "WITH_(USD|CYCLES_USD|CYCLES_HYDRA_RENDER_DELEGATE|CYCLES_OPENIMAGEDENOISE|CYCLES_DEVICE_CUDA|CYCLES_DEVICE_OPTIX|CYCLES_CUDA_BINARIES|CYCLES_CUDA_BUILD_SERIAL|STRICT_BUILD_OPTIONS)|CYCLES_CUDA_BINARIES_ARCH|OPENIMAGEDENOISE_ROOT_DIR|OPTIX_ROOT_DIR|CYCLES_RUNTIME_OPTIX_ROOT_DIR" |
    ForEach-Object { $_.Line }
}
