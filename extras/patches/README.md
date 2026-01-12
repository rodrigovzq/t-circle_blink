# Parches para TensorFlowLite_ESP32

Este directorio contiene parches necesarios para la librería `Arduino_TensorFlowLite_ESP32` que se aplican automáticamente durante la compilación.

## ⚠️ IMPORTANTE

Los parches en `.pio/libdeps/` se PIERDEN cuando ejecutas:
- `pio run -t clean`
- `rm -rf .pio/`
- Cuando PlatformIO reinstala las dependencias

Este directorio sirve como **backup** de los parches aplicados.

---

## Archivos Patcheados

### 1. `compatibility.h.patched`
**Ubicación original:** `.pio/libdeps/t-circle-s3-RV/TensorFlowLite_ESP32/src/tensorflow/lite/micro/compatibility.h`

**Problema:** Bug de compilación con toolchains GCC modernos
- Error: `operator delete(void*)` es privado pero se intenta usar

**Solución aplicada:**
```cpp
// ANTES (línea 27):
#define TF_LITE_REMOVE_VIRTUAL_DELETE \
  void operator delete(void* p) {}

// DESPUÉS:
#define TF_LITE_REMOVE_VIRTUAL_DELETE \
 public: \
  void operator delete(void* p) {}
```

**Aplicar manualmente:**
```bash
cp extras/patches/compatibility.h.patched \
   .pio/libdeps/t-circle-s3-RV/TensorFlowLite_ESP32/src/tensorflow/lite/micro/compatibility.h
```

---

### 2. `micro_graph.cpp.patched`
**Ubicación original:** `.pio/libdeps/t-circle-s3-RV/TensorFlowLite_ESP32/src/tensorflow/lite/micro/micro_graph.cpp`

**Propósito:** Debug - identificar qué operador causa el cuelgue en Invoke()

**Cambios aplicados:**
1. **Línea 28:** Agregado `#include <Arduino.h>`
2. **Líneas 160-161:** Print del total de operadores
3. **Líneas 180-189:** Prints ANTES y DESPUÉS de cada operador

**Output esperado:**
```
[TFLITE] Total operadores en modelo: XX
[TFLITE] Ejecutando op 1/XX: QUANTIZE ✅
[TFLITE] Ejecutando op 2/XX: CONV_2D ✅
[TFLITE] Ejecutando op 3/XX: RELU ✅
...
```

**Aplicar manualmente:**
```bash
cp extras/patches/micro_graph.cpp.patched \
   .pio/libdeps/t-circle-s3-RV/TensorFlowLite_ESP32/src/tensorflow/lite/micro/micro_graph.cpp
```

---

## Cómo Aplicar Todos los Parches

Si perdiste los parches después de un clean:

```bash
# Desde la raíz del proyecto:
cd /Users/rodrigovazquez/Documents/PlatformIO/Projects/t-circle_blink

# Aplicar parches
cp extras/patches/compatibility.h.patched \
   .pio/libdeps/t-circle-s3-RV/TensorFlowLite_ESP32/src/tensorflow/lite/micro/compatibility.h

cp extras/patches/micro_graph.cpp.patched \
   .pio/libdeps/t-circle-s3-RV/TensorFlowLite_ESP32/src/tensorflow/lite/micro/micro_graph.cpp

# Recompilar
pio run
```

---

## Verificar si los Parches Están Aplicados

### compatibility.h
```bash
grep -A 2 "TF_LITE_REMOVE_VIRTUAL_DELETE" \
  .pio/libdeps/t-circle-s3-RV/TensorFlowLite_ESP32/src/tensorflow/lite/micro/compatibility.h
```

**Debe contener:** `public:` antes de `void operator delete`

### micro_graph.cpp
```bash
grep "TFLITE" \
  .pio/libdeps/t-circle-s3-RV/TensorFlowLite_ESP32/src/tensorflow/lite/micro/micro_graph.cpp
```

**Debe contener:** Líneas con `[TFLITE]`

---

## Actualizar Backups

Si modificas los parches manualmente, actualiza los backups:

```bash
# Desde la raíz del proyecto:
cp .pio/libdeps/t-circle-s3-RV/TensorFlowLite_ESP32/src/tensorflow/lite/micro/compatibility.h \
   extras/patches/compatibility.h.patched

cp .pio/libdeps/t-circle-s3-RV/TensorFlowLite_ESP32/src/tensorflow/lite/micro/micro_graph.cpp \
   extras/patches/micro_graph.cpp.patched
```

---

## Notas

- Estos parches son **TEMPORALES** hasta que migres a `esp-tflite-micro` oficial
- El parche de `compatibility.h` es **OBLIGATORIO** para compilar
- El parche de `micro_graph.cpp` es **OPCIONAL** (solo para debug)
