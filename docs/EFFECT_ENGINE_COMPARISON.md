# Wpływ wyboru silnika efektów w LEDBrain

## Wprowadzenie

LEDBrain obsługuje **dwa silniki efektów**:
- **WLED Engine** - efekty wizualne z projektu WLED
- **LEDFx Engine** - efekty audio-reactive z projektu LEDFx

Wybór silnika ma **znaczący wpływ** na:
- Audio-reaktywność
- Dostępne efekty
- Wydajność
- Parametry konfiguracji
- Zachowanie efektów

## Kluczowe różnice

### 1. Audio-Reaktywność

#### WLED Engine
- ❌ **Brak audio-reaktywności**
- ✅ Efekty działają niezależnie od audio
- ✅ Czyste efekty wizualne (Rainbow, Meteor, Fire, etc.)
- ✅ Stabilne, przewidywalne zachowanie

**Kod:**
```cpp
// WLED effects - audio reactivity is DISABLED
if (binding.effect.audio_link && is_ledfx) {
  // Audio processing - ONLY for LEDFx!
}
// WLED effects render without audio
```

#### LEDFx Engine
- ✅ **Pełna audio-reaktywność**
- ✅ Reaguje na bass, mid, treble, beat
- ✅ Wsparcie dla 32-channel GEQ
- ✅ Custom frequency ranges
- ✅ Attack/release envelope
- ✅ Audio profiles (energy, tempo)

**Kod:**
```cpp
// LEDFx effects - audio reactivity is ENABLED
if (binding.effect.audio_link && is_ledfx) {
  // Get audio metrics
  AudioMetrics metrics = led_audio_get_metrics();
  // Process audio for effect modulation
  float audio_mod = calculate_audio_modulation(...);
  // Apply to effect
  return ledfx_effects::render_effect(..., audio_mod, ...);
}
```

### 2. Dostępne Efekty

#### WLED Engine
**Efekty wizualne (30+):**
- Solid, Blink, Breathe, Chase
- Rainbow, Colorloop, Meteor
- Fire, Fireworks, Candle
- Scanner, Theater, Noise
- Energy Flow, Power Cycle
- I wiele innych...

**Charakterystyka:**
- Czyste efekty wizualne
- Time-based animation
- Nie wymagają audio

#### LEDFx Engine
**Efekty audio-reactive (10+):**
- Energy Waves, Plasma, Matrix
- Paintbrush, 3D GEQ
- Fire, Waves, Rain
- Hyperspace, Aura
- I inne...

**Charakterystyka:**
- Audio-reactive
- Reagują na muzykę w czasie rzeczywistym
- Wymagają audio source (Snapcast)

### 3. Parametry Konfiguracji

#### WLED Engine
**Parametry:**
- `speed` (0-255) - szybkość animacji
- `intensity` (0-255) - intensywność efektu
- `brightness` (0-255) - jasność
- `color1`, `color2`, `color3` - kolory
- `gradient` / `palette` - gradienty

**Brak:**
- ❌ Audio reactivity settings
- ❌ Frequency band selection
- ❌ Attack/release envelope
- ❌ Audio profiles

#### LEDFx Engine
**Parametry:**
- `speed` (0-255) - szybkość animacji
- `intensity` (0-255) - intensywność efektu
- `brightness` (0-255) - jasność
- `color1`, `color2`, `color3` - kolory
- `gradient` / `palette` - gradienty
- ✅ **`audio_link`** - włącza audio-reaktywność
- ✅ **`reactive_mode`** - kick, bass, mids, treble, full
- ✅ **`selected_bands`** - wybór pasm częstotliwości
- ✅ **`freq_min` / `freq_max`** - custom frequency range
- ✅ **`band_gain_low/mid/high`** - wzmocnienie pasm
- ✅ **`attack_ms` / `release_ms`** - envelope timing
- ✅ **`audio_profile`** - energy, tempo, default
- ✅ **`amplitude_scale`** - skala amplitudy
- ✅ **`brightness_compress`** - kompresja jasności
- ✅ **`beat_response`** - odpowiedź na beat

### 4. Renderowanie

