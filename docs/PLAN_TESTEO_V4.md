# Plan de Testeo MoodLink - Tests 4 a 7

## Objetivo Final
Pipeline completo en loop: **Audio 4s → MFCC → Inferencia → UI**

## Tests: 4 (Memory) → 5 (Pipeline) → 6 (Optimización) → 7 (Producto Final)

---

## Elementos en Memoria del Pipeline

| Componente | Elemento | Tipo | Fórmula | Estimado v3.0 |
|------------|----------|------|---------|---------------|
| **Audio** | audioBuffer | int16_t[] | SAMPLE_RATE × 4s | 352 KB |
| **MFCC** | mfccOutput | float[] | 40 × 100 | 16 KB |
| | vReal | float[] | N_FFT (2048) | 8 KB |
| | vImag | float[] | N_FFT (2048) | 8 KB |
| | melFilterbank | float[] | N_MELS × (N_FFT/2+1) | ~164 KB* |
| | dctMatrix | float[] | N_MFCC × N_MELS | ~6.4 KB* |
| | hammingWindow | float[] | N_FFT | 8 KB |
| **TFLite** | modelo | uint8_t[] | archivo .tflite | 36 KB |
| | kTensorArena | uint8_t[] | configurable | 1024 KB (actual) |
| | MicroInterpreter | objeto | instancia | ~1 KB |
| | MicroMutableOpResolver | objeto | 11 ops | ~2 KB |
| | input_tensor | TfLiteTensor* | 40×100×1 float | 16 KB |
| | output_tensor | TfLiteTensor* | 7 float | 28 B |

*Con parámetros v3.0: N_MFCC=40, N_MELS=40, N_FFT=2048

**Total estimado**: ~1.6 MB PSRAM

---

## Tests (4 pruebas)

### Test 4 - Memory Profiling
**Objetivo**: Medir memoria exacta de cada componente del pipeline.

**Método**:
1. Medir PSRAM libre antes/después de cada allocación
2. Medir heap interno libre
3. Calcular tamaño exacto de cada buffer
4. Determinar tamaño mínimo de kTensorArena (binary search)

**Output esperado**:
```
═══════════════════════════════════════════════
MEMORY PROFILING - MoodLink v4.0
═══════════════════════════════════════════════
PSRAM Total:     8192 KB
PSRAM Libre:     XXXX KB

ALLOCACIONES:
├── Audio
│   └── audioBuffer:      XXX KB (int16_t[176400])
├── MFCC
│   ├── mfccOutput:       XXX KB (float[4000])
│   ├── vReal:            XXX KB (float[2048])
│   ├── vImag:            XXX KB (float[2048])
│   ├── melFilterbank:    XXX KB (float[40×1025])
│   ├── dctMatrix:        XXX KB (float[40×40])
│   └── hammingWindow:    XXX KB (float[2048])
├── TFLite
│   ├── modelo:           XXX KB
│   ├── kTensorArena:     XXX KB (mínimo funcional)
│   └── interpreter+resolver: XXX KB
└── TOTAL USADO:          XXX KB

Arena óptimo encontrado: XXX KB
═══════════════════════════════════════════════
```

**Criterio de éxito**: Documentar uso real < 2MB PSRAM

---

### Test 5 - Pipeline Integrado
**Objetivo**: Ejecutar pipeline completo con métricas de tiempo.

**Método**:
1. Adaptar parámetros MFCC a v3.0 (40 coefs, 100 frames)
2. Integrar captura → MFCC → inferencia
3. Medir tiempo de cada fase
4. Validar output de inferencia

**Parámetros v3.0**:
```cpp
#define N_MFCC 40        // (era 128)
#define N_FRAMES 100     // (era 345)
#define N_MELS 40        // (era 128)
#define MFCC_MEAN -8.0306f
#define MFCC_STD 82.2183f
```

**Output esperado**:
```
═══════════════════════════════════════════════
PIPELINE COMPLETO - MoodLink v4.0
═══════════════════════════════════════════════
FASE 1 - Captura Audio
├── Duración:     4000 ms (fijo)
├── Samples:      176400
└── Buffer:       352 KB

FASE 2 - Extracción MFCC
├── Tiempo:       XXXX ms
├── Frames:       100
├── Coeficientes: 40
└── Output:       16 KB

FASE 3 - Inferencia TFLite
├── Fill input:   XX ms
├── Invoke:       XXXX ms
└── Read output:  XX ms

RESULTADO:
├── Emoción:      XXXXXXX
├── Confianza:    XX.X%
└── Tiempo total: XXXX ms (sin contar captura)

Pipeline: Audio(4s) + MFCC(Xs) + Inferencia(Xs) = X.Xs
═══════════════════════════════════════════════
```

**Criterio de éxito**: Pipeline funcional end-to-end

---

### Test 6 - Optimización Pipeline
**Objetivo**: Reducir tiempo y memoria donde sea posible.

