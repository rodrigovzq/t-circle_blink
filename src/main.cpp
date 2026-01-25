#include <Arduino.h>
#include "esp_heap_caps.h"
#include "config.h"
#include "audio_capture.h"
#include "mfcc_extractor.h"
#include "emotion_model.h"

// =============================================================================
// MoodLink - Test 5.1: Pipeline Modular
// =============================================================================
// Pipeline de reconocimiento de emociones por voz:
//   Audio (I2S) -> Normalización -> MFCC -> TFLite -> Emoción
// =============================================================================

// Buffers del pipeline (alocados en PSRAM)
static int16_t* audio_buffer = nullptr;
static float* mfcc_buffer = nullptr;

// -----------------------------------------------------------------------------
// Helpers
// -----------------------------------------------------------------------------

static void print_separator() {
    Serial.println("================================================================");
}

static void print_memory_status(const char* label) {
    size_t psram_free = heap_caps_get_free_size(MALLOC_CAP_SPIRAM);
    size_t psram_total = heap_caps_get_total_size(MALLOC_CAP_SPIRAM);
    Serial.printf("[%s] PSRAM: %d KB libres / %d KB total\n",
                  label, psram_free / 1024, psram_total / 1024);
}

static void print_result(const EmotionResult& result) {
    print_separator();
    Serial.println("RESULTADO");
    print_separator();

    Serial.println("\n  Emocion      | Probabilidad");
    Serial.println("  -------------|-------------");

    for (int i = 0; i < NUM_EMOTIONS; i++) {
        const char* marker = (i == result.index) ? " <<" : "";
        Serial.printf("  %-12s | %6.2f%%%s\n",
                      EMOTION_LABELS[i],
                      result.probabilities[i] * 100,
                      marker);
    }

    Serial.printf("\n  >> %s (%.1f%%)\n", result.label, result.confidence * 100);
}

// -----------------------------------------------------------------------------
// Setup
// -----------------------------------------------------------------------------

void setup() {
    Serial.begin(115200);
    delay(2000);

    print_separator();
    Serial.println("MoodLink - Test 5.1: Pipeline Modular");
    Serial.printf("Version: %s\n", VERSION);
    print_separator();

    print_memory_status("INICIO");

    // -------------------------------------------------------------------------
    // 1. Alocar buffers del pipeline
    // -------------------------------------------------------------------------
    Serial.println("\n[1/4] Alocando buffers...");

    audio_buffer = (int16_t*)heap_caps_aligned_alloc(
        16, AUDIO_SAMPLES * sizeof(int16_t), MALLOC_CAP_SPIRAM);

    mfcc_buffer = (float*)heap_caps_aligned_alloc(
        16, N_MFCC * N_FRAMES * sizeof(float), MALLOC_CAP_SPIRAM);

    if (!audio_buffer || !mfcc_buffer) {
        Serial.println("ERROR: No se pudieron alocar buffers");
        while (1) delay(1000);
    }

    Serial.printf("  audio_buffer: %d KB\n", (AUDIO_SAMPLES * sizeof(int16_t)) / 1024);
    Serial.printf("  mfcc_buffer:  %d KB\n", (N_MFCC * N_FRAMES * sizeof(float)) / 1024);

    // -------------------------------------------------------------------------
    // 2. Inicializar módulos
    // -------------------------------------------------------------------------
    Serial.println("\n[2/4] Inicializando audio...");
    if (!audio_init()) {
        Serial.println("ERROR: Fallo audio_init()");
        while (1) delay(1000);
    }

    Serial.println("\n[3/4] Inicializando MFCC...");
    if (!mfcc_init()) {
        Serial.println("ERROR: Fallo mfcc_init()");
        while (1) delay(1000);
    }

    Serial.println("\n[4/4] Cargando modelo...");
    if (!model_load(MODEL_PATH)) {
        Serial.println("ERROR: Fallo model_load()");
        while (1) delay(1000);
    }

    print_memory_status("POST-INIT");

    Serial.println("\n========== SISTEMA LISTO ==========\n");
}

// -----------------------------------------------------------------------------
// Loop - Pipeline principal
// -----------------------------------------------------------------------------

void loop() {
    Serial.println("\n*** Preparate para hablar en 3 segundos ***\n");
    delay(3000);

    unsigned long pipeline_start = millis();

    // -------------------------------------------------------------------------
    // Etapa 1: Capturar audio
    // -------------------------------------------------------------------------
    unsigned long t1 = millis();

    if (!audio_capture(audio_buffer)) {
        Serial.println("ERROR: Fallo captura de audio");
        delay(5000);
        return;
    }

    // Estadísticas pre-normalización
    AudioStats stats_before = audio_get_stats(audio_buffer);
    Serial.printf("[Audio] Pre-norm  - RMS: %.1f, Picos: [%d, %d]\n",
                  stats_before.rms, stats_before.peak_neg, stats_before.peak_pos);

    // Normalizar
    audio_normalize(audio_buffer);

    // Estadísticas post-normalización
    AudioStats stats_after = audio_get_stats(audio_buffer);
    Serial.printf("[Audio] Post-norm - RMS: %.1f, Picos: [%d, %d]\n",
                  stats_after.rms, stats_after.peak_neg, stats_after.peak_pos);

    unsigned long t1_end = millis();

    // -------------------------------------------------------------------------
    // Etapa 2: Extraer MFCCs
    // -------------------------------------------------------------------------
    unsigned long t2 = millis();

    mfcc_extract(audio_buffer, mfcc_buffer);

    unsigned long t2_end = millis();

    // -------------------------------------------------------------------------
    // Etapa 3: Inferencia
    // -------------------------------------------------------------------------
    unsigned long t3 = millis();

    EmotionResult result = model_predict(mfcc_buffer);

    unsigned long t3_end = millis();

    // -------------------------------------------------------------------------
    // Mostrar resultado
    // -------------------------------------------------------------------------
    unsigned long pipeline_end = millis();

    print_result(result);

    // Resumen de tiempos
    Serial.println("\nTIEMPOS:");
    Serial.printf("  Captura + Norm: %4lu ms\n", t1_end - t1);
    Serial.printf("  MFCC:           %4lu ms\n", t2_end - t2);
    Serial.printf("  Inferencia:     %4lu ms\n", t3_end - t3);
    Serial.printf("  ----------------------\n");
    Serial.printf("  TOTAL:          %4lu ms\n", pipeline_end - pipeline_start);

    print_separator();

    // Esperar antes del próximo ciclo
    delay(2000);
}
