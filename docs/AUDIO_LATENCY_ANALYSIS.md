# Analiza Latencji Audio: Snapcast → LED

## Wprowadzenie

Ten dokument analizuje całkowitą latencję od momentu, gdy sygnał wyjdzie ze Snapcasta do momentu, gdy światło pojawi się na LEDach.

## Pipeline Latencji

### 1. Snapcast Server → ESP32-P4 (Sieć)

**Opóźnienie sieciowe:**
- LAN (Ethernet): 1-5ms
- WiFi: 10-50ms (zależy od jakości sygnału)
- **Typowo: 2-10ms** (dla dobrej sieci)

**Uwaga:** To opóźnienie jest poza kontrolą ESP32, ale jest uwzględnione w buforowaniu.

### 2. Buforowanie Audio (Snapcast Task)

**Konfiguracja:**
```cpp
const uint32_t target_delay_ms = s_cfg.latency_ms > 0 ? s_cfg.latency_ms : 60;
const uint32_t min_delay_ms = target_delay_ms > 12 ? target_delay_ms - 12 : 48;
const uint32_t max_delay_ms = target_delay_ms + 12 ? target_delay_ms + 12 : 72;
```

**Charakterystyka:**
- **Domyślny bufor: 60ms** (konfigurowalny w `SnapcastConfig.latency_ms`)
- Bufor jest dynamicznie regulowany (min 48ms, max 72ms)
- Zapewnia płynne odtwarzanie i synchronizację z innymi klientami Snapcast

**To jest główny składnik latencji!**

### 3. Przetwarzanie FFT i Analiza Audio

**Kroki przetwarzania:**
1. **PCM → Float conversion**: ~0.1ms
2. **FFT (1024 samples)**: ~1-2ms (hardware-accelerated)
3. **Magnitude calculation**: ~0.2ms
4. **32-channel GEQ**: ~0.5ms
5. **Audio Dynamic Limiter**: ~0.1ms
6. **Beat detection**: ~0.1ms

**Total: ~2-3ms**

### 4. Renderowanie Efektu LED

**Zależy od rozmiaru i efektu:**

**Małe segmenty (<300 LED):**
- Software rendering: ~0.5-1ms
- **Total: ~0.5-1ms**

**Średnie segmenty (300-1000 LED):**
- Software rendering: ~1-2ms
- PPA (jeśli używany): ~0.5-1ms
- **Total: ~1-2ms**

**Duże segmenty (1000+ LED):**
- Software rendering: ~2-5ms
- PPA (jeśli używany): ~1-2ms
- **Total: ~1-2ms** (z PPA) lub **~2-5ms** (bez PPA)

**Matryce 2D:**
- Software rendering: ~3-8ms
- PPA (jeśli używany): ~1-3ms
- **Total: ~1-3ms** (z PPA) lub **~3-8ms** (bez PPA)

### 5. Synchronizacja Audio

**Mechanizm:**
```cpp
const uint64_t led_update_time_us = 5000;  // ~5ms dla LED update
const uint64_t ppa_overhead_us = 2000;     // ~2ms worst-case PPA
const uint64_t target_render_time = audio_metrics.timestamp_us - led_update_time_us - ppa_overhead_us;
```

**Charakterystyka:**
- Renderowanie odbywa się **przed** czasem odtworzenia audio
- Kompensacja: ~7ms (5ms LED update + 2ms PPA overhead)
- **Nie dodaje latencji** - renderowanie jest przesunięte w czasie, ale światło pojawia się synchronicznie z audio

### 6. Wysłanie do LED (RMT Transmission)

**Zależy od liczby LED:**

**100 LED:**
- RMT transmission: ~0.5ms
- **Total: ~0.5ms**

**300 LED:**
- RMT transmission: ~1.5ms
- **Total: ~1.5ms**

**1000 LED:**
- RMT transmission: ~5ms
- **Total: ~5ms**

**2000 LED:**
- RMT transmission: ~10ms
- **Total: ~10ms**

**Uwaga:** RMT transmission jest **non-blocking** - nie blokuje innych tasków.

### 7. Aktualizacja LED (Fizyczna)

**Charakterystyka:**
- Aktualizacja LED jest **natychmiastowa** po zakończeniu RMT transmission
- **Latencja: ~0ms** (fizyczna aktualizacja jest synchroniczna z RMT)

## Całkowita Latencja

### Składniki (kolejno):

