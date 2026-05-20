param(
  [string]$RepositoryRoot = "",
  [string]$UpstreamRef = "",
  [int]$PollSeconds = 300,
  [string]$BuildDir = "build",
  [string]$OptixRoot = $env:OPTIX_ROOT_DIR,
  [switch]$Once
)

$ErrorActionPreference = "Stop"

function Write-Log {
  param([string]$Message)

  $timestamp = Get-Date -Format "yyyy-MM-dd HH:mm:ss"
  $line = "[$timestamp] $Message"
  Write-Output $line
  Add-Content -Path $script:LogPath -Value $line
}

function Invoke-RepoGit {
  param([string[]]$Arguments)

  & git -c "safe.directory=$script:GitSafeDirectory" @Arguments
}

function Get-RequiredGitOutput {
  param([string[]]$Arguments)

  $output = Invoke-RepoGit $Arguments
  if ($LASTEXITCODE -ne 0) {
    throw "git $($Arguments -join ' ') failed with exit code $LASTEXITCODE"
  }
  return ($output | Select-Object -First 1)
}

function Get-OptixRoot {
  if ($OptixRoot -and (Test-Path (Join-Path $OptixRoot "include\optix.h"))) {
    return $OptixRoot
  }

  $optixParent = "C:\ProgramData\NVIDIA Corporation"
  if (Test-Path $optixParent) {
    $sdk = Get-ChildItem $optixParent -Directory -Filter "OptiX SDK*" |
      Sort-Object Name -Descending |
      Where-Object { Test-Path (Join-Path $_.FullName "include\optix.h") } |
      Select-Object -First 1
    if ($sdk) {
      return $sdk.FullName
    }
  }

  return $null
}

function Split-RemoteRef {
  param([string]$Ref)

  $slash = $Ref.IndexOf("/")
  if ($slash -lt 1 -or $slash -eq ($Ref.Length - 1)) {
    throw "Upstream ref '$Ref' must look like remote/branch, for example origin/main or upstream/main."
  }

  return @{
    Remote = $Ref.Substring(0, $slash)
    Branch = $Ref.Substring($slash + 1)
  }
}

function Test-WorkingTreeClean {
  $status = Invoke-RepoGit @("status", "--porcelain")
  if ($LASTEXITCODE -ne 0) {
    throw "git status failed with exit code $LASTEXITCODE"
  }
  return -not $status
}

function Invoke-LoggedScript {
  param(
    [string]$ScriptPath,
    [string[]]$Arguments,
    [string]$Step
  )

  Write-Log "Starting $Step"
  & powershell -ExecutionPolicy Bypass -File $ScriptPath @Arguments *>&1 |
    Tee-Object -FilePath $script:LogPath -Append
  if ($LASTEXITCODE -ne 0) {
    throw "$Step failed with exit code $LASTEXITCODE"
  }
  Write-Log "Finished $Step"
}

if (-not $RepositoryRoot) {
  if (Test-Path (Join-Path (Get-Location) ".git")) {
    $RepositoryRoot = (Get-Location).Path
  }
  else {
    $RepositoryRoot = (Resolve-Path (Join-Path $PSScriptRoot "..")).Path
  }
}

$RepositoryRoot = (Resolve-Path $RepositoryRoot).Path
$script:GitSafeDirectory = $RepositoryRoot.Replace("\", "/")

Set-Location $RepositoryRoot

$logDir = Join-Path $RepositoryRoot "tmp\auto-build"
New-Item -ItemType Directory -Path $logDir -Force | Out-Null
$script:LogPath = Join-Path $logDir "auto-build.log"

$lockPath = Join-Path $logDir "auto-build.lock"
$lockStream = [System.IO.File]::Open($lockPath, [System.IO.FileMode]::OpenOrCreate, [System.IO.FileAccess]::ReadWrite, [System.IO.FileShare]::None)

try {
  if (-not $UpstreamRef) {
    $UpstreamRef = Get-RequiredGitOutput @("rev-parse", "--abbrev-ref", "--symbolic-full-name", "@{u}")
  }

  $remoteRef = Split-RemoteRef $UpstreamRef
  $resolvedOptixRoot = Get-OptixRoot
  if (-not $resolvedOptixRoot) {
    throw "Could not find OptiX SDK. Pass -OptixRoot pointing to a directory containing include\optix.h."
  }

  Write-Log "Watching $UpstreamRef every $PollSeconds seconds in $RepositoryRoot"
  Write-Log "Using OptiX root: $resolvedOptixRoot"

  do {
    try {
      Write-Log "Fetching $($remoteRef.Remote)"
      Invoke-RepoGit @("fetch", "--prune", $remoteRef.Remote) | Tee-Object -FilePath $script:LogPath -Append
      if ($LASTEXITCODE -ne 0) {
        throw "git fetch failed with exit code $LASTEXITCODE"
      }

      $headSha = Get-RequiredGitOutput @("rev-parse", "HEAD")
      $upstreamSha = Get-RequiredGitOutput @("rev-parse", $UpstreamRef)

      if ($headSha -eq $upstreamSha) {
        Write-Log "No upstream change. HEAD is $headSha"
      }
      else {
        Write-Log "Upstream changed: HEAD=$headSha $UpstreamRef=$upstreamSha"

        if (-not (Test-WorkingTreeClean)) {
          Write-Log "Working tree is dirty; skipping pull and build."
        }
        else {
          Invoke-RepoGit @("pull", "--ff-only", $remoteRef.Remote, $remoteRef.Branch) |
            Tee-Object -FilePath $script:LogPath -Append
          if ($LASTEXITCODE -ne 0) {
            throw "git pull --ff-only failed with exit code $LASTEXITCODE"
          }

          Invoke-LoggedScript `
            -ScriptPath (Join-Path $RepositoryRoot "scripts\configure-local-cuda-optix.ps1") `
            -Arguments @("-BuildDir", $BuildDir, "-OptixRoot", $resolvedOptixRoot) `
            -Step "configure"

          Invoke-LoggedScript `
            -ScriptPath (Join-Path $RepositoryRoot "scripts\build-local.ps1") `
            -Arguments @("-BuildDir", $BuildDir, "-Target", "install", "-Config", "Release") `
            -Step "build"

          $newHeadSha = Get-RequiredGitOutput @("rev-parse", "HEAD")
          Write-Log "Build completed for $newHeadSha"
        }
      }
    }
    catch {
      Write-Log "ERROR: $($_.Exception.Message)"
    }

    if (-not $Once) {
      Start-Sleep -Seconds $PollSeconds
    }
  } while (-not $Once)
}
finally {
  $lockStream.Dispose()
}
