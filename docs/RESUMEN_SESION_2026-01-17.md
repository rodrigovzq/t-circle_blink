# Resumen Sesión - 17 Enero 2026

## MoodLink - Test 4: Memory Profiling + Pipeline Dummy

**Branch:** `test/v4-memory-profiling`
**Modelo:** `ser_202601_optimized_int8.tflite` (36 KB)

---

## Objetivo del Test 4

Medir el uso exacto de memoria de cada componente del pipeline y validar funcionamiento end-to-end con datos sintéticos (ruido aleatorio).

---

## Resultados Obtenidos

### Memoria PSRAM

| Componente | Tamaño | Descripción |
|------------|--------|-------------|
| audioBuffer | 344.5 KB | int16_t[176400] - 4s @ 44.1kHz |
| melFilterbank | 160.2 KB | float[40×1025] - Filtros Mel |
| kTensorArena | 156.0 KB | Workspace TFLite (mínimo 130KB + 20%) |
| modelBuffer | 35.5 KB | Modelo cargado en PSRAM |
| mfccOutput | 15.6 KB | float[40×100] - Features extraídas |
| vReal + vImag | 16.0 KB | Buffers FFT |
| hammingWindow | 8.0 KB | Ventana para FFT |
| dctMatrix | 6.2 KB | Matriz DCT |
| **TOTAL** | **742 KB** | **9.1% de 8 MB disponibles** |

### Tiempos del Pipeline

| Etapa | Tiempo | % del Ciclo |
|-------|--------|-------------|
| Captura audio (simulada) | 3938 ms | 38.0% |
| Init matrices MFCC | 54 ms | 0.5% |
| Extraer MFCCs | 713 ms | 6.9% |
| Fill input tensor | 2 ms | 0.0% |
| Invoke() inferencia | 5602 ms | 54.0% |
| **TOTAL CICLO** | **~10.4 seg** | 100% |

### Arena Óptimo (Binary Search)

```
512 KB → OK
384 KB → OK
256 KB → OK
192 KB → OK
160 KB → OK
144 KB → OK
136 KB → OK
132 KB → OK
130 KB → OK ← Mínimo funcional
128 KB → FALLO

Resultado: 130 KB mínimo + 20% margen = 156 KB recomendado
```

### Parámetros Optimizados v3.0

```cpp
#define SAMPLE_RATE       44100
#define AUDIO_DURATION    4        // segundos
#define N_MFCC            40
#define N_FRAMES          100
#define N_MELS            40
#define N_FFT             2048
#define HOP_LENGTH        1761
#define kTensorArenaSize  (156 * 1024)

// Normalización
#define MFCC_MEAN         -8.0306f
#define MFCC_STD          82.2183f
```

---

## Criterios de Éxito - Test 4

| Criterio | Objetivo | Resultado | Estado |
|----------|----------|-----------|--------|
| Memoria PSRAM | < 2 MB | 742 KB | ✅ |
| Pipeline funcional | End-to-end | Completado | ✅ |
| Arena óptimo | Encontrar mínimo | 130 KB | ✅ |
| Sin memory leaks | Estable | Verificado | ✅ |

---

## Próximo: Test 5 - Pipeline con Audio Real

### Objetivo

Reemplazar el ruido aleatorio por **captura de audio real del micrófono** y ejecutar el pipeline completo midiendo:

1. **Temporalidad**: Tiempo real de cada etapa con audio del micrófono
2. **Espacialidad**: Verificar que el uso de memoria se mantiene estable

### Cambios Requeridos

1. **Configurar I2S** para el micrófono PDM del T-Circle S3
2. **Implementar captura de audio** de 4 segundos reales
3. **Validar calidad del audio** capturado (niveles, ruido)
4. **Ejecutar pipeline completo** con datos reales
5. **Medir tiempos** de cada fase

### Componentes de Hardware

- **Micrófono**: MSM261S4030H0R (PDM)
- **Pines I2S**:
  - CLK: GPIO42
  - DATA: GPIO41

### Output Esperado Test 5

```
═══════════════════════════════════════════════
PIPELINE COMPLETO - MoodLink v4.0
═══════════════════════════════════════════════
FASE 1 - Captura Audio (REAL)
├── Duración:     4000 ms
├── Samples:      176400
├── Nivel RMS:    XXXX
└── Buffer:       344 KB

FASE 2 - Extracción MFCC
├── Tiempo:       ~700 ms
├── Frames:       100
└── Coeficientes: 40

FASE 3 - Inferencia TFLite
├── Invoke:       ~5600 ms
└── Resultado:    [emoción] (XX.X%)

CICLO TOTAL: ~10.4 seg
═══════════════════════════════════════════════
```

### Criterios de Éxito Test 5

| Criterio | Objetivo |
|----------|----------|
| Captura I2S funcional | Audio sin glitches |
| Nivel de señal | RMS > umbral mínimo |
| Pipeline completo | End-to-end con audio real |
| Tiempo total | < 12 segundos por ciclo |
| Memoria estable | Sin memory leaks |

---

## Archivos Relevantes

| Archivo | Descripción |
|---------|-------------|
| `src/main.cpp` | Código del test actual |
| `data/ser_202601_optimized_int8.tflite` | Modelo TFLite INT8 |
| `docs/PLAN_TESTEO_V4.md` | Plan completo de tests 4-7 |
| `test/4_memory-profiling/` | Backup del Test 4 |

---

## Notas Técnicas

- El **87% del tiempo** de procesamiento está en `Invoke()` - difícil de optimizar sin cambiar modelo
- **PSRAM tiene 63% libre** - hay margen para features adicionales
- El **HOP_LENGTH=1761** fue calculado para obtener exactamente 100 frames de 176400 samples
- La inferencia produce resultados coherentes incluso con ruido (predice "anger" por la naturaleza del ruido aleatorio)
