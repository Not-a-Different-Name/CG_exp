@echo off
cd /d "%~dp0"
if exist "Skins\steve_64x64.png" (
    start "" "%~dp0viewer.exe" --scene forest --character "Skins\steve_64x64.png"
) else (
    echo No local skin yet. Run: powershell -NoProfile -File tools\fetch_skins.ps1
    start "" "%~dp0viewer.exe" --scene forest
)
