# Download sample Minecraft skins for local testing and build legacy / HD variants.
#
# Skins come from PrismarineJS/minecraft-assets (Mojang default skins); they are
# only for local format checks. Skins/ is git-ignored and is not redistributed.
# Usage: powershell -NoProfile -File tools\fetch_skins.ps1 [-Force]
# Valid existing Skins/*.png are kept (no download) unless -Force is given.
param([string]$OutDir = "", [switch]$Force)

$ErrorActionPreference = "Stop"
[Net.ServicePointManager]::SecurityProtocol = [Net.SecurityProtocolType]::Tls12

if ([string]::IsNullOrWhiteSpace($OutDir)) {
    $OutDir = Join-Path (Split-Path -Parent $PSScriptRoot) "Skins"
}
New-Item -ItemType Directory -Force -Path $OutDir | Out-Null

$player = "data/1.21.11/entity/player"
$sources = @(
    "https://cdn.jsdelivr.net/gh/PrismarineJS/minecraft-assets@master/$player",
    "https://github.com/PrismarineJS/minecraft-assets/raw/master/$player",
    "https://fastly.jsdelivr.net/gh/PrismarineJS/minecraft-assets@master/$player"
)
$steve64 = Join-Path $OutDir "steve_64x64.png"
$alex64  = Join-Path $OutDir "alex_64x64_slim.png"

function Get-PngSize([string]$Path) {
    if (-not (Test-Path $Path)) { return $null }
    $b = [System.IO.File]::ReadAllBytes($Path)
    if ($b.Length -lt 24 -or $b[0] -ne 137 -or $b[1] -ne 80 -or $b[2] -ne 78 -or $b[3] -ne 71) {
        return $null
    }
    $w = ($b[16] -shl 24) -bor ($b[17] -shl 16) -bor ($b[18] -shl 8) -bor $b[19]
    $h = ($b[20] -shl 24) -bor ($b[21] -shl 16) -bor ($b[22] -shl 8) -bor $b[23]
    return @($w, $h)
}

function Fetch([string]$RelPath, [string]$Out) {
    $tmp = "$Out.part"
    foreach ($base in $sources) {
        $url = "$base/$RelPath"
        try {
            if (Get-Command curl.exe -ErrorAction SilentlyContinue) {
                & curl.exe -fL --retry 2 --retry-all-errors --retry-delay 1 `
                    --ssl-no-revoke --connect-timeout 10 --max-time 30 -o $tmp $url
            } else {
                Invoke-WebRequest -UseBasicParsing -Uri $url -OutFile $tmp -TimeoutSec 60
            }
            $size = Get-PngSize $tmp
            if ($size -and $size[0] -eq 64 -and $size[1] -eq 64) {
                Move-Item -Force $tmp $Out
                Write-Host "  ok: $url"
                return
            }
        } catch { }
        if (Test-Path $tmp) { Remove-Item -Force $tmp }
    }
    if (Test-Path $Out) {
        Write-Warning "Download failed; keeping existing $Out"
        return
    }
    throw "Could not download $RelPath from any mirror."
}

function EnsureSkin([string]$RelPath, [string]$Out) {
    $size = Get-PngSize $Out
    if (-not $Force -and $size -and $size[0] -eq 64 -and $size[1] -eq 64) {
        Write-Host "  already present: $Out"
        return
    }
    Fetch $RelPath $Out
}

Write-Host "Checking Steve (wide) and Alex (slim) ..."
try { EnsureSkin "$player/wide/steve.png" $steve64 } catch { Write-Warning $_ }
try { EnsureSkin "$player/slim/alex.png"  $alex64  } catch { Write-Warning $_ }

# Offline fallback: the bundled TrailSurveyor skin is also a valid 64x64 PNG.
$size = Get-PngSize $steve64
if (-not $size -or $size[0] -ne 64 -or $size[1] -ne 64) {
    $fallback = Join-Path (Split-Path -Parent $PSScriptRoot) "Models\TrailSurveyor\TrailSurveyor.png"
    if (Test-Path $fallback) {
        Copy-Item -Force $fallback $steve64
        Write-Warning "Download unavailable; using bundled TrailSurveyor.png as the classic sample."
        $size = Get-PngSize $steve64
    }
}
if (-not $size -or $size[0] -ne 64 -or $size[1] -ne 64) {
    throw "No usable 64x64 skin: download failed and Models\TrailSurveyor\TrailSurveyor.png is missing."
}

Add-Type -AssemblyName System.Drawing
$src = [System.Drawing.Image]::FromFile($steve64)

# Legacy 64x32: the top half of a 64x64 skin is the legacy layout.
$legacy = New-Object System.Drawing.Bitmap(64, 32)
$g = [System.Drawing.Graphics]::FromImage($legacy)
$g.DrawImage($src,
    (New-Object System.Drawing.Rectangle(0, 0, 64, 32)),
    (New-Object System.Drawing.Rectangle(0, 0, 64, 32)),
    [System.Drawing.GraphicsUnit]::Pixel)
$g.Dispose()
$legacy.Save((Join-Path $OutDir "steve_legacy_64x32.png"),
             [System.Drawing.Imaging.ImageFormat]::Png)
$legacy.Dispose()

# HD 128x128: nearest-neighbour upscale keeps the pixel blocks crisp.
$hd = New-Object System.Drawing.Bitmap(128, 128)
$g = [System.Drawing.Graphics]::FromImage($hd)
$g.InterpolationMode = [System.Drawing.Drawing2D.InterpolationMode]::NearestNeighbor
$g.PixelOffsetMode   = [System.Drawing.Drawing2D.PixelOffsetMode]::Half
$g.DrawImage($src, 0, 0, 128, 128)
$g.Dispose()
$hd.Save((Join-Path $OutDir "steve_hd_128x128.png"),
         [System.Drawing.Imaging.ImageFormat]::Png)
$hd.Dispose()
$src.Dispose()

Get-ChildItem $OutDir -Filter *.png |
    ForEach-Object { Write-Host ("  {0}  {1} bytes" -f $_.Name, $_.Length) }
Write-Host "Skins ready in $OutDir"
