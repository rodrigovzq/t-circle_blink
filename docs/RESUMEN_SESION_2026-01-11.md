# Changelog - MoodLink v3.0 Test

## V3.0 - Test con modelo optimizado (2026-01-11)

### Objetivo
Validar modelo optimizado de reconocimiento de emociones con inferencia dummy (datos sintéticos).

### Configuración
- **Modelo**: `ser_202601_optimized_int8.tflite` (36KB, reducción 98% vs v2.2)
- **Input**: 40x100 MFCCs (reducción 91% vs v2.2: 44,160 → 4,000 valores)
- **Emociones**: 7 clases normalizadas (anger, disgust, fear, happy, neutral, sad, surprise)
- **Operadores TFLite**: 11 (incluye MEAN para GlobalAveragePooling2D)
- **Arena**: 1MB PSRAM

### Resultados
- **Tiempo total**: 7.84 segundos
  - Inicialización (FASE 1): 28 ms
    - LittleFS mount: 3 ms
    - Load modelo: 20 ms
    - AllocateTensors: 2 ms
  - Inferencia (FASE 2): 5.6 segundos
    - Fill input: 90 ms
    - Invoke: 5602 ms (98.4% del tiempo total)

- **Mejora vs v2.2**: 60x más rápido (5.6s vs ~6 min)
- **Bottleneck identificado**: Invoke() en PSRAM sin aceleración

### Fixes críticos
- Normalización de labels: 12 → 7 emociones
- Fix `operator delete` privado en TensorFlowLite_ESP32/compatibility.h
- Fix nullptr error_reporter → static MicroErrorReporter
- Agregado operador MEAN faltante

---

## V4.0 - Intenciones

### Nuevas capacidades
1. **Captura de audio real** desde micrófono I2S
2. **Extracción de MFCCs** on-device (40 coefs x 100 frames)
3. **Normalización** con parámetros de entrenamiento:
   - Mean: -8.0306
   - Std: 82.2183
4. **Predicción en tiempo real** de emoción desde voz

### Optimización objetivo
- **Aceleración ESP-NN**: aprovechar instrucciones SIMD del ESP32-S3
- **Target**: <1 segundo de inferencia (mejora 10-20x esperada)
- **Pipeline completo**: audio → MFCC → inferencia → resultado en <2s
