param([switch]$InstallFfmpeg)

$ErrorActionPreference = 'Stop'
$repoRoot = Split-Path -Parent $PSScriptRoot
# An already-open terminal may still have the PATH from before winget installed.
$env:Path = [Environment]::GetEnvironmentVariable('Path', 'Machine') + ';' +
    [Environment]::GetEnvironmentVariable('Path', 'User') + ';' + $env:Path

if ($InstallFfmpeg -and !(Get-Command ffmpeg -ErrorAction SilentlyContinue)) {
    if (!(Get-Command winget -ErrorAction SilentlyContinue)) {
        throw 'winget is unavailable. Install Windows App Installer, then retry.'
    }
    & winget install --id Gyan.FFmpeg --exact --source winget --accept-package-agreements --accept-source-agreements --disable-interactivity
    if ($LASTEXITCODE -ne 0) { throw "FFmpeg installation failed: $LASTEXITCODE" }
    $env:Path = [Environment]::GetEnvironmentVariable('Path', 'Machine') + ';' +
        [Environment]::GetEnvironmentVariable('Path', 'User') + ';' + $env:Path
}

$missing = @()
$vswhere = Join-Path ${env:ProgramFiles(x86)} 'Microsoft Visual Studio\Installer\vswhere.exe'
$vsRoot = if (Test-Path -LiteralPath $vswhere) {
    & $vswhere -latest -products '*' -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath
}
if ($vsRoot) { Write-Output "[OK] MSVC C++ tools: $vsRoot" }
else { $missing += 'Visual Studio 2022 C++ desktop tools and Windows SDK' }

foreach ($file in @(
    'third_party/freeglut/include/GL/freeglut.h',
    'third_party/freeglut/lib/x64/freeglut.lib',
    'third_party/freeglut/bin/x64/freeglut.dll',
    'third_party/stb/stb_image.h',
    'Models/bunny_10k.obj'
)) {
    if (Test-Path -LiteralPath (Join-Path $repoRoot $file)) { Write-Output "[OK] $file" }
    else { $missing += $file }
}

foreach ($command in @('cmake', 'ctest', 'python', 'ffmpeg', 'ffprobe')) {
    $found = Get-Command $command -ErrorAction SilentlyContinue
    if ($found) { Write-Output "[OK] ${command}: $($found.Source)" }
    else { Write-Warning "$command is unavailable on PATH (see docs/IMPLEMENTATION_PLAN.md)." }
}
if (Get-Command ffmpeg -ErrorAction SilentlyContinue) {
    $encoders = & ffmpeg -hide_banner -encoders 2>&1
    if ($LASTEXITCODE -ne 0 -or !($encoders -match '\blibx264\b')) {
        $missing += 'FFmpeg libx264 encoder'
    } else { Write-Output '[OK] FFmpeg H.264 encoder (libx264)' }
} elseif ($InstallFfmpeg) { $missing += 'FFmpeg' }

if ($missing.Count) { throw ('Missing dependencies: ' + ($missing -join ', ')) }
Write-Output 'Dependency check complete. Use build.bat x64 to verify the compiler and SDK.'
