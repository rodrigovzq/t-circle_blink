// MoodLink v3.0 - ESP-IDF + esp-tflite-micro migration
// Optimized with ESP-NN hardware acceleration

#include <stdio.h>
#include <stdlib.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "esp_heap_caps.h"
#include "esp_spiffs.h"
#include "esp_system.h"

// TensorFlow Lite Micro headers
#include "tensorflow/lite/micro/micro_mutable_op_resolver.h"
#include "tensorflow/lite/micro/micro_interpreter.h"
#include "tensorflow/lite/micro/system_setup.h"
#include "tensorflow/lite/schema/schema_generated.h"
#include "tensorflow/lite/micro/micro_log.h"

#define VERSION "v3.0-espidf-migration"

static const char *TAG = "MoodLink";

// TFLite globals
namespace
{
  const tflite::Model *model = nullptr;
  tflite::MicroInterpreter *interpreter = nullptr;
  TfLiteTensor *input = nullptr;
  TfLiteTensor *output = nullptr;

  constexpr int kTensorArenaSize = 7 * 1024 * 1024; // 7 MB
  uint8_t *tensor_arena = nullptr;
  size_t actual_arena_size = 0;

#ifdef CONFIG_NN_OPTIMIZED
  constexpr int kScratchBufferSize = 60 * 1024; // 60 KB for ESP-NN optimizations
  uint8_t *scratch_buffer = nullptr;
#endif
}

// Nombres de emociones
const char *emotion_labels[] = {
    "angry", "disgust", "fear", "happy", "neutral", "sad", "surprise"};

// Initialize SPIFFS for model storage
static esp_err_t init_spiffs(void)
{
  ESP_LOGI(TAG, "Initializing SPIFFS");

  esp_vfs_spiffs_conf_t conf = {
    .base_path = "/spiffs",
    .partition_label = NULL,
    .max_files = 5,
    .format_if_mount_failed = false
  };

  esp_err_t ret = esp_vfs_spiffs_register(&conf);
  if (ret != ESP_OK) {
    ESP_LOGE(TAG, "Failed to initialize SPIFFS (%s)", esp_err_to_name(ret));
    return ret;
  }

  size_t total = 0, used = 0;
  ret = esp_spiffs_info(NULL, &total, &used);
  if (ret == ESP_OK) {
    ESP_LOGI(TAG, "SPIFFS: %d KB total, %d KB used", total / 1024, used / 1024);
  }

  return ESP_OK;
}

