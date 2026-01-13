# Script to fix sdkconfig after reconfigure
# This ensures OTA partition table and 4MB flash size are always used

$ErrorActionPreference = "Stop"

$sdkconfigPath = "sdkconfig"

if (-not (Test-Path $sdkconfigPath)) {
    Write-Host "sdkconfig not found, skipping fix" -ForegroundColor Yellow
    exit 0
}

Write-Host "Fixing sdkconfig for OTA partitions..." -ForegroundColor Cyan

# Read current sdkconfig
$content = Get-Content $sdkconfigPath -Raw

# Fix flash size (4MB)
$content = $content -replace '(?m)^CONFIG_ESPTOOLPY_FLASHSIZE_2MB=y', '# CONFIG_ESPTOOLPY_FLASHSIZE_2MB is not set'
$content = $content -replace '(?m)^# CONFIG_ESPTOOLPY_FLASHSIZE_4MB is not set', 'CONFIG_ESPTOOLPY_FLASHSIZE_4MB=y'
$content = $content -replace '(?m)^CONFIG_ESPTOOLPY_FLASHSIZE="2MB"', 'CONFIG_ESPTOOLPY_FLASHSIZE="4MB"'

# Fix partition table (custom OTA instead of single app)
$content = $content -replace '(?m)^CONFIG_PARTITION_TABLE_SINGLE_APP=y', '# CONFIG_PARTITION_TABLE_SINGLE_APP is not set'
$content = $content -replace '(?m)^# CONFIG_PARTITION_TABLE_CUSTOM is not set', 'CONFIG_PARTITION_TABLE_CUSTOM=y'
$content = $content -replace '(?m)^CONFIG_PARTITION_TABLE_FILENAME="partitions_singleapp\.csv"', 'CONFIG_PARTITION_TABLE_FILENAME="partitions.csv"'

# Ensure other partition types are disabled
$content = $content -replace '(?m)^CONFIG_PARTITION_TABLE_SINGLE_APP_LARGE=y', '# CONFIG_PARTITION_TABLE_SINGLE_APP_LARGE is not set'
$content = $content -replace '(?m)^CONFIG_PARTITION_TABLE_TWO_OTA=y', '# CONFIG_PARTITION_TABLE_TWO_OTA is not set'
$content = $content -replace '(?m)^CONFIG_PARTITION_TABLE_TWO_OTA_LARGE=y', '# CONFIG_PARTITION_TABLE_TWO_OTA_LARGE is not set'

# Write back
Set-Content -Path $sdkconfigPath -Value $content -NoNewline

Write-Host "sdkconfig fixed successfully!" -ForegroundColor Green
