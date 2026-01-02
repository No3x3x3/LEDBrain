# ESP-IDF Installation Script for Windows
# This script installs ESP-IDF v5.5.2

$ErrorActionPreference = "Stop"

Write-Host "Installing ESP-IDF v5.5.2..." -ForegroundColor Green

# Set installation directory
$IDF_PATH = "C:\Espressif\frameworks\esp-idf-v5.5.2"
$IDF_TOOLS_PATH = "C:\Espressif"

# Create directories
New-Item -ItemType Directory -Force -Path $IDF_PATH | Out-Null
New-Item -ItemType Directory -Force -Path $IDF_TOOLS_PATH | Out-Null

# Clone ESP-IDF
Write-Host "Cloning ESP-IDF repository..." -ForegroundColor Yellow
Set-Location $IDF_PATH\..
if (Test-Path "esp-idf-v5.5.2") {
    Write-Host "ESP-IDF directory already exists, skipping clone..." -ForegroundColor Yellow
    Set-Location "esp-idf-v5.5.2"
} else {
    git clone --recursive --branch v5.5.2 https://github.com/espressif/esp-idf.git esp-idf-v5.5.2
    Set-Location "esp-idf-v5.5.2"
}

# Install ESP-IDF tools
Write-Host "Installing ESP-IDF tools..." -ForegroundColor Yellow
python -m pip install --user -r requirements.txt
.\install.bat

Write-Host "ESP-IDF installation completed!" -ForegroundColor Green
Write-Host "To use ESP-IDF, run: C:\Espressif\frameworks\esp-idf-v5.5.2\export.ps1" -ForegroundColor Cyan

