# Resumen Sesion - 22 Enero 2026

## MoodLink - Test 5: Pipeline con Audio Real

**Branch:** `test/v5-real-audio-pipeline`
**Modelo:** `ser_202601_optimized_int8.tflite` (36 KB)
**Commit:** `2bc45dd feat: Test 5 - Pipeline con audio real del microfono`

---

## Objetivo del Test 5

Reemplazar el ruido aleatorio del Test 4 por **captura de audio real del microfono** y ejecutar el pipeline completo.

---

## Cambios Implementados

### 1. Captura de Audio I2S
- Biblioteca: `Arduino_DriveBus_Library` con `Arduino_MEMS`
- Microfono: MSM261S4030H0R
- Pines: BCLK=GPIO7, WS=GPIO9, DATA=GPIO8
- Sample rate: 44,100 Hz
- Duracion: 4 segundos
- Samples: 176,400

### 2. Normalizacion de Audio
- Target: -1dB (pico maximo = 29,203 de 32,767)
- Automatica post-captura
- Calcula factor de escala y aplica a todas las muestras

### 3. Estadisticas de Audio
- RMS (nivel de energia)
- Picos positivo/negativo
- Zero crossings
- Validacion de nivel de senal

### 4. Correccion de Bugs
- Logging de progreso: ahora muestra 1 linea por segundo (antes mostraba cientos)
- platformio.ini: movido `boards_dir` a seccion `[platformio]`, eliminados defines duplicados

---

## Resultados Obtenidos

### Captura y Normalizacion

| Metrica | Antes Norm. | Despues Norm. |
|---------|-------------|---------------|
| RMS | 542.7 | 4185.9 |
| Picos | [-3218, 3786] | [-24821, 29203] |
| Ganancia aplicada | - | x7.71 (17.7 dB) |

### Tiempos del Pipeline

| Etapa | Tiempo | % Total |
|-------|--------|---------|
| Captura audio (REAL) | 4009 ms | 38.0% |
| Init matrices MFCC | 54 ms | 0.5% |
| Extraer MFCCs | 713 ms | 6.8% |
| Fill input tensor | 2 ms | 0.0% |
| Invoke() inferencia | 5603 ms | 53.1% |
| **TOTAL** | **10542 ms** | 100% |

### Memoria (sin cambios desde Test 4)

| Componente | Tamano |
|------------|--------|
| PSRAM consumida | 742 KB (9.1%) |
| PSRAM libre | 7447 KB |
| DRAM consumida | 2 KB |

---

## Criterios de Exito - Test 5

| Criterio | Objetivo | Resultado | Estado |
|----------|----------|-----------|--------|
| Captura I2S funcional | Audio sin glitches | 176,400 samples OK | PASS |
| Normalizacion | -1dB automatica | Pico 29203 OK | PASS |
| Nivel de senal | RMS > umbral | 4185.9 post-norm | PASS |
| Pipeline completo | End-to-end | Funcionando | PASS |
| Tiempo total | < 12 segundos | 10.5 seg | PASS |
| Memoria estable | Sin memory leaks | Verificado | PASS |

---

## Problema Conocido: Prediccion Incorrecta

El modelo predice **"anger"** independientemente del tono usado (incluso con tono feliz).

**Causa probable:**
- Modelo entrenado con dataset diferente (idioma, voces)
- Parametros de normalizacion MFCC pueden no coincidir exactamente

**Solucion planificada:**
- Dejar el pipeline modular para intercambiar modelos `.tflite`
- Resolver en Test 6 o posterior

---

## Archivos Modificados

| Archivo | Cambios |
|---------|---------|
| `src/main.cpp` | +230 lineas: I2S, normalizacion, estadisticas |
| `platformio.ini` | Limpieza de warnings, reorganizacion |

---

## Proximo: Test 6 - Optimizacion

### Objetivos Posibles
1. Reducir tiempo de Invoke() (53% del ciclo)
2. Optimizar extraccion MFCC
3. Probar diferentes modelos de red neuronal
4. Implementar loop continuo de deteccion

### Notas Tecnicas
- El 53% del tiempo esta en `Invoke()` - dificil de optimizar sin cambiar modelo
- PSRAM tiene 91% libre - hay margen para features adicionales
- El pipeline es modular: se puede cambiar el modelo sin modificar el codigo

---

## Comandos Utiles

```bash
# Compilar
pio run

# Subir
pio run -t upload

# Monitor serial
pio device monitor

# Ver rama actual
git branch

# Push cambios
git push origin test/v5-real-audio-pipeline
```

---

## Estado del Proyecto

```
Tests Completados:
[x] Test 1-3: Versiones anteriores
[x] Test 4: Memory Profiling + Pipeline Dummy
[x] Test 5: Pipeline con Audio Real  <-- COMPLETADO HOY

Tests Pendientes:
[ ] Test 6: Optimizacion
[ ] Test 7: Producto final (loop continuo)
```
