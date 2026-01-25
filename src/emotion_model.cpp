#include "emotion_model.h"
#include "config.h"
#include <Arduino.h>
#include <LittleFS.h>
#include "esp_heap_caps.h"
#include "esp_task_wdt.h"
#include "tensorflow/lite/micro/micro_mutable_op_resolver.h"
#include "tensorflow/lite/micro/micro_interpreter.h"
#include "tensorflow/lite/micro/system_setup.h"
#include "tensorflow/lite/schema/schema_generated.h"
#include "tensorflow/lite/micro/micro_error_reporter.h"

// Declaración de función ROM de cache (ESP32-S3)
extern "C" {
    void Cache_WriteBack_All(void);
}

// =============================================================================
// Implementación - Modelo TFLite
// =============================================================================

// Tamaño del tensor arena (con margen de seguridad)
static constexpr size_t TENSOR_ARENA_SIZE = 156 * 1024;

// Buffers internos
static uint8_t* modelBuffer = nullptr;
static uint8_t* tensorArena = nullptr;
static size_t modelSize = 0;

// TFLite
static const tflite::Model* model = nullptr;
static tflite::MicroInterpreter* interpreter = nullptr;
static TfLiteTensor* inputTensor = nullptr;
static TfLiteTensor* outputTensor = nullptr;

// Resolver con las operaciones necesarias
static tflite::MicroMutableOpResolver<11> resolver;
static tflite::MicroErrorReporter errorReporter;

// -----------------------------------------------------------------------------
// API pública
// -----------------------------------------------------------------------------

bool model_load(const char* path) {
    Serial.printf("[Model] Cargando %s...\n", path);

    // Montar LittleFS
    if (!LittleFS.begin(true)) {
        Serial.println("[Model] ERROR: No se pudo montar LittleFS");
        return false;
    }

    // Abrir archivo
    File file = LittleFS.open(path, "r");
    if (!file) {
        Serial.printf("[Model] ERROR: No se pudo abrir %s\n", path);
        return false;
    }

    modelSize = file.size();
    Serial.printf("[Model] Tamaño: %.1f KB\n", modelSize / 1024.0f);

    // Alocar buffer para el modelo en PSRAM
    modelBuffer = (uint8_t*)heap_caps_aligned_alloc(16, modelSize, MALLOC_CAP_SPIRAM);
    if (!modelBuffer) {
        Serial.println("[Model] ERROR: No se pudo alocar modelBuffer");
        file.close();
        return false;
    }

    // Leer modelo
    file.read(modelBuffer, modelSize);
    file.close();

    // Alocar tensor arena en PSRAM
    tensorArena = (uint8_t*)heap_caps_aligned_alloc(16, TENSOR_ARENA_SIZE, MALLOC_CAP_SPIRAM);
    if (!tensorArena) {
        Serial.println("[Model] ERROR: No se pudo alocar tensorArena");
        return false;
    }

    // Verificar modelo
    model = tflite::GetModel(modelBuffer);
    if (model->version() != TFLITE_SCHEMA_VERSION) {
        Serial.println("[Model] ERROR: Versión de modelo incompatible");
        return false;
    }

    // Agregar operaciones al resolver
    resolver.AddConv2D();
    resolver.AddMaxPool2D();
    resolver.AddAveragePool2D();
    resolver.AddMean();
    resolver.AddFullyConnected();
    resolver.AddSoftmax();
    resolver.AddReshape();
    resolver.AddQuantize();
    resolver.AddDequantize();
    resolver.AddMul();
    resolver.AddAdd();

    // Crear intérprete
    static tflite::MicroInterpreter staticInterpreter(
        model, resolver, tensorArena, TENSOR_ARENA_SIZE, &errorReporter
    );
    interpreter = &staticInterpreter;

    // Alocar tensores
    if (interpreter->AllocateTensors() != kTfLiteOk) {
        Serial.println("[Model] ERROR: No se pudo alocar tensores");
        return false;
    }

    inputTensor = interpreter->input(0);
    outputTensor = interpreter->output(0);

    Serial.printf("[Model] Input: [%d, %d, %d, %d] %s\n",
                  inputTensor->dims->data[0], inputTensor->dims->data[1],
                  inputTensor->dims->data[2], inputTensor->dims->data[3],
                  inputTensor->type == kTfLiteInt8 ? "INT8" : "FLOAT");
    Serial.printf("[Model] Output: [%d, %d]\n",
                  outputTensor->dims->data[0], outputTensor->dims->data[1]);
    Serial.println("[Model] Cargado OK");

    return true;
}

EmotionResult model_predict(const float* mfcc_in) {
    EmotionResult result = {nullptr, 0.0f, 0, {0}};

    if (!interpreter || !inputTensor || !outputTensor) {
        Serial.println("[Model] ERROR: Modelo no cargado");
        result.label = "error";
        return result;
    }

    // Quantizar MFCCs a INT8 y llenar input tensor
    float inputScale = inputTensor->params.scale;
    int inputZeroPoint = inputTensor->params.zero_point;

    for (int mfcc_idx = 0; mfcc_idx < N_MFCC; mfcc_idx++) {
        for (int frame_idx = 0; frame_idx < N_FRAMES; frame_idx++) {
            float value = mfcc_in[mfcc_idx * N_FRAMES + frame_idx];
            int32_t quantized = (int32_t)round(value / inputScale) + inputZeroPoint;
            quantized = constrain(quantized, -128, 127);
            inputTensor->data.int8[mfcc_idx * N_FRAMES + frame_idx] = (int8_t)quantized;
        }
    }

    // Preparar para inferencia
    esp_task_wdt_init(120, false);
    Cache_WriteBack_All();
    delay(50);

    // Ejecutar inferencia
    Serial.println("[Model] Ejecutando inferencia...");
    unsigned long startTime = millis();

    TfLiteStatus status = interpreter->Invoke();

    unsigned long elapsed = millis() - startTime;
    esp_task_wdt_init(5, true);

    if (status != kTfLiteOk) {
        Serial.println("[Model] ERROR: Invoke() falló");
        result.label = "error";
        return result;
    }

    Serial.printf("[Model] Inferencia: %lu ms\n", elapsed);

    // Procesar salida
    float outputScale = outputTensor->params.scale;
    int outputZeroPoint = outputTensor->params.zero_point;

    float maxProb = -1000.0f;
    int maxIdx = 0;

    for (int i = 0; i < NUM_EMOTIONS; i++) {
        int8_t quantValue = outputTensor->data.int8[i];
        float prob = (quantValue - outputZeroPoint) * outputScale;
        result.probabilities[i] = prob;

        if (prob > maxProb) {
            maxProb = prob;
            maxIdx = i;
        }
    }

    result.label = EMOTION_LABELS[maxIdx];
    result.confidence = maxProb;
    result.index = maxIdx;

    return result;
}

void model_unload() {
    if (modelBuffer) {
        heap_caps_free(modelBuffer);
        modelBuffer = nullptr;
    }
    if (tensorArena) {
        heap_caps_free(tensorArena);
        tensorArena = nullptr;
    }
    interpreter = nullptr;
    inputTensor = nullptr;
    outputTensor = nullptr;
}
