# Wrapper script for idf.py build with shorter build directory
# This script automatically sets BUILD_DIR_BASE to avoid long path issues on Windows

$ErrorActionPreference = "Stop"

# Set shorter build directory
$BUILD_DIR = "C:\build\ledbrain"

# Create build directory if it doesn't exist
if (-not (Test-Path $BUILD_DIR)) {
    New-Item -ItemType Directory -Force -Path $BUILD_DIR | Out-Null
    Write-Host "Utworzono katalog build: $BUILD_DIR" -ForegroundColor Yellow
}

# Set environment variable
$env:BUILD_DIR_BASE = $BUILD_DIR
Write-Host "Używam katalogu build: $BUILD_DIR" -ForegroundColor Cyan

# Pass all arguments to idf.py
& idf.py @args