// Initialize TensorFlow Lite model
static void init_tflite(void)
{
  ESP_LOGI(TAG, "\n🧠 MoodLink - TFLite Inference Test");
  ESP_LOGI(TAG, "════════════════════════════════════");
  ESP_LOGI(TAG, "📌 VERSION: %s", VERSION);
  ESP_LOGI(TAG, "════════════════════════════════════");

  // 1. Mount SPIFFS
  ESP_LOGI(TAG, "\n📂 Mounting SPIFFS...");
  if (init_spiffs() != ESP_OK) {
    ESP_LOGE(TAG, "❌ Error mounting SPIFFS");
    abort();
  }
  ESP_LOGI(TAG, "✅ SPIFFS OK");

  // 2. Load model from SPIFFS
  ESP_LOGI(TAG, "\n📦 Loading model...");
  FILE *modelFile = fopen("/spiffs/ser_cnn_int8.tflite", "rb");
  if (!modelFile) {
    ESP_LOGE(TAG, "❌ Cannot open model file");
    abort();
  }

  // Get file size
  fseek(modelFile, 0, SEEK_END);
  size_t modelSize = ftell(modelFile);
  fseek(modelFile, 0, SEEK_SET);
  ESP_LOGI(TAG, "   Size: %d bytes", modelSize);

  // Allocate buffer in PSRAM
  uint8_t *modelBuffer = (uint8_t *)heap_caps_malloc(modelSize, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
  if (!modelBuffer) {
    ESP_LOGE(TAG, "❌ No PSRAM available for model");
    fclose(modelFile);
    abort();
  }

  // Read model into buffer
  size_t bytes_read = fread(modelBuffer, 1, modelSize, modelFile);
  fclose(modelFile);

  if (bytes_read != modelSize) {
    ESP_LOGE(TAG, "❌ Failed to read model (%d/%d bytes)", bytes_read, modelSize);
    abort();
  }

  ESP_LOGI(TAG, "✅ Model loaded into PSRAM");

  // 3. Initialize TFLite
  ESP_LOGI(TAG, "\n🔧 Initializing TFLite...");

  model = tflite::GetModel(modelBuffer);
  if (model->version() != TFLITE_SCHEMA_VERSION) {
    ESP_LOGE(TAG, "❌ Incorrect version: model=%d, expected=%d",
             model->version(), TFLITE_SCHEMA_VERSION);
    abort();
  }
  ESP_LOGI(TAG, "✅ Model compatible (schema v%d)", TFLITE_SCHEMA_VERSION);

  // 4. Create OpResolver with all required operations
  static tflite::MicroMutableOpResolver<15> resolver;
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
  resolver.AddAveragePool2D();
  resolver.AddPad();
  resolver.AddRelu();
  resolver.AddLogistic();
  resolver.AddRelu6();
  resolver.AddDepthwiseConv2D();
  ESP_LOGI(TAG, "✅ OpResolver configured (15 operations)");

  // 5. Allocate tensor arena in PSRAM
  ESP_LOGI(TAG, "\n💾 Allocating tensor arena...");

  tensor_arena = (uint8_t *)heap_caps_malloc(kTensorArenaSize, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);

  if (!tensor_arena) {
    ESP_LOGE(TAG, "❌ Failed to allocate tensor arena (%d KB)", kTensorArenaSize / 1024);
    abort();
  }

  actual_arena_size = kTensorArenaSize;
  ESP_LOGI(TAG, "✅ Tensor arena allocated: %d KB in PSRAM", actual_arena_size / 1024);

#ifdef CONFIG_NN_OPTIMIZED
  // Allocate scratch buffer for ESP-NN optimizations
  ESP_LOGI(TAG, "💾 Allocating scratch buffer for ESP-NN...");
  scratch_buffer = (uint8_t *)heap_caps_malloc(kScratchBufferSize, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);

  if (!scratch_buffer) {
    ESP_LOGW(TAG, "⚠️  Failed to allocate scratch buffer, continuing without it");
  } else {
    ESP_LOGI(TAG, "✅ Scratch buffer allocated: %d KB", kScratchBufferSize / 1024);
  }
#endif

  // 6. Create interpreter
  ESP_LOGI(TAG, "🔧 Creating interpreter...");
  static tflite::MicroInterpreter static_interpreter(
      model, resolver, tensor_arena, actual_arena_size);
  interpreter = &static_interpreter;

  TfLiteStatus allocate_status = interpreter->AllocateTensors();
  if (allocate_status != kTfLiteOk) {
    ESP_LOGE(TAG, "❌ Failed to allocate tensors");
    abort();
  }
  ESP_LOGI(TAG, "✅ Interpreter created successfully");

  // 7. Get input/output tensors
  input = interpreter->input(0);
  output = interpreter->output(0);

  ESP_LOGI(TAG, "\n📐 Model information:");
  ESP_LOGI(TAG, "   Input shape: [%d, %d, %d, %d]",
           input->dims->data[0], input->dims->data[1],
           input->dims->data[2], input->dims->data[3]);
  ESP_LOGI(TAG, "   Input type: %s",
           input->type == kTfLiteInt8 ? "INT8" : "FLOAT32");
  ESP_LOGI(TAG, "   Output shape: [%d, %d]",
           output->dims->data[0], output->dims->data[1]);
  ESP_LOGI(TAG, "   Output type: %s",
           output->type == kTfLiteInt8 ? "INT8" : "FLOAT32");

  ESP_LOGI(TAG, "\n✅ PHASE 1 COMPLETED");
  ESP_LOGI(TAG, "════════════════════════════════════");

  vTaskDelay(pdMS_TO_TICKS(1000));

  // ════════════════════════════════════
  // PHASE 2: DUMMY INFERENCE
  // ════════════════════════════════════

  ESP_LOGI(TAG, "\n🎲 PHASE 2: Inference with random data");
  ESP_LOGI(TAG, "════════════════════════════════════");

  // 8. Fill input with random data
  ESP_LOGI(TAG, "\n🎲 Generating random input data...");

  int input_size = input->dims->data[1] * input->dims->data[2] * input->dims->data[3];
  ESP_LOGI(TAG, "   Total input elements: %d", input_size);

  // Model int8: use values in range [-128, 127]
  for (int i = 0; i < input_size; i++) {
    input->data.int8[i] = (esp_random() % 256) - 128;
  }
  ESP_LOGI(TAG, "✅ Input tensor filled with random data");

  // 9. Run inference
  ESP_LOGI(TAG, "\n🧠 Running inference...");

  // Memory status before inference
  ESP_LOGI(TAG, "\n📊 Memory status PRE-INVOKE:");
  ESP_LOGI(TAG, "   Free heap: %d bytes", heap_caps_get_free_size(MALLOC_CAP_INTERNAL));
  ESP_LOGI(TAG, "   Free PSRAM: %d bytes", heap_caps_get_free_size(MALLOC_CAP_SPIRAM));
  ESP_LOGI(TAG, "   Largest free block: %d bytes", heap_caps_get_largest_free_block(MALLOC_CAP_8BIT));

  ESP_LOGI(TAG, "\n⏱️  Starting Invoke() with ESP-NN optimizations...");

  // High-precision timing
  int64_t start_time = esp_timer_get_time();
  TfLiteStatus invoke_status = interpreter->Invoke();
  int64_t end_time = esp_timer_get_time();
  int64_t elapsed_us = end_time - start_time;

  if (invoke_status != kTfLiteOk) {
    ESP_LOGE(TAG, "❌ Error during inference");
    abort();
  }

  ESP_LOGI(TAG, "✅ Invoke() COMPLETED!");

  // Memory status after inference
  ESP_LOGI(TAG, "\n📊 Memory status POST-INVOKE:");
  ESP_LOGI(TAG, "   Free heap: %d bytes", heap_caps_get_free_size(MALLOC_CAP_INTERNAL));
  ESP_LOGI(TAG, "   Free PSRAM: %d bytes", heap_caps_get_free_size(MALLOC_CAP_SPIRAM));

  // Convert to human-readable format
  float elapsed_ms = elapsed_us / 1000.0f;
  float elapsed_sec = elapsed_ms / 1000.0f;

  ESP_LOGI(TAG, "\n✅ Inference completed:");
  ESP_LOGI(TAG, "   Time: %.2f ms (%.2f seconds)", elapsed_ms, elapsed_sec);

  if (elapsed_sec >= 60) {
    ESP_LOGI(TAG, "   (%.2f minutes)", elapsed_sec / 60.0f);
  }

  // 10. Read and display results
  ESP_LOGI(TAG, "\n📊 Results (probabilities):");
  ESP_LOGI(TAG, "════════════════════════════════════");

  // Output is int8, need to dequantize
  float scale = output->params.scale;
  int zero_point = output->params.zero_point;

  ESP_LOGI(TAG, "   Output quantization: scale=%.6f, zero_point=%d\n", scale, zero_point);

  int max_idx = 0;
  float max_prob = -1000.0f;

  for (int i = 0; i < 7; i++) {
    // Dequantize: float = (int8 - zero_point) * scale
    int8_t quant_value = output->data.int8[i];
    float prob = (quant_value - zero_point) * scale;

    ESP_LOGI(TAG, "   %-10s: %6.2f%% (quant: %4d)",
             emotion_labels[i], prob * 100, quant_value);

    if (prob > max_prob) {
      max_prob = prob;
      max_idx = i;
    }
  }

  ESP_LOGI(TAG, "   ────────────────────────────────");
  ESP_LOGI(TAG, "   🎯 Predicted emotion: %s (%.2f%%)",
           emotion_labels[max_idx], max_prob * 100);
  ESP_LOGI(TAG, "   ────────────────────────────────");

  ESP_LOGI(TAG, "\n💡 Note: Input data is random,");
  ESP_LOGI(TAG, "   so the result has no real meaning.");
  ESP_LOGI(TAG, "   The goal is to verify that inference works.\n");

  ESP_LOGI(TAG, "✅ PHASE 2 COMPLETED");
  ESP_LOGI(TAG, "════════════════════════════════════");

  // Performance comparison
  float speedup = 339000.0f / elapsed_ms; // baseline was 339 seconds
  ESP_LOGI(TAG, "\n🚀 PERFORMANCE IMPROVEMENT:");
  ESP_LOGI(TAG, "   Baseline (v2.2): 339.0 seconds");
  ESP_LOGI(TAG, "   Current (v3.0):  %.2f seconds", elapsed_sec);
  ESP_LOGI(TAG, "   Speedup: %.1fx faster!", speedup);
}

// ESP-IDF entry point
extern "C" void app_main(void)
{
  // Initialize and run TFLite inference
  init_tflite();

  // Keep running
  while (1) {
    vTaskDelay(pdMS_TO_TICKS(1000));
  }
}
