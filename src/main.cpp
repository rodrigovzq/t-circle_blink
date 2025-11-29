#include <Arduino.h>
#include <LittleFS.h>
#include "tensorflow/lite/micro/micro_mutable_op_resolver.h"
#include "tensorflow/lite/micro/micro_interpreter.h"
#include "tensorflow/lite/micro/system_setup.h"
#include "tensorflow/lite/schema/schema_generated.h"
#include "tensorflow/lite/micro/micro_error_reporter.h"
#include "esp_task_wdt.h"

// === CACHE MANAGEMENT para PSRAM ===
#include "esp_heap_caps.h"

// Declaraciones de funciones ROM de caché (ESP32-S3)
extern "C" {
  void Cache_WriteBack_All(void);
}

#define VERSION "v2.2-cache-management"

// TFLite globals
namespace
{
  const tflite::Model *model = nullptr;
  tflite::MicroInterpreter *interpreter = nullptr;
  TfLiteTensor *input = nullptr;
  TfLiteTensor *output = nullptr;

  constexpr int kTensorArenaSize = 7 * 1024 * 1024; // 7 MB
  uint8_t *tensor_arena = nullptr;
  size_t actual_arena_size = 0; // Tamaño real asignado

  tflite::ErrorReporter *error_reporter = nullptr;
}

// Nombres de emociones
const char *emotion_labels[] = {
    "angry", "disgust", "fear", "happy", "neutral", "sad", "surprise"};

