param(
  [string]$BuildDir = "build",
  [string]$Target = "install",
  [string]$Config = "Release",
  [string]$OidnRoot = $env:OPENIMAGEDENOISE_ROOT_DIR
)

$ErrorActionPreference = "Stop"

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

function Find-OidnRoot {
  if ($OidnRoot -and (Test-Path (Join-Path $OidnRoot "bin\OpenImageDenoise.dll"))) {
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
    Where-Object { $_ -and (Test-Path (Join-Path $_ "bin\OpenImageDenoise.dll")) } |
    Select-Object -First 1
}

function Add-CMakeCompilerPath {
  param([string]$CachePath)

  if (-not (Test-Path $CachePath)) {
    return
  }

  $compilerLine = Get-Content $CachePath |
    Where-Object { $_ -match "^CMAKE_CXX_COMPILER:.*=(.+)$" } |
    Select-Object -First 1

  if ($compilerLine -and $compilerLine -match "^CMAKE_CXX_COMPILER:.*=(.+)$") {
    $compilerPath = $matches[1]
    if (Test-Path $compilerPath) {
      Add-PathEntry (Split-Path -Parent $compilerPath)
    }
  }
}

Import-VcVars64

$buildNinja = Join-Path $BuildDir "build.ninja"
if (-not (Test-Path $buildNinja)) {
  throw "Build directory $BuildDir is not configured. Run .\scripts\configure-local-cuda-optix.ps1 -OptixRoot <OptiX SDK root> -OidnRoot <OIDN SDK root> first, then run this build script again."
}

Add-CMakeCompilerPath (Join-Path $BuildDir "CMakeCache.txt")

$vcpkgBin = Join-Path (Join-Path (Get-Location) $BuildDir) "vcpkg_installed\x64-windows\bin"
Add-PathEntry $vcpkgBin

$resolvedOidnRoot = Find-OidnRoot
if (-not $resolvedOidnRoot) {
  throw "Could not find OpenImageDenoise runtime. Pass -OidnRoot pointing to the SDK root containing bin\OpenImageDenoise.dll."
}
$env:OPENIMAGEDENOISE_ROOT_DIR = $resolvedOidnRoot
Add-PathEntry (Join-Path $resolvedOidnRoot "bin")

if ($env:CUDA_PATH) {
  Add-PathEntry (Join-Path $env:CUDA_PATH "bin")
}
else {
  $cudaParent = "C:\Program Files\NVIDIA GPU Computing Toolkit\CUDA"
  if (Test-Path $cudaParent) {
    $cudaRoot = Get-ChildItem $cudaParent -Directory | Sort-Object Name -Descending | Select-Object -First 1
    if ($cudaRoot) {
      $env:CUDA_PATH = $cudaRoot.FullName
      Add-PathEntry (Join-Path $cudaRoot.FullName "bin")
    }
  }
}

cmake --build $BuildDir --target $Target --config $Config
if ($LASTEXITCODE -ne 0) {
  exit $LASTEXITCODE
}

if ($Target -eq "install") {
  $installDir = Join-Path (Get-Location) "install"
  if ((Test-Path $vcpkgBin) -and (Test-Path $installDir)) {
    Copy-Item -Path (Join-Path $vcpkgBin "*.dll") -Destination $installDir -Force
  }
  $oidnBin = Join-Path $resolvedOidnRoot "bin"
  if ((Test-Path $oidnBin) -and (Test-Path $installDir)) {
    Copy-Item -Path (Join-Path $oidnBin "*.dll") -Destination $installDir -Force
  }
}

exit 0
