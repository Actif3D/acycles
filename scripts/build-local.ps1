param(
  [string]$BuildDir = "build",
  [string]$Target = "install",
  [string]$Config = "Release"
)

$ErrorActionPreference = "Stop"

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

function Add-PathEntry {
  param([string]$PathEntry)

  if ($PathEntry -and (Test-Path $PathEntry)) {
    $entries = $env:PATH -split [System.IO.Path]::PathSeparator
    if ($entries -notcontains $PathEntry) {
      $env:PATH = "$PathEntry$([System.IO.Path]::PathSeparator)$env:PATH"
    }
  }
}

Import-VcVars64

$vcpkgBin = Join-Path (Join-Path (Get-Location) $BuildDir) "vcpkg_installed\x64-windows\bin"
Add-PathEntry $vcpkgBin

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
}

exit 0