**Estrategias a evaluar**:

| Optimización | Impacto Memoria | Impacto Tiempo |
|--------------|-----------------|----------------|
| Reducir kTensorArena al mínimo | ↓ hasta 500KB | neutral |
| Mover modelo a DRAM (si cabe) | ↓ PSRAM | ↑ velocidad |
| Procesar MFCC por bloques | ↓ buffers temp | neutral |
| Reusar buffers audio/mfcc | ↓ ~350KB | neutral |
| ESP-NN acceleration | neutral | ↓↓ tiempo |

**Output esperado**:
```
═══════════════════════════════════════════════
OPTIMIZACIÓN - MoodLink v4.0
═══════════════════════════════════════════════
                    ANTES      DESPUÉS    MEJORA
Memoria total:      XXXX KB    XXXX KB    -XX%
Tiempo inferencia:  XXXX ms    XXXX ms    -XX%
Tiempo MFCC:        XXXX ms    XXXX ms    -XX%
Arena size:         1024 KB    XXX KB     -XX%

Configuración óptima guardada.
═══════════════════════════════════════════════
```

**Criterio de éxito**:
- Memoria < 1.5MB
- Tiempo total (MFCC + inferencia) < 8s

---

### Test 7 - Producto Final
**Objetivo**: Sistema completo con UI en loop permanente.

**Componentes**:
1. Loop principal continuo
2. LED indicador de estado (RGB)
3. Display de emoción detectada
4. Métricas en Serial (opcional, toggle)

**Estados del LED**:
| Color | Estado |
|-------|--------|
| 🟢 Verde | Listo/Idle |
| 🔴 Rojo | Grabando |
| 🟡 Amarillo | Procesando MFCC |
| 🔵 Azul | Inferencia |
| 🟣 Púrpura | Resultado (pulso) |

**Flow del loop**:
```
┌─────────────────────────────────────────┐
│              INICIO                      │
└─────────────┬───────────────────────────┘
              ▼
┌─────────────────────────────────────────┐
│  🟢 IDLE - Esperando trigger             │
│     (botón o automático)                 │
└─────────────┬───────────────────────────┘
              ▼
┌─────────────────────────────────────────┐
│  🔴 GRABANDO - 4 segundos                │
└─────────────┬───────────────────────────┘
              ▼
┌─────────────────────────────────────────┐
│  🟡 MFCC - Extrayendo features           │
└─────────────┬───────────────────────────┘
              ▼
┌─────────────────────────────────────────┐
│  🔵 INFERENCIA - Prediciendo emoción     │
└─────────────┬───────────────────────────┘
              ▼
┌─────────────────────────────────────────┐
│  🟣 RESULTADO - Mostrar emoción          │
│     (2s de display)                      │
└─────────────┬───────────────────────────┘
              │
              └──────────► (volver a IDLE)
```

**Output esperado**:
```
═══════════════════════════════════════════════
MoodLink v4.0 - SISTEMA ACTIVO
═══════════════════════════════════════════════
Memoria PSRAM: XXXX/8192 KB (XX%)
Modo: Loop continuo

[12:34:56] 🎤 Grabando...
[12:35:00] 🔄 Procesando MFCC... (XXXms)
[12:35:01] 🧠 Inferencia... (XXXms)
[12:35:02] ✨ Emoción: HAPPY (87.3%)

[12:35:04] 🎤 Grabando...
...
═══════════════════════════════════════════════
```

**Criterio de éxito**:
- Loop estable por 10+ ciclos sin memory leak
- Tiempo total por ciclo < 12s (4s audio + 8s proceso)

---

## Resumen de Complejidad

### Complejidad Temporal
| Fase | Complejidad | Estimado v3.0 |
|------|-------------|---------------|
| Captura audio | O(n) donde n=samples | 4000ms (fijo) |
| MFCC | O(frames × (FFT + Mel + DCT)) | ~500-2000ms |
| Inferencia | O(modelo) | ~5600ms (actual) |
| **Total** | - | ~10-12s por ciclo |

### Complejidad Espacial
| Componente | Complejidad | Bytes v3.0 |
|------------|-------------|------------|
| Audio | O(sample_rate × duration) | 352KB |
| MFCC buffers | O(N_FFT + N_MELS²) | ~200KB |
| TFLite | O(arena_size + model) | ~1060KB |
| **Total** | - | ~1.6MB |

---

## Orden de Ejecución

```
Test 4 (Memory) ──► Test 5 (Pipeline) ──► Test 6 (Optim) ──► Test 7 (Final)
      │                   │                    │                   │
      ▼                   ▼                    ▼                   ▼
   Baseline          Funcional            Optimizado           Producto
   memoria           end-to-end           reducido             completo
```

Cada test depende del anterior. No avanzar hasta que el test actual pase sus criterios.
