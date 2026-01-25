#include <Arduino.h>
#include "esp_heap_caps.h"
#include "config.h"
#include "audio_capture.h"
#include "mfcc_extractor.h"
#include "emotion_model.h"
#include "profiler.h"

// =============================================================================
// MoodLink - Test 5.3: Pipeline con Profiling y CSV
// =============================================================================
// Pipeline de reconocimiento de emociones con métricas de memoria y tiempo.
// Los datos se guardan en /profiling.csv para exportar y analizar.
// =============================================================================

#define CSV_FILENAME "/profiling.csv"

// Buffers del pipeline (alocados en PSRAM)
static int16_t* audio_buffer = nullptr;
static float* mfcc_buffer = nullptr;

// Contador de iteraciones
static uint32_t iteration_count = 0;

// Perfil de memoria de inicialización
static InitMemoryProfile init_memory;

// -----------------------------------------------------------------------------
// Prototipos
// -----------------------------------------------------------------------------
static void print_help();

// -----------------------------------------------------------------------------
// Helpers
// -----------------------------------------------------------------------------

static void print_separator() {
    Serial.println("================================================================");
}

static uint32_t get_psram_free_kb() {
    return heap_caps_get_free_size(MALLOC_CAP_SPIRAM) / 1024;
}

static uint32_t get_psram_total_kb() {
    return heap_caps_get_total_size(MALLOC_CAP_SPIRAM) / 1024;
}

static uint32_t get_dram_free_kb() {
    return heap_caps_get_free_size(MALLOC_CAP_INTERNAL) / 1024;
}

static void print_result(const EmotionResult& result) {
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
    Serial.println("MoodLink - Test 5.3: Pipeline con Profiling");
    Serial.printf("Version: %s\n", VERSION);
    Serial.printf("CSV Output: %s\n", CSV_FILENAME);
    print_separator();

    // Guardar PSRAM inicial
    init_memory.psram_initial_kb = get_psram_free_kb();
    Serial.printf("\n[MEMORIA] PSRAM inicial: %u KB\n", init_memory.psram_initial_kb);

    // -------------------------------------------------------------------------
    // 1. Alocar buffers del pipeline
    // -------------------------------------------------------------------------
    Serial.println("\n[1/5] Alocando buffers...");

    size_t audio_size = AUDIO_SAMPLES * sizeof(int16_t);
    size_t mfcc_size = N_MFCC * N_FRAMES * sizeof(float);

    audio_buffer = (int16_t*)heap_caps_aligned_alloc(
        16, audio_size, MALLOC_CAP_SPIRAM);

    mfcc_buffer = (float*)heap_caps_aligned_alloc(
        16, mfcc_size, MALLOC_CAP_SPIRAM);

    if (!audio_buffer || !mfcc_buffer) {
        Serial.println("ERROR: No se pudieron alocar buffers");
        while (1) delay(1000);
    }

    init_memory.audio_buffer_kb = audio_size / 1024.0f;
    init_memory.mfcc_buffer_kb = mfcc_size / 1024.0f;

    Serial.printf("  audio_buffer: %.1f KB\n", init_memory.audio_buffer_kb);
    Serial.printf("  mfcc_buffer:  %.1f KB\n", init_memory.mfcc_buffer_kb);

    // -------------------------------------------------------------------------
    // 2. Inicializar módulos
    // -------------------------------------------------------------------------
    Serial.println("\n[2/5] Inicializando audio...");
    if (!audio_init()) {
        Serial.println("ERROR: Fallo audio_init()");
        while (1) delay(1000);
    }

    Serial.println("\n[3/5] Inicializando MFCC...");
    if (!mfcc_init()) {
        Serial.println("ERROR: Fallo mfcc_init()");
        while (1) delay(1000);
    }
    init_memory.mfcc_internal_kb = mfcc_get_internal_memory_bytes() / 1024.0f;
    Serial.printf("  MFCC internal: %.1f KB\n", init_memory.mfcc_internal_kb);

    Serial.println("\n[4/5] Cargando modelo...");
    if (!model_load(MODEL_PATH)) {
        Serial.println("ERROR: Fallo model_load()");
        while (1) delay(1000);
    }
    init_memory.model_buffer_kb = model_get_size_bytes() / 1024.0f;
    init_memory.tensor_arena_kb = model_get_arena_size_bytes() / 1024.0f;
    Serial.printf("  Model: %.1f KB  |  Arena: %.1f KB\n",
                  init_memory.model_buffer_kb, init_memory.tensor_arena_kb);

    // Calcular memoria total
    init_memory.psram_after_init_kb = get_psram_free_kb();
    init_memory.total_allocated_kb = init_memory.psram_initial_kb - init_memory.psram_after_init_kb;

    // -------------------------------------------------------------------------
    // 5. Inicializar profiler
    // -------------------------------------------------------------------------
    Serial.println("\n[5/5] Inicializando profiler...");
    if (!profiler_init(CSV_FILENAME)) {
        Serial.println("ERROR: Fallo profiler_init()");
        while (1) delay(1000);
    }

    // Guardar y mostrar perfil de memoria inicial
    profiler_log_init_memory(init_memory);
    profiler_print_init_memory(init_memory);

    Serial.println("\n========== SISTEMA LISTO ==========");
    Serial.printf("Iteraciones previas en CSV: %d\n", profiler_get_row_count());
    print_help();
}

