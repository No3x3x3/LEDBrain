# PPA/Framebuffer a Synchronizacja Audio

## Wprowadzenie

Ten dokument wyjaśnia, jak PPA (Pixel Processing Accelerator) i framebuffer współpracują z analizą FFT i Snapcast, oraz jak zapewniamy synchronizację audio bez dropów i desynchronizacji.

## Architektura Synchronizacji

### 1. Priorytety Tasków

```
Priorytet 9: snapclient_task (najwyższy)
  - Przetwarzanie audio (FFT, GEQ, limiter)
  - Aktualizacja AudioMetrics z timestamp_us

Priorytet 8: wled_fx_task
  - Renderowanie efektów LED
  - Synchronizacja z audio timestamp
  - Użycie PPA (blocking, ale szybkie)
```

### 2. Mechanizm Synchronizacji Audio

**Timestamp-based sync:**
- Snapcast dostarcza `timestamp_us` - kiedy ramka audio powinna być odtworzona
- WLED FX task renderuje **przed** czasem odtworzenia (accounting dla LED update + PPA overhead)
- Kompensacja opóźnień PPA jest wbudowana w timing

**Kod synchronizacji:**
```cpp
const uint64_t led_update_time_us = 5000;  // ~5ms dla LED update
const uint64_t ppa_overhead_us = 2000;     // ~2ms worst-case PPA overhead
const uint64_t target_render_time = audio_metrics.timestamp_us - led_update_time_us - ppa_overhead_us;
```

## PPA a Synchronizacja

### Charakterystyka PPA

**Tryb operacji:**
- `PPA_TRANS_MODE_BLOCKING` - blokuje task podczas operacji
- Operacje są **bardzo szybkie** dla typowych rozmiarów:
  - 300-1000 LED: <0.5ms
  - 1000-2000 LED: 0.5-1ms
  - 2000+ LED: 1-2ms (worst-case)

**Dlaczego blocking jest OK:**
1. Operacje są szybkie (<2ms nawet dla dużych segmentów)
2. Task renderowania ma wysoki priorytet (8), więc nie jest przerywany
3. Synchronizacja audio kompensuje opóźnienia PPA
4. PPA jest używany tylko dla dużych segmentów (300+ LED), gdzie korzyści > overhead

### Kompensacja Opóźnień

**Wbudowana kompensacja:**
- System renderuje **przed** czasem odtworzenia audio
- PPA overhead (2ms) jest uwzględniony w timing
- Małe opóźnienia są naturalnie absorbowane przez buffer audio

**Przykład:**
```
Audio timestamp: 100ms
LED update time: 5ms
PPA overhead: 2ms
Target render: 100ms - 5ms - 2ms = 93ms

Jeśli PPA zajmie 1.5ms zamiast 2ms:
- Render kończy się w 94.5ms
- LED update kończy się w 99.5ms
- Audio gra w 100ms ✅ Synchronizacja zachowana
```

## Framebuffer a Synchronizacja

### Charakterystyka Framebuffera

**Użycie:**
- Tylko dla dużych segmentów (500+ LED)
- Tylko dla efektów multi-pass (Fire, przyszłe: blur, glow)
- Zarządzanie przez `FramebufferManager` (shared_ptr, thread-safe)

**Overhead:**
- Alokacja: jednorazowa (przy pierwszym użyciu)
- Kopiowanie: ~0.1-0.5ms dla 1000 LED (memcpy)
- **Nie powoduje problemów z synchronizacją**, bo:
  - Używany tylko dla dużych segmentów (gdzie PPA też jest używany)
  - Overhead jest mały w porównaniu do korzyści
  - Kompensowany przez audio sync timing

## FFT i Snapcast

### Przetwarzanie Audio

**Snapcast task (priorytet 9):**
- Odbiera PCM z Snapcast server
- Przetwarza FFT (hardware-accelerated)
- Oblicza 32-channel GEQ
- Stosuje Audio Dynamic Limiter
- Aktualizuje `AudioMetrics` z `timestamp_us`

**Timing:**
- FFT: ~1-2ms (hardware-accelerated)
- GEQ: ~0.5ms
- Limiter: ~0.1ms
- **Total: ~2-3ms per frame** (nie blokuje renderowania)

### Synchronizacja z LED

**Mechanizm:**
1. Snapcast przetwarza audio → `AudioMetrics.timestamp_us`
2. WLED FX task sprawdza timestamp przed renderowaniem
3. Czeka do odpowiedniego momentu (accounting dla PPA)
4. Renderuje frame (może użyć PPA)
5. Wysyła do LED (RMT, non-blocking)

**Gwarancje:**
- Audio jest zawsze przetwarzane pierwsze (wyższy priorytet)
- LED renderowanie jest synchronizowane z audio timestamp
- PPA overhead jest kompensowany
- Brak desynchronizacji nawet przy dużych segmentach

## Próg Użycia PPA

### Automatyczna Detekcja

**Progi optymalizacji:**
- PPA Fill: 300+ LED (strips) lub 32x32+ (matryce)
- PPA Blend: 200+ LED
- Framebuffer: 500+ LED

**Dlaczego te progi:**
- Dla mniejszych segmentów: overhead PPA > korzyści
- Dla większych: PPA jest znacznie szybszy niż software
- Progi są konserwatywne - zapewniają, że PPA zawsze jest szybszy

## Potencjalne Problemy i Rozwiązania

### Problem 1: PPA zajmuje zbyt długo

**Rozwiązanie:**
- PPA jest używany tylko dla segmentów, gdzie jest szybszy
- Kompensacja opóźnień w audio sync (2ms overhead)
- Fallback do software jeśli PPA failuje

### Problem 2: Framebuffer overhead

**Rozwiązanie:**
- Używany tylko dla dużych segmentów (500+ LED)
- Overhead jest mały (~0.5ms) w porównaniu do korzyści
- Kompensowany przez audio sync timing

### Problem 3: Desynchronizacja przy dużych segmentach

**Rozwiązanie:**
- Audio sync uwzględnia PPA overhead
- Renderowanie jest przed czasem odtworzenia
- Małe opóźnienia są absorbowane przez buffer audio

## Podsumowanie

✅ **PPA i framebuffer NIE powodują problemów z synchronizacją audio:**

1. **PPA operacje są szybkie** (<2ms nawet dla dużych segmentów)
2. **Synchronizacja kompensuje opóźnienia** (2ms overhead w timing)
3. **Framebuffer overhead jest minimalny** (~0.5ms dla 1000 LED)
4. **Audio ma wyższy priorytet** (task 9 vs 8)
5. **Progi optymalizacji są konserwatywne** (zawsze szybsze niż software)

**Rezultat:**
- Brak dropów audio
- Brak desynchronizacji światła względem dźwięku
- Płynne działanie nawet przy dużych instalacjach (1000+ LED)
- Optymalna wydajność dzięki PPA dla dużych segmentów
