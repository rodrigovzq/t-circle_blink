# Resumen Sesion - 25 Enero 2026

## MoodLink - Test 5.1 y 5.3

**Branches creadas:**
- `test/v5.1-modular-pipeline` - Pipeline modular
- `test/v5.3-profiling-csv` - Pipeline con profiling y CSV

---

## Test 5.1 - Pipeline Modular

### Objetivo
Refactorizar el codigo del Test 5 para que sea modular, legible y sirva de base para tests futuros.

### Cambios Implementados

**Estructura de archivos:**
```
src/
├── config.h              # Constantes del pipeline
├── audio_capture.h/cpp   # Captura I2S, normalizacion, estadisticas
├── mfcc_extractor.h/cpp  # FFT, Mel filterbank, DCT, extraccion
├── emotion_model.h/cpp   # Carga modelo, inferencia TFLite
└── main.cpp              # Orquestador (~180 lineas)
```

**Principios de diseño:**
- Cada modulo tiene responsabilidad unica
- Flujo de datos explicito (buffers pasados como parametros)
- Interfaces claras y minimas
- main.cpp se lee como pipeline de alto nivel

**Codigo del loop (simplificado):**
```cpp
void loop() {
    audio_capture(audio_buffer);
    audio_normalize(audio_buffer);
    mfcc_extract(audio_buffer, mfcc_buffer);
    EmotionResult result = model_predict(mfcc_buffer);
    print_result(result);
}
```

---

## Test 5.3 - Profiling y CSV

### Objetivo
Medir memoria y tiempos de cada etapa del pipeline, guardando datos en CSV para analisis.

### Cambios Implementados

**Nuevo modulo `profiler.h/cpp`:**
- Guarda metricas en `/profiling.csv` (LittleFS)
- Flush despues de cada escritura (no se corrompe)
- Comandos Serial para exportar/resetear

**Comandos Serial:**
| Comando | Accion |
|---------|--------|
| `d` | Dump CSV al Serial |
| `r` | Reset CSV |
| `c` | Count iteraciones |
| `p` | Pause/resume |
| `s` | Skip espera 3s |
| `h` | Help |

**Columnas del CSV:**
| Columna | Descripcion |
|---------|-------------|
| iteration | Numero de iteracion |
| timestamp_ms | Tiempo desde boot |
| psram_free_kb | PSRAM libre |
| psram_used_kb | PSRAM usada |
| dram_free_kb | DRAM libre |
| time_capture_ms | Tiempo captura audio |
| time_normalize_ms | Tiempo normalizacion |
| time_mfcc_ms | Tiempo extraccion MFCC |
| time_inference_ms | Tiempo Invoke() |
| time_total_ms | Tiempo total ciclo |
| audio_rms | RMS del audio |
| audio_peak_pos | Pico positivo |
| audio_peak_neg | Pico negativo |
| emotion_index | Indice emocion (0-6) |
| confidence | Confianza prediccion |

**Mapeo emotion_index:**
0=anger, 1=disgust, 2=fear, 3=happy, 4=neutral, 5=sad, 6=surprise

---

## Estado del Proyecto

```
Tests Completados:
[x] Test 1-3: Versiones anteriores
[x] Test 4: Memory Profiling + Pipeline Dummy
[x] Test 5: Pipeline con Audio Real
[x] Test 5.1: Refactor - Pipeline Modular  <-- HOY
[x] Test 5.3: Profiling con CSV            <-- HOY

Tests Pendientes:
[ ] Test 6: Optimizacion
[ ] Test 7: Producto final (loop continuo)
```

---

## Proximo: Test 6 - Optimizacion

### Objetivo
Reducir tiempo y memoria donde sea posible.

### Estrategias a evaluar

| Optimizacion | Impacto Memoria | Impacto Tiempo |
|--------------|-----------------|----------------|
| Reducir kTensorArena al minimo | ↓ hasta 500KB | neutral |
| Mover modelo a DRAM (si cabe) | ↓ PSRAM | ↑ velocidad |
| Procesar MFCC por bloques | ↓ buffers temp | neutral |
| Reusar buffers audio/mfcc | ↓ ~350KB | neutral |
| ESP-NN acceleration | neutral | ↓↓ tiempo |

### Criterios de exito
- Memoria < 1.5MB
- Tiempo total (MFCC + inferencia) < 8s

### Datos necesarios
Correr Test 5.3 varias iteraciones y exportar CSV para tener baseline de:
- Tiempos actuales por etapa
- Memoria actual usada
- Variabilidad entre iteraciones

---

## Comandos Utiles

```bash
# Compilar
pio run

# Subir
pio run -t upload

# Monitor con input habilitado
pio device monitor

# Ver branches
git branch -a
```

---

## Archivos Clave Actualizados

| Archivo | Descripcion |
|---------|-------------|
| `src/config.h` | Constantes del pipeline |
| `src/audio_capture.*` | Modulo de audio |
| `src/mfcc_extractor.*` | Modulo de MFCC |
| `src/emotion_model.*` | Modulo de inferencia |
| `src/profiler.*` | Modulo de profiling |
| `src/main.cpp` | Orquestador principal |
