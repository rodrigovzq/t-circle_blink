# MoodLink - Contexto del Proyecto

## Descripcion
Dispositivo de reconocimiento de emociones por voz usando ESP32-S3 (T-Circle) con modelo TFLite.

## Pipeline
```
Audio 4s (44.1kHz) → MFCC (40x100) → TFLite INT8 → 7 emociones
```

## Estado Actual
- **Branch**: `test/v4-memory-profiling`
- **Test completado**: Test 4 - Memory Profiling + Pipeline Dummy
- **Siguiente**: Test 5 - Pipeline con audio real del micrófono

## Resultados Test 4 (17-Ene-2025)

### Memoria (PSRAM)
| Componente | Tamaño |
|------------|--------|
| audioBuffer | 344.5 KB |
| MFCC buffers | 206.0 KB |
| TFLite (modelo + arena) | 191.5 KB |
| **TOTAL** | **742 KB** (9.1% de 8 MB) |

### Tiempos
| Etapa | Tiempo |
|-------|--------|
| Captura audio | 4000 ms (fijo) |
| Init matrices | 54 ms |
| Extraer MFCCs | 713 ms |
| Invoke() | 5602 ms |
| **CICLO TOTAL** | **~10.4 seg** |

### Valores Optimizados
```cpp
#define kTensorArenaSize  (156 * 1024)  // minimo 130 KB + 20%
#define N_MFCC            40
#define N_FRAMES          100
#define N_MELS            40
#define N_FFT             2048
#define HOP_LENGTH        1761
```

## Plan de Tests
| Test | Descripcion | Status |
|------|-------------|--------|
| Test 1-3 | Versiones anteriores | Completados |
| Test 4 | Memory Profiling | COMPLETADO |
| Test 5 | Pipeline con audio real | Pendiente |
| Test 6 | Optimizacion | Pendiente |
| Test 7 | Producto final (loop continuo) | Pendiente |

## Archivos Clave
- `src/main.cpp` - Test actual
- `test/3_v3-optimized-model` - Backup del test 3
- `docs/PLAN_TESTEO_V4.md` - Plan detallado de tests 4-7
- `data/ser_202601_optimized_int8.tflite` - Modelo (36 KB)

## Modelo
- Input: `[1, 40, 100, 1]` INT8
- Output: `[1, 7]` INT8 (anger, disgust, fear, happy, neutral, sad, surprise)
- Normalizacion MFCC: mean=-8.0306, std=82.2183