void setup()
{
  Serial.begin(115200);
  delay(2000);

  Serial.println("\n🧠 MoodLink - Test TFLite (Fase 2)");
  Serial.println("════════════════════════════════════");
  Serial.printf("📌 VERSION: %s\n", VERSION);
  Serial.println("════════════════════════════════════");

  // 1. Montar LittleFS
  Serial.println("\n📂 Montando LittleFS...");
  if (!LittleFS.begin(true))
  {
    Serial.println("❌ Error montando LittleFS");
    while (1)
      ;
  }
  Serial.println("✅ LittleFS OK");

  // 2. Cargar modelo
  Serial.println("\n📦 Cargando modelo...");
  File modelFile = LittleFS.open("/ser_cnn_int8.tflite", "r");
  if (!modelFile)
  {
    Serial.println("❌ No se pudo abrir modelo");
    while (1)
      ;
  }

  size_t modelSize = modelFile.size();
  Serial.printf("   Tamaño: %d bytes\n", modelSize);

  uint8_t *modelBuffer = (uint8_t *)ps_malloc(modelSize);
  if (!modelBuffer)
  {
    Serial.println("❌ No hay PSRAM para modelo");
    while (1)
      ;
  }

  modelFile.read(modelBuffer, modelSize);
  modelFile.close();
  Serial.println("✅ Modelo cargado en PSRAM");

  // 3. Inicializar TFLite
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

  // 4. Crear OpResolver
  static tflite::MicroMutableOpResolver<12> resolver;
  resolver.AddConv2D();
  resolver.AddMaxPool2D();
  resolver.AddFullyConnected();
  resolver.AddSoftmax();
  resolver.AddReshape();
  resolver.AddMean();
  resolver.AddQuantize();
  resolver.AddDequantize();
  resolver.AddMul();
  resolver.AddAdd();
  resolver.AddAveragePool2D();   // NUEVO
  resolver.AddPad();             // NUEVO
  resolver.AddRelu();            // NUEVO
  resolver.AddLogistic();        // NUEVO
  resolver.AddRelu6();           // NUEVO
  resolver.AddDepthwiseConv2D(); // NUEVO - por si acaso
  Serial.println("✅ OpResolver configurado");

  // 5. Reservar tensor arena
  // === CAMBIO CRÍTICO: Intentar RAM interna primero ===
  Serial.println("\n💾 Intentando asignar tensor arena en RAM interna...");

  // Intentar con arena reducida en RAM interna (sin cache coherency issues)
  constexpr int kReducedArenaSize = 512 * 1024; // 512 KB
  tensor_arena = (uint8_t *)heap_caps_aligned_alloc(16, kReducedArenaSize,
                                                     MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT);

  if (tensor_arena) {
    actual_arena_size = kReducedArenaSize;
    Serial.printf("✅ Tensor arena en RAM INTERNA: %d KB\n", actual_arena_size / 1024);
    Serial.println("   ⚠️  NOTA: Arena reducida, puede fallar AllocateTensors()");
  } else {
    // Fallback a PSRAM con DMA capability (bypass cache)
    Serial.println("⚠️  RAM interna insuficiente, usando PSRAM...");
    Serial.println("   🔧 Intentando PSRAM con acceso DMA (sin caché)...");

    // Intento 1: PSRAM con DMA (puede bypasear caché)
    tensor_arena = (uint8_t *)heap_caps_aligned_alloc(16, kTensorArenaSize,
                                                       MALLOC_CAP_SPIRAM | MALLOC_CAP_DMA);

    if (!tensor_arena) {
      // Intento 2: PSRAM normal
      Serial.println("   ⚠️  DMA fallback, usando PSRAM estándar...");
      tensor_arena = (uint8_t *)heap_caps_aligned_alloc(16, kTensorArenaSize, MALLOC_CAP_SPIRAM);
    }

    if (!tensor_arena) {
      Serial.println("❌ No hay memoria disponible para tensor arena");
      while (1);
    }
    actual_arena_size = kTensorArenaSize;
    Serial.printf("✅ Tensor arena en PSRAM: %d KB\n", actual_arena_size / 1024);
    Serial.println("   ⚠️  ADVERTENCIA: PSRAM con cache management experimental");
  }

  // 6. Crear error reporter
  error_reporter = tflite::GetMicroErrorReporter();

  // 7. Crear intérprete
  static tflite::MicroInterpreter static_interpreter(
      model, resolver, tensor_arena, actual_arena_size, error_reporter);
  interpreter = &static_interpreter;

  TfLiteStatus allocate_status = interpreter->AllocateTensors();
  if (allocate_status != kTfLiteOk)
  {
    Serial.println("❌ Error al asignar tensors");
    while (1)
      ;
  }
  Serial.println("✅ Intérprete creado");

  // 8. Obtener tensors
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

  Serial.println("\n✅ FASE 1 COMPLETADA");
  Serial.println("════════════════════════════════════");

  delay(2000);

  // ════════════════════════════════════
  // FASE 2: INFERENCIA DUMMY
  // ════════════════════════════════════

  Serial.println("\n🎲 FASE 2: Inferencia con datos aleatorios");
  Serial.println("════════════════════════════════════");

  // 9. Llenar input con datos aleatorios
  Serial.println("\n🎲 Generando datos de entrada aleatorios...");

  int input_size = input->dims->data[1] * input->dims->data[2] * input->dims->data[3];
  Serial.printf("   Total elementos input: %d\n", input_size);

  // Modelo int8: usar valores en rango [-128, 127]
  for (int i = 0; i < input_size; i++)
  {
    input->data.int8[i] = random(-128, 128);
  }
  Serial.println("✅ Input tensor llenado con datos random");

  // 10. Ejecutar inferencia
  Serial.println("\n🧠 Ejecutando inferencia...");

  // === DEBUG: Test de acceso a memoria ===
  Serial.println("\n🔍 DEBUG: Validando acceso a tensor_arena...");
  volatile uint8_t test_val = tensor_arena[0];
  tensor_arena[0] = 0xAA;
  if (tensor_arena[0] != 0xAA) {
    Serial.println("❌ ERROR: PSRAM write/read failed!");
    while(1);
  }
  tensor_arena[0] = test_val;
  Serial.println("✅ Tensor arena accesible");

  // === DEBUG: Estado de memoria antes de Invoke() ===
  Serial.println("\n📊 Estado de memoria PRE-INVOKE:");
  Serial.printf("   Free heap: %d bytes\n", ESP.getFreeHeap());
  Serial.printf("   Free PSRAM: %d bytes\n", ESP.getFreePsram());
  Serial.printf("   Largest free block: %d bytes\n", heap_caps_get_largest_free_block(MALLOC_CAP_8BIT));
  Serial.flush();

  // Desactivar watchdog durante inferencia
  esp_task_wdt_init(60, false); // 60 segundos sin watchdog

  // === CACHE MANAGEMENT: CRITICAL FIX para PSRAM ===
  Serial.println("\n🔄 Sincronizando caché con PSRAM...");

  // ESP32-S3: Forzar writeback de todas las líneas de caché a PSRAM
  Cache_WriteBack_All();
  Serial.println("   ✅ Cache writeback completado");
  Serial.println("   ⚠️  NOTA: Cache writeback solo ayuda parcialmente");
  Serial.println("   ⚠️  Si se cuelga, el problema es acceso durante ejecución de Conv2D");

  Serial.flush();
  delay(100); // Dar tiempo para que se complete el sync

  Serial.println("\n⏱️  Iniciando Invoke() - Si se cuelga aquí, revisa serial...");
  Serial.flush();

  unsigned long start = millis();
  TfLiteStatus invoke_status = interpreter->Invoke();
  unsigned long elapsed = millis() - start;

  Serial.println("✅ Invoke() COMPLETADO!");
  Serial.flush();

  esp_task_wdt_init(5, true); // 5 segundos normal

  // === DEBUG: Estado de memoria después de Invoke() ===
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

  // 11. Leer resultados
  Serial.println("\n📊 Resultados (probabilidades):");
  Serial.println("════════════════════════════════════");

  // Output es int8, necesitamos dequantizar
  float scale = output->params.scale;
  int zero_point = output->params.zero_point;

  Serial.printf("   Output quantization: scale=%.6f, zero_point=%d\n\n",
                scale, zero_point);

  int max_idx = 0;
  float max_prob = -1000.0f;

  for (int i = 0; i < 7; i++)
  {
    // Dequantizar: float = (int8 - zero_point) * scale
    int8_t quant_value = output->data.int8[i];
    float prob = (quant_value - zero_point) * scale;

    Serial.printf("   %-10s: %6.2f%% (quant: %4d)\n",
                  emotion_labels[i], prob * 100, quant_value);

    if (prob > max_prob)
    {
      max_prob = prob;
      max_idx = i;
    }
  }

  Serial.println("   ────────────────────────────────");
  Serial.printf("   🎯 Emoción predicha: %s (%.2f%%)\n",
                emotion_labels[max_idx], max_prob * 100);
  Serial.println("   ────────────────────────────────");

  Serial.println("\n💡 Nota: Los datos de entrada son aleatorios,");
  Serial.println("   así que el resultado no tiene sentido real.");
  Serial.println("   El objetivo es verificar que la inferencia funciona.\n");

  Serial.println("✅ FASE 2 COMPLETADA");
  Serial.println("════════════════════════════════════");
}

void loop()
{
  delay(1000);
}