#### WLED Engine
**Renderowanie:**
```cpp
// WLED effects - time-based animation
const uint32_t counter = (frame_idx * speed_val) >> 8;
// Effects use counter for animation
// No audio processing
```

**Charakterystyka:**
- Time-based (zależne od czasu)
- Frame counter dla animacji
- Deterministic (przewidywalne)
- Nie zależy od audio

#### LEDFx Engine
**Renderowanie:**
```cpp
// LEDFx effects - audio-reactive
const float time_s = static_cast<float>(frame_idx) / fps;
float audio_mod = calculate_audio_modulation(...);
// Effects use audio_mod for reactivity
return ledfx_effects::render_effect(..., audio_mod, ...);
```

**Charakterystyka:**
- Audio-reactive (zależne od audio)
- Audio modulation dla intensywności
- Dynamiczne (reaguje na muzykę)
- Wymaga audio source

### 5. Wydajność

#### WLED Engine
**CPU Usage:**
- Niski overhead (~1-3% dla 120 LED)
- Proste obliczenia matematyczne
- Brak przetwarzania audio
- Szybkie renderowanie

**Memory:**
- Minimalne (tylko state variables)
- Brak buforów audio

#### LEDFx Engine
**CPU Usage:**
- Średni overhead (~3-5% dla 120 LED)
- Dodatkowe przetwarzanie audio
- Audio modulation calculations
- Envelope processing

**Memory:**
- Minimalne (tylko state variables)
- Współdzielone audio metrics (nie duplikowane)

## Praktyczne Wpływy

### 1. Audio-Reaktywność

**WLED Engine:**
```cpp
// Audio reactivity is IGNORED for WLED effects
if (binding.effect.audio_link && is_ledfx) {
  // This block is SKIPPED for WLED effects
}
// WLED effects render without audio
```

**Wpływ:**
- ❌ Efekty WLED **nie reagują** na muzykę
- ✅ Działają stabilnie bez audio
- ✅ Idealne dla efektów wizualnych bez audio

**LEDFx Engine:**
```cpp
// Audio reactivity is PROCESSED for LEDFx effects
if (binding.effect.audio_link && is_ledfx) {
  AudioMetrics metrics = led_audio_get_metrics();
  // Calculate audio modulation
  float audio_mod = calculate_audio_modulation(...);
  // Apply to effect
}
```

**Wpływ:**
- ✅ Efekty LEDFx **reagują** na muzykę
- ✅ Dynamiczne, żywe efekty
- ✅ Wymagają audio source (Snapcast)

### 2. Dostępność Efektów

**WLED Engine:**
- ✅ Wszystkie efekty WLED dostępne
- ✅ Działają zawsze (nie wymagają audio)
- ✅ Stabilne, przewidywalne

**LEDFx Engine:**
- ✅ Wszystkie efekty LEDFx dostępne
- ⚠️ Wymagają audio source dla pełnej funkcjonalności
- ✅ Bardziej dynamiczne z audio

### 3. Konfiguracja Audio

**WLED Engine:**
- ❌ `audio_link` jest **ignorowane**
- ❌ Wszystkie audio settings są **niedostępne**
- ✅ UI automatycznie wyłącza audio settings

**LEDFx Engine:**
- ✅ `audio_link` **włącza** audio-reaktywność
- ✅ Wszystkie audio settings są **dostępne**
- ✅ Pełna kontrola nad audio reactivity

### 4. UI Behavior

**Automatyczne zachowanie:**
```javascript
// When engine changes to "wled"
if (assignment.engine === "wled") {
  assignment.audio_link = false;  // Disable audio
  // Audio settings become disabled in UI
}
```

**Wpływ:**
- UI automatycznie wyłącza audio dla WLED
- UI automatycznie włącza audio dla LEDFx
- Effect catalog filtruje efekty według silnika

## Kiedy używać którego silnika?

### Użyj WLED Engine gdy:

1. **Chcesz czyste efekty wizualne**
   - Rainbow, Meteor, Fire (bez audio)
   - Stabilne, przewidywalne animacje
   - Nie potrzebujesz audio-reaktywności

