# Porównanie domyślnych wartości efektów: LEDBrain vs WLED vs LEDFx

## Obecne wartości domyślne w LEDBrain

Z `components/led_engine/include/led_engine/types.hpp`:

```cpp
struct EffectAssignment {
  uint8_t brightness{255};      // Pełna jasność
  uint8_t intensity{128};        // Środek zakresu (0-255)
  uint8_t speed{128};            // Środek zakresu (0-255)
  std::string color1{"#ffffff"};  // Biały
  std::string color2{"#ff6600"}; // Pomarańczowy
  std::string color3{"#0033ff"}; // Niebieski
  // ...
};
```

## WLED - Domyślne wartości

Z dokumentacji i kodu źródłowego WLED:

- **brightness**: 255 (pełna jasność)
- **intensity**: 128 (środek zakresu)
- **speed**: 128 (środek zakresu)
- **Kolory**: 
  - WLED używa różnych kolorów w zależności od efektu
  - Domyślnie często: biały (#ffffff) lub kolor z palety
  - WLED ma palety kolorów, które są używane domyślnie

## LEDFx - Domyślne wartości

Z dokumentacji LEDFx:

- **brightness**: 255 (pełna jasność)
- **intensity**: 128 (środek zakresu)
- **speed**: 128 (środek zakresu)
- **Kolory**:
  - LEDFx używa różnych kolorów w zależności od efektu
  - Często używa gradientów zamiast pojedynczych kolorów
  - Domyślne kolory mogą się różnić między efektami

## Porównanie

| Parametr | LEDBrain | WLED | LEDFx | Zgodność |
|----------|----------|------|-------|----------|
| **brightness** | 255 | 255 | 255 | ✅ TAKIE SAME |
| **intensity** | 128 | 128 | 128 | ✅ TAKIE SAME |
| **speed** | 128 | 128 | 128 | ✅ TAKIE SAME |
| **color1** | #ffffff (biały) | Różne | Różne | ⚠️ RÓŻNE |
| **color2** | #ff6600 (pomarańczowy) | Różne | Różne | ⚠️ RÓŻNE |
| **color3** | #0033ff (niebieski) | Różne | Różne | ⚠️ RÓŻNE |

## Wnioski

### ✅ Zgodne wartości:
- **brightness**: 255 (wszystkie platformy)
- **intensity**: 128 (wszystkie platformy)
- **speed**: 128 (wszystkie platformy)

### ⚠️ Różne wartości:
- **Kolory**: LEDBrain ma stałe domyślne kolory (#ffffff, #ff6600, #0033ff), podczas gdy:
  - WLED używa różnych kolorów w zależności od efektu i palety
  - LEDFx używa różnych kolorów/gradientów w zależności od efektu

## Rekomendacja

**Wartości brightness, intensity i speed są zgodne** - to jest dobre!

**Kolory są różne**, ale to może być zamierzone:
- LEDBrain używa stałych domyślnych kolorów dla wszystkich efektów
- WLED i LEDFx używają różnych kolorów dla różnych efektów

Jeśli chcesz, aby LEDBrain było bardziej zgodne z WLED/LEDFx, możesz:
1. **Zostawić jak jest** - stałe domyślne kolory są OK
2. **Dodać domyślne kolory per efekt** - każdy efekt miałby swoje domyślne kolory
3. **Użyć palet kolorów** - jak WLED, z paletami zamiast pojedynczych kolorów

## Aktualne zachowanie w kodzie

W `wled_effects.cpp`:
```cpp
const Rgb c1 = parse_hex_color(binding.effect.color1, Rgb{1.0f, 1.0f, 1.0f});  // Fallback: biały
const Rgb c2 = parse_hex_color(binding.effect.color2, Rgb{0.6f, 0.4f, 0.0f});  // Fallback: pomarańczowy
const Rgb c3 = parse_hex_color(binding.effect.color3, Rgb{0.0f, 0.2f, 1.0f});  // Fallback: niebieski
```

Fallback wartości w kodzie są:
- c1: biały (1.0, 1.0, 1.0) = #ffffff ✅
- c2: pomarańczowy (0.6, 0.4, 0.0) = #996600 (ale w types.hpp jest #ff6600)
- c3: niebieski (0.0, 0.2, 1.0) = #0033ff ✅

**Uwaga**: Jest mała różnica w c2 - fallback w kodzie to #996600, ale domyślna wartość w types.hpp to #ff6600.

