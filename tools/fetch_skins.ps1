# Download sample Minecraft skins for local testing and build legacy / HD variants.
#
# Skins come from PrismarineJS/minecraft-assets (Mojang default skins); they are
# only for local format checks. Skins/ is git-ignored and is not redistributed.
# Usage: powershell -NoProfile -File tools\fetch_skins.ps1
param([string]$OutDir = "")

$ErrorActionPreference = "Stop"
if ([string]::IsNullOrWhiteSpace($OutDir)) {
    $OutDir = Join-Path (Split-Path -Parent $PSScriptRoot) "Skins"
}
New-Item -ItemType Directory -Force -Path $OutDir | Out-Null

$base = "https://cdn.jsdelivr.net/gh/PrismarineJS/minecraft-assets@master/data/1.21.11/entity/player"
$steve64 = Join-Path $OutDir "steve_64x64.png"
$alex64  = Join-Path $OutDir "alex_64x64_slim.png"

function Fetch([string]$Url, [string]$Out) {
    # Invoke-WebRequest can time out on some networks; prefer the system curl.
    if (Get-Command curl.exe -ErrorAction SilentlyContinue) {
        & curl.exe -sSL --fail --max-time 60 -o $Out $Url
        if ($LASTEXITCODE -ne 0) { throw "curl failed: $Url" }
    } else {
        Invoke-WebRequest -UseBasicParsing -Uri $Url -OutFile $Out -TimeoutSec 60
    }
}

Write-Host "Downloading Steve (wide) and Alex (slim) ..."
Fetch "$base/wide/steve.png" $steve64
Fetch "$base/slim/alex.png"  $alex64

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
