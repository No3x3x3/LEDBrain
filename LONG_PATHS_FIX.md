# Rozwiązanie problemu z długimi ścieżkami w Windows

Projekt ESP-IDF może generować bardzo długie ścieżki podczas kompilacji, co na Windows może powodować błędy (limit 260 znaków).

## Rozwiązania

### Rozwiązanie 1: Użycie krótszego katalogu build (ZALECANE)

ESP-IDF pozwala na ustawienie własnego katalogu build poprzez zmienną środowiskową `BUILD_DIR_BASE`.

#### Opcja A: Użycie skryptu PowerShell
```powershell
.\fix_long_paths.ps1
```

#### Opcja B: Ręczne ustawienie
```powershell
# Dla bieżącej sesji
$env:BUILD_DIR_BASE = "C:\build\ledbrain"
idf.py build

# Lub na stałe (dla użytkownika)
[System.Environment]::SetEnvironmentVariable("BUILD_DIR_BASE", "C:\build\ledbrain", "User")
```

#### Opcja C: Użycie skryptu wrapper (NAJŁATWIEJSZE)
Użyj skryptu `build.ps1` zamiast bezpośredniego wywołania `idf.py`:
```powershell
.\build.ps1 build
.\build.ps1 flash
.\build.ps1 monitor
# itd.
```

Skrypt automatycznie ustawi krótszą ścieżkę build przed uruchomieniem `idf.py`.

### Rozwiązanie 2: Włączenie obsługi długich ścieżek w Windows 10/11

Windows 10 (wersja 1607+) i Windows 11 obsługują długie ścieżki, ale wymagają włączenia.

#### Metoda 1: Przez Edytor Rejestru (wymaga uprawnień administratora)

1. Otwórz Edytor Rejestru (`Win + R`, wpisz `regedit`)
2. Przejdź do: `HKEY_LOCAL_MACHINE\SYSTEM\CurrentControlSet\Control\FileSystem`
3. Znajdź wartość `LongPathsEnabled` (jeśli nie istnieje, utwórz nową wartość DWORD 32-bit)
4. Ustaw wartość na `1`
5. Zrestartuj komputer

#### Metoda 2: Przez PowerShell (wymaga uprawnień administratora)

```powershell
# Uruchom PowerShell jako Administrator
New-ItemProperty -Path "HKLM:\SYSTEM\CurrentControlSet\Control\FileSystem" `
    -Name "LongPathsEnabled" `
    -Value 1 `
    -PropertyType DWORD `
    -Force
```

Po wykonaniu tej komendy **wymagany jest restart systemu**.

#### Metoda 3: Przez Zasady Grupy (Windows Pro/Enterprise)

1. Otwórz `gpedit.msc`
2. Przejdź do: `Computer Configuration > Administrative Templates > System > Filesystem`
3. Włącz "Enable Win32 long paths"
4. Zrestartuj komputer

### Rozwiązanie 3: Przeniesienie projektu do krótszej ścieżki

Jeśli to możliwe, przenieś projekt do krótszej ścieżki:
- Z: `C:\Users\Dawid\LEDBrain`
- Do: `C:\LEDBrain` lub `C:\Projects\LEDBrain`

**Uwaga:** Po przeniesieniu projektu może być konieczne:
- Zaktualizowanie ścieżek w konfiguracji IDE
- Ponowne skonfigurowanie ESP-IDF w nowej lokalizacji

## Sprawdzenie długości ścieżek

Możesz sprawdzić najdłuższe ścieżki w projekcie:

```powershell
Get-ChildItem -Recurse -File | 
    Select-Object FullName, @{Name="Length";Expression={$_.FullName.Length}} | 
    Sort-Object Length -Descending | 
    Select-Object -First 10
```

## Zalecane rozwiązanie

Dla projektów ESP-IDF najlepszym rozwiązaniem jest **Rozwiązanie 1** (krótszy katalog build), ponieważ:
- Nie wymaga uprawnień administratora
- Nie wymaga restartu systemu
- Działa natychmiast
- Nie wpływa na inne aplikacje

