#include <Arduino.h>
#include <LittleFS.h>
#include "tensorflow/lite/micro/micro_mutable_op_resolver.h"
#include "tensorflow/lite/micro/micro_interpreter.h"
#include "tensorflow/lite/micro/system_setup.h"
#include "tensorflow/lite/schema/schema_generated.h"
#include "esp_task_wdt.h"

// === CACHE MANAGEMENT para PSRAM ===
#include "esp_heap_caps.h"

// Declaraciones de funciones ROM de caché (ESP32-S3)
extern "C"
{
  void Cache_WriteBack_All(void);
}

#define VERSION "v3.0-optimized-model"

// ═══════════════════════════════════════════════════════════
// CONFIGURACIÓN DEL MODELO
// ═══════════════════════════════════════════════════════════
const char *MODEL_PATH = "/ser_202601_optimized_int8.tflite";

// Parámetros del modelo optimizado (40 MFCCs x 100 frames)
constexpr int EXPECTED_INPUT_HEIGHT = 40;  // MFCCs
constexpr int EXPECTED_INPUT_WIDTH = 100;  // Time frames
constexpr int EXPECTED_INPUT_CHANNELS = 1; // Mono
constexpr int EXPECTED_NUM_EMOTIONS = 7;   // 7 emociones normalizadas

// ═══════════════════════════════════════════════════════════
// TFLite globals
// ═══════════════════════════════════════════════════════════
namespace
{
  const tflite::Model *model = nullptr;
  tflite::MicroInterpreter *interpreter = nullptr;
  TfLiteTensor *input = nullptr;
  TfLiteTensor *output = nullptr;

  constexpr int kTensorArenaSize = 7 * 1024 * 1024; // 7 MB
  uint8_t *tensor_arena = nullptr;
  size_t actual_arena_size = 0;

  tflite::ErrorReporter *error_reporter = nullptr;
}

// Nombres de emociones (7 emociones normalizadas)
const char *emotion_labels[] = {
    "anger", "disgust", "fear", "happy", "neutral", "sad", "surprise"};

// ═══════════════════════════════════════════════════════════
// FUNCIÓN AUXILIAR: Extraer nombre del archivo
// ═══════════════════════════════════════════════════════════
const char *getModelFileName(const char *path)
{
  const char *lastSlash = strrchr(path, '/');
  return (lastSlash != nullptr) ? (lastSlash + 1) : path;
}

