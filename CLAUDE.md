# MoodLink - Contexto del Proyecto

## Descripcion
Dispositivo de reconocimiento de emociones por voz usando ESP32-S3 (T-Circle) con modelo TFLite.

## Pipeline
```
Audio 4s (44.1kHz) -> Normalizar -1dB -> MFCC (40x100) -> TFLite INT8 -> 7 emociones
```

## Estado Actual
- **Branch**: `test/v5.3-profiling-csv`
- **Test completado**: Test 5.1 (modular) + Test 5.3 (profiling CSV)
- **Siguiente**: Test 6 - Optimizacion

## Estructura del Codigo (v5.1+)

```
src/
├── config.h              # Constantes del pipeline
├── audio_capture.h/cpp   # Captura I2S, normalizacion
├── mfcc_extractor.h/cpp  # Extraccion de MFCCs
├── emotion_model.h/cpp   # Inferencia TFLite
├── profiler.h/cpp        # Metricas y CSV (v5.3)
└── main.cpp              # Orquestador principal
```

## Resultados Test 5 (22-Ene-2026)

### Pipeline Funcional
| Etapa | Tiempo |
|-------|--------|
| Captura audio (I2S real) | 4009 ms |
| Init matrices MFCC | 54 ms |
| Extraer MFCCs | 713 ms |
| Invoke() inferencia | 5603 ms |
| **CICLO TOTAL** | **~10.5 seg** |

### Memoria (PSRAM)
| Componente | Tamano |
|------------|--------|
| Total consumida | 742 KB (9.1%) |
| Libre | 7447 KB |

## Configuracion Hardware

### Microfono I2S (MSM261S4030H0R)
```cpp
// Pines (de pin_config.h para T_Circle_S3_V1_0)
MSM261_BCLK = GPIO7
MSM261_WS   = GPIO9
MSM261_DATA = GPIO8
```

### Parametros de Audio
```cpp
SAMPLE_RATE       44100
AUDIO_DURATION    4 segundos
AUDIO_SAMPLES     176400
```

### Parametros MFCC
```cpp
N_MFCC            40
N_FRAMES          100
N_MELS            40
N_FFT             2048
HOP_LENGTH        1761
MFCC_MEAN         -8.0306f
MFCC_STD          82.2183f
```

### TFLite
```cpp
kTensorArenaSize  156 KB (minimo 130 KB + 20%)
Input shape       [1, 40, 100, 1] INT8
Output shape      [1, 7] INT8
Emociones         anger, disgust, fear, happy, neutral, sad, surprise
```

## Profiling CSV (Test 5.3)

### Comandos Serial
| Comando | Accion |
|---------|--------|
| `d` | Dump CSV al Serial |
| `r` | Reset CSV |
| `c` | Count iteraciones |
| `p` | Pause/resume |
| `s` | Skip espera |

### Columnas CSV
```
iteration, timestamp_ms, psram_free_kb, psram_used_kb, dram_free_kb,
time_capture_ms, time_normalize_ms, time_mfcc_ms, time_inference_ms, time_total_ms,
audio_rms, audio_peak_pos, audio_peak_neg, emotion_index, confidence
```

## Plan de Tests
| Test | Descripcion | Status |
|------|-------------|--------|
| Test 1-3 | Versiones anteriores | Completados |
| Test 4 | Memory Profiling | Completado |
| Test 5 | Pipeline con audio real | Completado |
| Test 5.1 | Refactor modular | **COMPLETADO** |
| Test 5.3 | Profiling con CSV | **COMPLETADO** |
| Test 6 | Optimizacion | **PENDIENTE** |
| Test 7 | Producto final | Pendiente |

## Test 6 - Optimizacion (Proximo)

### Objetivo
Reducir tiempo y memoria.

### Estrategias
1. Reducir kTensorArena al minimo
2. Evaluar ESP-NN acceleration
3. Procesar MFCC por bloques
4. Reusar buffers

### Criterios de exito
- Memoria < 1.5MB
- Tiempo total (MFCC + inferencia) < 8s

### Preparacion
1. Correr Test 5.3 varias iteraciones
2. Exportar CSV con comando `d`
3. Analizar baseline de tiempos y memoria

## Compilacion

```bash
# Compilar
pio run

# Subir
pio run -t upload

# Monitor (con input para comandos)
pio device monitor
```

## Git
```bash
# Branch actual
git branch  # test/v5.3-profiling-csv

# Branches disponibles
git branch -a

# Push
git push origin test/v5.3-profiling-csv
```

## Archivos de Documentacion
- `docs/RESUMEN_SESION_2026-01-25.md` - Sesion actual
- `docs/RESUMEN_SESION_2026-01-22.md` - Test 5
- `docs/PLAN_TESTEO_V4.md` - Plan detallado de tests
