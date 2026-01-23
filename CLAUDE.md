# MoodLink - Contexto del Proyecto

## Descripcion
Dispositivo de reconocimiento de emociones por voz usando ESP32-S3 (T-Circle) con modelo TFLite.

## Pipeline
```
Audio 4s (44.1kHz) -> Normalizar -1dB -> MFCC (40x100) -> TFLite INT8 -> 7 emociones
```

## Estado Actual
- **Branch**: `test/v5-real-audio-pipeline`
- **Test completado**: Test 5 - Pipeline con Audio Real
- **Siguiente**: Test 6 - Optimizacion

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

### Normalizacion de Audio
- Automatica a -1dB post-captura
- Pico target: 29203 (de 32767)
- Calcula RMS, picos, zero crossings

### Problema Conocido
El modelo predice "anger" siempre, independiente del tono.
Causa: modelo/dataset. Solucion: cambiar modelo en Test 6+.

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

## Plan de Tests
| Test | Descripcion | Status |
|------|-------------|--------|
| Test 1-3 | Versiones anteriores | Completados |
| Test 4 | Memory Profiling | Completado |
| Test 5 | Pipeline con audio real | **COMPLETADO** |
| Test 6 | Optimizacion | Pendiente |
| Test 7 | Producto final (loop continuo) | Pendiente |

## Archivos Clave
- `src/main.cpp` - Codigo principal (Test 5)
- `lib/Mylibrary/pin_config.h` - Pines del hardware
- `data/ser_202601_optimized_int8.tflite` - Modelo (36 KB)
- `docs/RESUMEN_SESION_2026-01-22.md` - Ultima sesion
- `docs/PLAN_TESTEO_V4.md` - Plan detallado de tests

## Compilacion

```bash
# Compilar
pio run

# Subir
pio run -t upload

# Monitor
pio device monitor
```

## Git
```bash
# Branch actual
git branch  # test/v5-real-audio-pipeline

# Push (siempre a la branch, no a main)
git push origin test/v5-real-audio-pipeline
```