// -----------------------------------------------------------------------------
// Comandos Serial
// -----------------------------------------------------------------------------

static void print_help() {
    Serial.println("\n=== COMANDOS DISPONIBLES ===");
    Serial.println("  d, dump   - Exportar CSV al Serial");
    Serial.println("  r, reset  - Borrar CSV y empezar de nuevo");
    Serial.println("  c, count  - Mostrar cantidad de iteraciones");
    Serial.println("  s, skip   - Saltar espera e iniciar grabación");
    Serial.println("  h, help   - Mostrar esta ayuda");
    Serial.println("  p, pause  - Pausar/reanudar el loop");
    Serial.println("============================\n");
}

static bool paused = false;

static void handle_serial_commands() {
    while (Serial.available()) {
        char cmd = Serial.read();

        switch (cmd) {
            case 'd':
                profiler_dump_csv();
                break;
            case 'r':
                profiler_reset_csv();
                iteration_count = 0;
                break;
            case 'c':
                Serial.printf("[Profiler] Iteraciones en CSV: %d\n", profiler_get_row_count());
                break;
            case 'h':
                print_help();
                break;
            case 'p':
                paused = !paused;
                Serial.printf("[Sistema] %s\n", paused ? "PAUSADO" : "REANUDADO");
                break;
            case 's':
                // Se maneja en el loop
                break;
            case '\n':
            case '\r':
                // Ignorar
                break;
            default:
                Serial.printf("[?] Comando '%c' no reconocido. Usa 'h' para ayuda.\n", cmd);
                break;
        }
    }
}

// -----------------------------------------------------------------------------
// Loop - Pipeline principal con profiling
// -----------------------------------------------------------------------------

void loop() {
    // Procesar comandos Serial
    handle_serial_commands();

    // Si está pausado, solo procesar comandos
    if (paused) {
        delay(100);
        return;
    }

    iteration_count++;

    Serial.printf("\n*** Iteración #%u - Preparate para hablar (3 seg, 's' para saltar) ***\n", iteration_count);

    // Espera con opción de saltar
    for (int i = 0; i < 30; i++) {  // 30 * 100ms = 3 segundos
        delay(100);
        if (Serial.available() && Serial.peek() == 's') {
            Serial.read();  // Consumir el 's'
            Serial.println("[!] Saltando espera...");
            break;
        }
    }

    // Estructura para métricas de esta iteración
    PipelineMetrics metrics = {0};
    metrics.iteration = iteration_count;
    metrics.timestamp_ms = millis();

    // Memoria al inicio de la iteración
    metrics.psram_free_kb = get_psram_free_kb();
    metrics.psram_used_kb = get_psram_total_kb() - metrics.psram_free_kb;
    metrics.dram_free_kb = get_dram_free_kb();

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

    metrics.time_capture_ms = millis() - t1;

    // -------------------------------------------------------------------------
    // Etapa 2: Normalizar
    // -------------------------------------------------------------------------
    unsigned long t2 = millis();

    audio_normalize(audio_buffer);

    metrics.time_normalize_ms = millis() - t2;

    // Estadísticas de audio
    AudioStats stats = audio_get_stats(audio_buffer);
    metrics.audio_rms = stats.rms;
    metrics.audio_peak_pos = stats.peak_pos;
    metrics.audio_peak_neg = stats.peak_neg;

    Serial.printf("[Audio] RMS: %.1f, Picos: [%d, %d]\n",
                  stats.rms, stats.peak_neg, stats.peak_pos);

    // -------------------------------------------------------------------------
    // Etapa 3: Extraer MFCCs
    // -------------------------------------------------------------------------
    unsigned long t3 = millis();

    mfcc_extract(audio_buffer, mfcc_buffer);

    metrics.time_mfcc_ms = millis() - t3;

    // -------------------------------------------------------------------------
    // Etapa 4: Inferencia
    // -------------------------------------------------------------------------
    unsigned long t4 = millis();

    EmotionResult result = model_predict(mfcc_buffer);

    metrics.time_inference_ms = millis() - t4;

    // -------------------------------------------------------------------------
    // Finalizar métricas
    // -------------------------------------------------------------------------
    metrics.time_total_ms = millis() - pipeline_start;
    metrics.emotion_index = result.index;
    metrics.confidence = result.confidence;

    // Guardar en CSV
    profiler_log_iteration(metrics);

    // Mostrar resultado
    print_result(result);

    // Mostrar métricas en Serial
    profiler_print_iteration(metrics);

    Serial.printf("\n[CSV] Datos guardados en %s\n", CSV_FILENAME);

    // Esperar antes del próximo ciclo
    delay(2000);
}