1. **Sieć (Snapcast → ESP32)**: 2-10ms
2. **Buforowanie audio**: **60ms** (domyślnie, konfigurowalne)
3. **Przetwarzanie FFT**: 2-3ms
4. **Renderowanie efektu**: 0.5-3ms (zależy od rozmiaru)
5. **Synchronizacja**: 0ms (kompensowana przez timing)
6. **RMT transmission**: 0.5-10ms (zależy od liczby LED)
7. **Aktualizacja LED**: 0ms

### Całkowita Latencja:

**Dla małych segmentów (<300 LED):**
```
Sieć: 2-10ms
Bufor: 60ms
FFT: 2-3ms
Render: 0.5-1ms
RMT: 0.5-1.5ms
─────────────────
Total: ~65-75ms
```

**Dla średnich segmentów (300-1000 LED):**
```
Sieć: 2-10ms
Bufor: 60ms
FFT: 2-3ms
Render: 1-2ms
RMT: 1.5-5ms
─────────────────
Total: ~66-80ms
```

**Dla dużych segmentów (1000+ LED):**
```
Sieć: 2-10ms
Bufor: 60ms
FFT: 2-3ms
Render: 1-2ms (z PPA) lub 2-5ms (bez PPA)
RMT: 5-10ms
─────────────────
Total: ~70-88ms (z PPA) lub ~71-91ms (bez PPA)
```

## Optymalizacja Latencji

### 1. Zmniejszenie Bufora Audio

**Konfiguracja:**
```cpp
SnapcastConfig config;
config.latency_ms = 30;  // Zmniejsz z 60ms do 30ms
```

**Efekt:**
- Latencja: **~35-45ms** (zamiast 65-75ms)
- **UWAGA:** Może powodować jitter przy słabej sieci

### 2. Użycie PPA dla Dużych Segmentów

**Efekt:**
- Renderowanie: **1-2ms** (zamiast 2-5ms)
- Oszczędność: **~1-3ms**
- **Korzyść:** Mniejsza latencja + lepsza wydajność

### 3. Optymalizacja Sieci

**Ethernet zamiast WiFi:**
- Latencja sieciowa: **1-2ms** (zamiast 10-50ms)
- Oszczędność: **~5-45ms**
- **Korzyść:** Znacznie niższa latencja całkowita

## Przykładowe Scenariusze

### Scenariusz 1: Mała instalacja (300 LED, WiFi, domyślny bufor)

```
Sieć: 10ms
Bufor: 60ms
FFT: 2ms
Render: 1ms
RMT: 1.5ms
─────────────────
Total: ~74.5ms
```

### Scenariusz 2: Duża instalacja (2000 LED, Ethernet, zoptymalizowany bufor)

```
Sieć: 2ms
Bufor: 30ms (zoptymalizowany)
FFT: 2ms
Render: 2ms (z PPA)
RMT: 10ms
─────────────────
Total: ~46ms
```

### Scenariusz 3: Matryca 2D (64x64 = 4096 LED, Ethernet, PPA)

```
Sieć: 2ms
Bufor: 60ms
FFT: 2ms
Render: 3ms (z PPA)
RMT: 20ms (dla 4096 LED)
─────────────────
Total: ~87ms
```

## Wnioski

### Główne Składniki Latencji:

1. **Buforowanie audio (60ms)** - **największy składnik** (~80% latencji)
2. **RMT transmission (0.5-20ms)** - zależy od liczby LED
3. **Sieć (2-50ms)** - zależy od typu połączenia
4. **Przetwarzanie (2-3ms)** - stałe, niezależne od rozmiaru
5. **Renderowanie (0.5-3ms)** - zależy od rozmiaru i użycia PPA

### Rekomendacje:

1. **Dla niskiej latencji:** Zmniejsz `latency_ms` do 30ms (jeśli sieć jest stabilna)
2. **Dla dużych instalacji:** Użyj PPA (automatycznie włączony dla 300+ LED)
3. **Dla najlepszej wydajności:** Użyj Ethernet zamiast WiFi
4. **Dla synchronizacji:** Domyślny bufor 60ms zapewnia najlepszą synchronizację z innymi klientami Snapcast

### Typowa Latencja:

- **Małe instalacje (<300 LED)**: **~65-75ms**
- **Średnie instalacje (300-1000 LED)**: **~66-80ms**
- **Duże instalacje (1000+ LED)**: **~70-88ms**
- **Zoptymalizowane (Ethernet + mniejszy bufor)**: **~35-50ms**

**Uwaga:** Latencja jest **synchroniczna** - światło jest zsynchronizowane z audio, więc opóźnienie jest stałe i przewidywalne.
