# Skrypt naprawczy dla programatorów CH340 z problemem CM_PROB_PHANTOM
# Uruchom jako Administrator

Write-Host "=== Naprawa programatorów CH340 ===" -ForegroundColor Cyan
Write-Host ""

# Sprawdź czy skrypt jest uruchomiony jako administrator
$isAdmin = ([Security.Principal.WindowsPrincipal] [Security.Principal.WindowsIdentity]::GetCurrent()).IsInRole([Security.Principal.WindowsBuiltInRole]::Administrator)
if (-not $isAdmin) {
    Write-Host "⚠️  UWAGA: Ten skrypt wymaga uprawnień administratora!" -ForegroundColor Yellow
    Write-Host "Kliknij prawym przyciskiem na PowerShell i wybierz 'Uruchom jako administrator'" -ForegroundColor Yellow
    Write-Host ""
    pause
    exit
}

# Krok 1: Znajdź wszystkie urządzenia CH340 z problemem
Write-Host "Krok 1: Wyszukiwanie urządzeń CH340..." -ForegroundColor Green
$ch340Devices = Get-PnpDevice | Where-Object {$_.InstanceId -like "*VID_1A86*"}

if ($ch340Devices.Count -eq 0) {
    Write-Host "❌ Nie znaleziono urządzeń CH340" -ForegroundColor Red
    Write-Host "   Podłącz programator i spróbuj ponownie" -ForegroundColor Yellow
    pause
    exit
}

Write-Host "   Znaleziono $($ch340Devices.Count) urządzeń CH340:" -ForegroundColor Green
foreach ($device in $ch340Devices) {
    Write-Host "   - $($device.FriendlyName) (Status: $($device.Status), Problem: $($device.Problem))" -ForegroundColor Yellow
}
Write-Host ""

# Krok 2: Odinstaluj urządzenia z problemem
Write-Host "Krok 2: Odinstalowywanie urządzeń CH340 z problemem..." -ForegroundColor Green
foreach ($device in $ch340Devices) {
    if ($device.Status -eq "Unknown" -or $device.Problem -ne "CM_PROB_NONE") {
        Write-Host "   Odinstalowywanie: $($device.FriendlyName)..." -ForegroundColor Yellow
        try {
            Remove-PnpDevice -InstanceId $device.InstanceId -Confirm:$false -ErrorAction SilentlyContinue
            Write-Host "   ✓ Odinstalowano: $($device.FriendlyName)" -ForegroundColor Green
        } catch {
            Write-Host "   ⚠️  Nie udało się odinstalować: $($device.FriendlyName)" -ForegroundColor Yellow
            Write-Host "      Spróbuj ręcznie w Menedżerze urządzeń" -ForegroundColor Yellow
        }
    }
}
Write-Host ""

# Krok 3: Odczekaj chwilę
Write-Host "Krok 3: Oczekiwanie 3 sekundy..." -ForegroundColor Green
Start-Sleep -Seconds 3
Write-Host ""

# Krok 4: Sprawdź czy urządzenia zostały usunięte
Write-Host "Krok 4: Sprawdzanie czy urządzenia zostały usunięte..." -ForegroundColor Green
$remainingDevices = Get-PnpDevice | Where-Object {$_.InstanceId -like "*VID_1A86*"}
if ($remainingDevices.Count -eq 0) {
    Write-Host "   ✓ Wszystkie urządzenia zostały usunięte" -ForegroundColor Green
} else {
    Write-Host "   ⚠️  Pozostało $($remainingDevices.Count) urządzeń:" -ForegroundColor Yellow
    foreach ($device in $remainingDevices) {
        Write-Host "      - $($device.FriendlyName)" -ForegroundColor Yellow
    }
}
Write-Host ""

# Krok 5: Instrukcje dla użytkownika
Write-Host "=== INSTRUKCJE ===" -ForegroundColor Cyan
Write-Host ""
Write-Host "1. ODŁĄCZ programator od komputera" -ForegroundColor Yellow
Write-Host "2. Jeśli używasz zewnętrznego zasilania - ODŁĄCZ je również" -ForegroundColor Yellow
Write-Host "3. Odczekaj 5 sekund" -ForegroundColor Yellow
Write-Host "4. PODŁĄCZ dodatkowe zasilanie USB do płytki (jak wczoraj)" -ForegroundColor Green
Write-Host "5. PODŁĄCZ programator do komputera (inny port USB jeśli możesz)" -ForegroundColor Green
Write-Host "6. Odczekaj 10 sekund" -ForegroundColor Yellow
Write-Host "7. Sprawdź Menedżer urządzeń (Win+X → Menedżer urządzeń)" -ForegroundColor Yellow
Write-Host ""
Write-Host "Jeśli programator nadal nie działa:" -ForegroundColor Cyan
Write-Host "  a) Spróbuj innego kabla USB" -ForegroundColor Yellow
Write-Host "  b) Spróbuj innego portu USB (najlepiej USB 2.0)" -ForegroundColor Yellow
Write-Host "  c) Zainstaluj sterowniki CH340 ponownie:" -ForegroundColor Yellow
Write-Host "     http://www.wch-ic.com/downloads/CH341SER_EXE.html" -ForegroundColor Cyan
Write-Host ""

# Krok 6: Sprawdź czy pojawiły się nowe urządzenia
Write-Host "Czy chcesz sprawdzić czy programator został wykryty? (T/N)" -ForegroundColor Cyan
$response = Read-Host
if ($response -eq "T" -or $response -eq "t") {
    Write-Host ""
    Write-Host "Sprawdzanie nowych urządzeń CH340..." -ForegroundColor Green
    Start-Sleep -Seconds 2
    $newDevices = Get-PnpDevice | Where-Object {$_.InstanceId -like "*VID_1A86*"}
    if ($newDevices.Count -gt 0) {
        Write-Host "   ✓ Znaleziono $($newDevices.Count) urządzeń:" -ForegroundColor Green
        foreach ($device in $newDevices) {
            $statusColor = if ($device.Status -eq "OK") { "Green" } else { "Yellow" }
            Write-Host "   - $($device.FriendlyName) (Status: $($device.Status))" -ForegroundColor $statusColor
        }
        
        # Sprawdź porty COM
        Write-Host ""
        Write-Host "Dostępne porty COM:" -ForegroundColor Green
        $comPorts = [System.IO.Ports.SerialPort]::getportnames()
        if ($comPorts.Count -gt 0) {
            foreach ($port in $comPorts) {
                Write-Host "   - $port" -ForegroundColor Cyan
            }
        } else {
            Write-Host "   ❌ Brak dostępnych portów COM" -ForegroundColor Red
        }
    } else {
        Write-Host "   ❌ Nie znaleziono nowych urządzeń CH340" -ForegroundColor Red
        Write-Host "   Podłącz programator i sprawdź Menedżer urządzeń" -ForegroundColor Yellow
    }
}

Write-Host ""
Write-Host "=== Zakończono ===" -ForegroundColor Cyan
Write-Host ""
pause