2. **Nie masz audio source**
   - Brak Snapcast
   - Tylko efekty wizualne
   - Standalone operation

3. **Wysoka wydajność**
   - Minimalne CPU usage
   - Szybkie renderowanie
   - Duże instalacje LED

4. **Kompatybilność z WLED**
   - Wysyłanie efektów do urządzeń WLED
   - DDP protocol compatibility
   - WLED device control

### Użyj LEDFx Engine gdy:

1. **Chcesz audio-reactive effects**
   - Reakcja na muzykę w czasie rzeczywistym
   - Dynamiczne, żywe efekty
   - Synchronizacja z audio

2. **Masz audio source**
   - Snapcast configured
   - Real-time audio analysis
   - Audio metrics available

3. **Zaawansowana kontrola audio**
   - Wybór pasm częstotliwości
   - Custom frequency ranges
   - Attack/release envelope
   - Audio profiles

4. **Efekty LEDFx**
   - Paintbrush, 3D GEQ
   - Energy Waves, Plasma
   - Matrix, Hyperspace

## Przykłady użycia

### Przykład 1: WLED Engine (bez audio)

```json
{
  "engine": "wled",
  "effect": "Rainbow",
  "speed": 128,
  "intensity": 128,
  "brightness": 255,
  "audio_link": false  // Ignored for WLED
}
```

**Wynik:**
- ✅ Efekt Rainbow działa
- ❌ Nie reaguje na audio
- ✅ Stabilny, przewidywalny

### Przykład 2: LEDFx Engine (z audio)

```json
{
  "engine": "ledfx",
  "effect": "Paintbrush",
  "speed": 128,
  "intensity": 128,
  "brightness": 255,
  "audio_link": true,  // Required for audio reactivity
  "reactive_mode": "full",
  "selected_bands": ["bass", "mid", "treble"],
  "attack_ms": 25,
  "release_ms": 120
}
```

**Wynik:**
- ✅ Efekt Paintbrush działa
- ✅ Reaguje na audio (bass, mid, treble)
- ✅ Dynamiczny, żywy
- ✅ Smooth attack/release

### Przykład 3: LEDFx Engine (bez audio)

```json
{
  "engine": "ledfx",
  "effect": "Plasma",
  "audio_link": false  // Audio disabled
}
```

**Wynik:**
- ✅ Efekt Plasma działa
- ❌ Nie reaguje na audio
- ✅ Działa jak efekt wizualny
- ⚠️ Mniej dynamiczny niż z audio

## Wpływ na wydajność

### CPU Usage Comparison

| Silnik | Bez Audio | Z Audio | Overhead |
|--------|-----------|---------|----------|
| **WLED** | ~1-3% | ~1-3% | Brak |
| **LEDFx** | ~2-4% | ~3-5% | +1-2% |

### Memory Usage

| Silnik | State | Audio | Total |
|--------|-------|-------|-------|
| **WLED** | ~100 bytes | 0 | ~100 bytes |
| **LEDFx** | ~100 bytes | 0* | ~100 bytes |

*Audio metrics są współdzielone, nie duplikowane

## Wnioski

### Kluczowe różnice:

1. **Audio-Reaktywność:**
   - WLED: ❌ Brak
   - LEDFx: ✅ Pełna

2. **Dostępne Efekty:**
   - WLED: 30+ efektów wizualnych
   - LEDFx: 10+ efektów audio-reactive

3. **Parametry:**
   - WLED: Podstawowe (speed, intensity, colors)
   - LEDFx: Zaawansowane (audio settings, frequency bands)

4. **Wydajność:**
   - WLED: Niższa (1-3% CPU)
   - LEDFx: Wyższa (3-5% CPU z audio)

5. **Zastosowanie:**
   - WLED: Efekty wizualne, bez audio
   - LEDFx: Audio-reactive effects, z muzyką

### Rekomendacja:

- **Używaj WLED** dla efektów wizualnych bez audio
- **Używaj LEDFx** dla efektów audio-reactive z muzyką
- **Mieszaj oba** dla różnych segmentów (niektóre z audio, niektóre bez)