void setup()
{
  Serial.begin(115200);
  delay(2000);

  Serial.println("\n🧠 MoodLink - Test TFLite Optimized Model");
  Serial.println("════════════════════════════════════════════════════════");
  Serial.printf("📌 VERSION: %s\n", VERSION);
  Serial.printf("📦 MODELO: %s\n", getModelFileName(MODEL_PATH));
  Serial.println("════════════════════════════════════════════════════════");

  Serial.printf("\n📐 Configuración esperada:\n");
  Serial.printf("   Input: [1, %d, %d, %d] (batch, MFCCs, frames, channels)\n",
                EXPECTED_INPUT_HEIGHT, EXPECTED_INPUT_WIDTH, EXPECTED_INPUT_CHANNELS);
  Serial.printf("   Output: [1, %d] (batch, emociones)\n", EXPECTED_NUM_EMOTIONS);
  Serial.println("════════════════════════════════════════════════════════");

  // ═══════════════════════════════════════════════════════════
  // 1. Montar LittleFS
  // ═══════════════════════════════════════════════════════════
  Serial.println("\n📂 Montando LittleFS...");
  if (!LittleFS.begin(true))
  {
    Serial.println("❌ Error montando LittleFS");
    while (1)
      ;
  }
  Serial.println("✅ LittleFS OK");

  // ═══════════════════════════════════════════════════════════
  // 2. Cargar modelo
  // ═══════════════════════════════════════════════════════════
  Serial.println("\n📦 Cargando modelo...");
  File modelFile = LittleFS.open(MODEL_PATH, "r");
  if (!modelFile)
  {
    Serial.printf("❌ No se pudo abrir: %s\n", MODEL_PATH);
    Serial.println("   Verifica que el archivo existe en LittleFS");
    while (1)
      ;
  }

  size_t modelSize = modelFile.size();
  Serial.printf("   Archivo: %s\n", getModelFileName(MODEL_PATH));
  Serial.printf("   Tamaño: %d bytes (%.2f KB)\n", modelSize, modelSize / 1024.0);

  uint8_t *modelBuffer = (uint8_t *)ps_malloc(modelSize);
  if (!modelBuffer)
  {
    Serial.println("❌ No hay PSRAM para modelo");
    while (1)
      ;
  }

  modelFile.read(modelBuffer, modelSize);
  modelFile.close();
  Serial.printf("✅ Modelo cargado: %s\n", getModelFileName(MODEL_PATH));

  // ═══════════════════════════════════════════════════════════
  // 3. Inicializar TFLite
  // ═══════════════════════════════════════════════════════════
  Serial.println("\n🔧 Inicializando TFLite...");

  model = tflite::GetModel(modelBuffer);
  if (model->version() != TFLITE_SCHEMA_VERSION)
  {
    Serial.printf("❌ Versión incorrecta: modelo=%d, esperado=%d\n",
                  model->version(), TFLITE_SCHEMA_VERSION);
    while (1)
      ;
  }
  Serial.println("✅ Modelo compatible");

  // ═══════════════════════════════════════════════════════════
  // 4. Crear OpResolver con operadores del modelo optimizado
  // ═══════════════════════════════════════════════════════════
  // Modelo optimizado usa:
  // - Conv2D (3 capas)
  // - MaxPool2D (3 capas)
  // - GlobalAveragePooling2D → AveragePool2D en TFLite
  // - BatchNormalization (fusionado en Conv2D generalmente)
  // - Dense (2 capas) → FullyConnected
  // - Softmax (salida)
  // - Quantize/Dequantize (INT8)
  // - Reshape, Add, Mul (para normalización)

  static tflite::MicroMutableOpResolver<10> resolver;
  resolver.AddConv2D();
  resolver.AddMaxPool2D();
  resolver.AddAveragePool2D(); // Para GlobalAveragePooling2D
  resolver.AddFullyConnected();
  resolver.AddSoftmax();
  resolver.AddReshape();
  resolver.AddQuantize();
  resolver.AddDequantize();
  resolver.AddMul(); // Para BatchNorm
  resolver.AddAdd(); // Para BatchNorm

  Serial.println("✅ OpResolver configurado (10 operadores)");

  // ═══════════════════════════════════════════════════════════
  // 5. Reservar tensor arena
  // ═══════════════════════════════════════════════════════════
  Serial.println("\n💾 Asignando tensor arena...");

  // Intentar RAM interna primero (más rápida, sin cache issues)
  constexpr int kReducedArenaSize = 512 * 1024; // 512 KB
  tensor_arena = (uint8_t *)heap_caps_aligned_alloc(16, kReducedArenaSize,
                                                    MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT);

  if (tensor_arena)
  {
    actual_arena_size = kReducedArenaSize;
    Serial.printf("✅ Tensor arena en RAM INTERNA: %d KB\n", actual_arena_size / 1024);
    Serial.println("   ℹ️  Modelo optimizado requiere menos memoria");
  }
  else
  {
    // Fallback a PSRAM con DMA capability
    Serial.println("⚠️  RAM interna insuficiente, usando PSRAM...");

    // Intento 1: PSRAM con DMA
    tensor_arena = (uint8_t *)heap_caps_aligned_alloc(16, kTensorArenaSize,
                                                      MALLOC_CAP_SPIRAM | MALLOC_CAP_DMA);

    if (!tensor_arena)
    {
      // Intento 2: PSRAM normal
      Serial.println("   ⚠️  DMA fallback, usando PSRAM estándar...");
      tensor_arena = (uint8_t *)heap_caps_aligned_alloc(16, kTensorArenaSize, MALLOC_CAP_SPIRAM);
    }

    if (!tensor_arena)
    {
      Serial.println("❌ No hay memoria disponible para tensor arena");
      while (1)
        ;
    }
    actual_arena_size = kTensorArenaSize;
    Serial.printf("✅ Tensor arena en PSRAM: %d KB\n", actual_arena_size / 1024);
    Serial.println("   ⚠️  ADVERTENCIA: PSRAM con cache management experimental");
  }

  // ═══════════════════════════════════════════════════════════
  // 6. Crear error reporter e intérprete
  // ═══════════════════════════════════════════════════════════
  // Error reporter como nullptr (librería Arduino requiere el parámetro)
  error_reporter = nullptr;

  static tflite::MicroInterpreter static_interpreter(
      model, resolver, tensor_arena, actual_arena_size, error_reporter);
  interpreter = &static_interpreter;

  TfLiteStatus allocate_status = interpreter->AllocateTensors();
  if (allocate_status != kTfLiteOk)
  {
    Serial.println("❌ Error al asignar tensors");
    Serial.println("   Posibles causas:");
    Serial.println("   - Arena muy pequeña para el modelo");
    Serial.println("   - Operadores faltantes en OpResolver");
    while (1)
      ;
  }
  Serial.println("✅ Intérprete creado y tensors asignados");

  // ═══════════════════════════════════════════════════════════
  // 7. Obtener tensors y validar dimensiones
  // ═══════════════════════════════════════════════════════════
  input = interpreter->input(0);
  output = interpreter->output(0);

  Serial.println("\n📐 Información del modelo:");
  Serial.printf("   Input shape: [%d, %d, %d, %d]\n",
                input->dims->data[0], input->dims->data[1],
                input->dims->data[2], input->dims->data[3]);
  Serial.printf("   Input type: %s\n",
                input->type == kTfLiteInt8 ? "INT8" : "FLOAT32");
  Serial.printf("   Output shape: [%d, %d]\n",
                output->dims->data[0], output->dims->data[1]);
  Serial.printf("   Output type: %s\n",
                output->type == kTfLiteInt8 ? "INT8" : "FLOAT32");

  // Validar dimensiones esperadas
  bool dims_ok = true;
  if (input->dims->data[1] != EXPECTED_INPUT_HEIGHT ||
      input->dims->data[2] != EXPECTED_INPUT_WIDTH ||
      input->dims->data[3] != EXPECTED_INPUT_CHANNELS)
  {
    Serial.println("\n⚠️  ADVERTENCIA: Dimensiones de entrada no coinciden!");
    Serial.printf("   Esperado: [1, %d, %d, %d]\n",
                  EXPECTED_INPUT_HEIGHT, EXPECTED_INPUT_WIDTH, EXPECTED_INPUT_CHANNELS);
    Serial.printf("   Obtenido: [%d, %d, %d, %d]\n",
                  input->dims->data[0], input->dims->data[1],
                  input->dims->data[2], input->dims->data[3]);
    dims_ok = false;
  }

  if (output->dims->data[1] != EXPECTED_NUM_EMOTIONS)
  {
    Serial.println("\n⚠️  ADVERTENCIA: Número de emociones no coincide!");
    Serial.printf("   Esperado: %d emociones\n", EXPECTED_NUM_EMOTIONS);
    Serial.printf("   Obtenido: %d emociones\n", output->dims->data[1]);
    dims_ok = false;
  }

  if (dims_ok)
  {
    Serial.println("\n✅ Dimensiones del modelo correctas");
  }

  Serial.println("\n✅ FASE 1 COMPLETADA");
  Serial.println("════════════════════════════════════════════════════════");

  delay(2000);

  // ═══════════════════════════════════════════════════════════
  // FASE 2: INFERENCIA DUMMY
  // ═══════════════════════════════════════════════════════════

  Serial.println("\n🎲 FASE 2: Inferencia con datos aleatorios");
  Serial.println("════════════════════════════════════════════════════════");

  // 8. Llenar input con datos aleatorios
  Serial.println("\n🎲 Generando datos de entrada aleatorios...");

  int input_size = input->dims->data[1] * input->dims->data[2] * input->dims->data[3];
  Serial.printf("   Total elementos input: %d\n", input_size);
  Serial.printf("   Tamaño: %d MFCCs x %d frames = %d valores\n",
                input->dims->data[1], input->dims->data[2], input_size);

  // Modelo int8: usar valores en rango [-128, 127]
  for (int i = 0; i < input_size; i++)
  {
    input->data.int8[i] = random(-128, 128);
  }
  Serial.println("✅ Input tensor llenado con datos random");

  // 9. Ejecutar inferencia
  Serial.println("\n🧠 Ejecutando inferencia...");

  // Test de acceso a memoria
  Serial.println("\n🔍 DEBUG: Validando acceso a tensor_arena...");
  volatile uint8_t test_val = tensor_arena[0];
  tensor_arena[0] = 0xAA;
  if (tensor_arena[0] != 0xAA)
  {
    Serial.println("❌ ERROR: Memory write/read failed!");
    while (1)
      ;
  }
  tensor_arena[0] = test_val;
  Serial.println("✅ Tensor arena accesible");

  // Estado de memoria antes de Invoke()
  Serial.println("\n📊 Estado de memoria PRE-INVOKE:");
  Serial.printf("   Free heap: %d bytes\n", ESP.getFreeHeap());
  Serial.printf("   Free PSRAM: %d bytes\n", ESP.getFreePsram());
  Serial.printf("   Largest free block: %d bytes\n", heap_caps_get_largest_free_block(MALLOC_CAP_8BIT));
  Serial.flush();

  // Desactivar watchdog durante inferencia
  esp_task_wdt_init(60, false); // 60 segundos sin watchdog

  // CACHE MANAGEMENT para PSRAM
  if (actual_arena_size == kTensorArenaSize) // Solo si usamos PSRAM
  {
    Serial.println("\n🔄 Sincronizando caché con PSRAM...");
    Cache_WriteBack_All();
    Serial.println("   ✅ Cache writeback completado");
    delay(100);
  }

  Serial.println("\n⏱️  Iniciando Invoke()...");
  Serial.flush();

  unsigned long start = millis();
  TfLiteStatus invoke_status = interpreter->Invoke();
  unsigned long elapsed = millis() - start;

  Serial.println("✅ Invoke() COMPLETADO!");
  Serial.flush();

  esp_task_wdt_init(5, true); // 5 segundos normal

  // Estado de memoria después de Invoke()
  Serial.println("\n📊 Estado de memoria POST-INVOKE:");
  Serial.printf("   Free heap: %d bytes\n", ESP.getFreeHeap());
  Serial.printf("   Free PSRAM: %d bytes\n", ESP.getFreePsram());
  Serial.flush();

  if (invoke_status != kTfLiteOk)
  {
    Serial.println("❌ Error durante la inferencia");
    while (1)
      ;
  }

  Serial.printf("✅ Inferencia completada en %lu ms\n", elapsed);

  // 10. Leer resultados
  Serial.println("\n📊 Resultados (probabilidades):");
  Serial.println("════════════════════════════════════════════════════════");

  // Output es int8, necesitamos dequantizar
  float scale = output->params.scale;
  int zero_point = output->params.zero_point;

  Serial.printf("   Output quantization: scale=%.6f, zero_point=%d\n\n",
                scale, zero_point);

  int max_idx = 0;
  float max_prob = -1000.0f;

  int num_emotions = output->dims->data[1];
  for (int i = 0; i < num_emotions; i++)
  {
    // Dequantizar: float = (int8 - zero_point) * scale
    int8_t quant_value = output->data.int8[i];
    float prob = (quant_value - zero_point) * scale;

    // Usar etiqueta si existe, sino mostrar "emotion_N"
    const char *label = (i < EXPECTED_NUM_EMOTIONS) ? emotion_labels[i] : "unknown";

    Serial.printf("   %-10s: %6.2f%% (quant: %4d)\n",
                  label, prob * 100, quant_value);

    if (prob > max_prob)
    {
      max_prob = prob;
      max_idx = i;
    }
  }

  const char *predicted_label = (max_idx < EXPECTED_NUM_EMOTIONS) ? emotion_labels[max_idx] : "unknown";

  Serial.println("   ────────────────────────────────────────────────────");
  Serial.printf("   🎯 Emoción predicha: %s (%.2f%%)\n",
                predicted_label, max_prob * 100);
  Serial.println("   ────────────────────────────────────────────────────");

  Serial.println("\n💡 Nota: Los datos de entrada son aleatorios,");
  Serial.println("   así que el resultado no tiene sentido real.");
  Serial.println("   Para inferencia real, reemplaza el input con MFCCs");
  Serial.println("   de un audio procesado (40 MFCCs x 100 frames).");

  Serial.println("\n✅ FASE 2 COMPLETADA");
  Serial.println("════════════════════════════════════════════════════════");
  Serial.println("\n🎉 Test finalizado exitosamente!");
  Serial.printf("📦 Modelo probado: %s\n", getModelFileName(MODEL_PATH));
  Serial.println("════════════════════════════════════════════════════════");
}

void loop()
{
  delay(1000);
}
