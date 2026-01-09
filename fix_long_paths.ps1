# Script to fix long path issues in ESP-IDF builds on Windows
# This script configures a shorter build directory path

$ErrorActionPreference = "Stop"

Write-Host "Konfiguracja krótszej ścieżki dla katalogu build..." -ForegroundColor Green

# Set shorter build directory (using C:\build instead of project/build)
$BUILD_DIR = "C:\build\ledbrain"

# Create build directory if it doesn't exist
if (-not (Test-Path $BUILD_DIR)) {
    New-Item -ItemType Directory -Force -Path $BUILD_DIR | Out-Null
    Write-Host "Utworzono katalog build: $BUILD_DIR" -ForegroundColor Yellow
}

# Set environment variable for current session
$env:BUILD_DIR_BASE = $BUILD_DIR
Write-Host "Ustawiono BUILD_DIR_BASE=$BUILD_DIR dla bieżącej sesji" -ForegroundColor Cyan

# Optionally set it permanently (requires user approval)
Write-Host "`nCzy chcesz ustawić tę zmienną na stałe? (T/N)" -ForegroundColor Yellow
$response = Read-Host
if ($response -eq "T" -or $response -eq "t") {
    [System.Environment]::SetEnvironmentVariable("BUILD_DIR_BASE", $BUILD_DIR, "User")
    Write-Host "Zmienna BUILD_DIR_BASE została ustawiona na stałe" -ForegroundColor Green
}

Write-Host "`nAby użyć krótszej ścieżki build, uruchom:" -ForegroundColor Cyan
Write-Host "  `$env:BUILD_DIR_BASE='$BUILD_DIR'" -ForegroundColor White
Write-Host "  idf.py build" -ForegroundColor White



