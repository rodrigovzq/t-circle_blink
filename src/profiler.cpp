#include "profiler.h"
#include "config.h"
#include <Arduino.h>
#include <LittleFS.h>
#include "esp_heap_caps.h"

// =============================================================================
// Implementación - Profiler
// =============================================================================

static File csvFile;
static bool initialized = false;
static const char* csvFilename = nullptr;

bool profiler_init(const char* filename) {
    if (!LittleFS.begin(true)) {
        Serial.println("[Profiler] ERROR: No se pudo montar LittleFS");
        return false;
    }

    csvFilename = filename;

    // Verificar si el archivo existe para saber si escribir header
    bool writeHeader = !LittleFS.exists(filename);

    csvFile = LittleFS.open(filename, "a");  // Append mode
    if (!csvFile) {
        Serial.println("[Profiler] ERROR: No se pudo abrir archivo CSV");
        return false;
    }

    if (writeHeader) {
        // Escribir header del CSV
        csvFile.println(
            "iteration,"
            "timestamp_ms,"
            "psram_free_kb,"
            "psram_used_kb,"
            "dram_free_kb,"
            "time_capture_ms,"
            "time_normalize_ms,"
            "time_mfcc_ms,"
            "time_inference_ms,"
            "time_total_ms,"
            "audio_rms,"
            "audio_peak_pos,"
            "audio_peak_neg,"
            "emotion_index,"
            "confidence"
        );
        csvFile.flush();
        Serial.printf("[Profiler] CSV creado: %s\n", filename);
    } else {
        Serial.printf("[Profiler] CSV existente, agregando datos: %s\n", filename);
    }

    initialized = true;
    return true;
}

void profiler_log_init_memory(const InitMemoryProfile& profile) {
    if (!initialized) return;

    // Guardar perfil de memoria inicial en un archivo separado
    File initFile = LittleFS.open("/init_memory.txt", "w");
    if (initFile) {
        initFile.printf("PSRAM Initial: %u KB\n", profile.psram_initial_kb);
        initFile.printf("Audio Buffer: %.1f KB\n", profile.audio_buffer_kb);
        initFile.printf("MFCC Buffer: %.1f KB\n", profile.mfcc_buffer_kb);
        initFile.printf("MFCC Internal: %.1f KB\n", profile.mfcc_internal_kb);
        initFile.printf("Model Buffer: %.1f KB\n", profile.model_buffer_kb);
        initFile.printf("Tensor Arena: %.1f KB\n", profile.tensor_arena_kb);
        initFile.printf("PSRAM After Init: %u KB\n", profile.psram_after_init_kb);
        initFile.printf("Total Allocated: %u KB\n", profile.total_allocated_kb);
        initFile.close();
    }
}

void profiler_log_iteration(const PipelineMetrics& metrics) {
    if (!initialized || !csvFile) return;

    // Escribir línea CSV
    csvFile.printf(
        "%u,%lu,%u,%u,%u,%lu,%lu,%lu,%lu,%lu,%.2f,%d,%d,%d,%.4f\n",
        metrics.iteration,
        metrics.timestamp_ms,
        metrics.psram_free_kb,
        metrics.psram_used_kb,
        metrics.dram_free_kb,
        metrics.time_capture_ms,
        metrics.time_normalize_ms,
        metrics.time_mfcc_ms,
        metrics.time_inference_ms,
        metrics.time_total_ms,
        metrics.audio_rms,
        metrics.audio_peak_pos,
        metrics.audio_peak_neg,
        metrics.emotion_index,
        metrics.confidence
    );

    // Flush cada iteración para no perder datos
    csvFile.flush();
}

void profiler_close() {
    if (csvFile) {
        csvFile.close();
    }
    initialized = false;
}

void profiler_print_init_memory(const InitMemoryProfile& profile) {
    Serial.println("\n================================================================");
    Serial.println("MEMORY PROFILING - Inicialización");
    Serial.println("================================================================");

    Serial.printf("\nPSRAM Inicial:     %u KB\n", profile.psram_initial_kb);

    Serial.println("\nALLOCACIONES:");
    Serial.println("├── Audio");
    Serial.printf("│   └── audioBuffer:      %6.1f KB\n", profile.audio_buffer_kb);
    Serial.println("├── MFCC");
    Serial.printf("│   ├── mfccOutput:       %6.1f KB\n", profile.mfcc_buffer_kb);
    Serial.printf("│   └── internal buffers: %6.1f KB\n", profile.mfcc_internal_kb);
    Serial.println("├── TFLite");
    Serial.printf("│   ├── modelBuffer:      %6.1f KB\n", profile.model_buffer_kb);
    Serial.printf("│   └── tensorArena:      %6.1f KB\n", profile.tensor_arena_kb);
    Serial.println("└──────────────────────────────────");
    Serial.printf("   TOTAL ALLOCADO:    %6u KB\n", profile.total_allocated_kb);
    Serial.printf("   PSRAM RESTANTE:    %6u KB\n", profile.psram_after_init_kb);

    Serial.println("================================================================\n");
}

void profiler_print_iteration(const PipelineMetrics& metrics) {
    Serial.println("\n----------------------------------------------------------------");
    Serial.printf("ITERACIÓN #%u  |  timestamp: %lu ms\n", metrics.iteration, metrics.timestamp_ms);
    Serial.println("----------------------------------------------------------------");

    Serial.println("\nMEMORIA:");
    Serial.printf("  PSRAM libre: %u KB  |  usado: %u KB\n", metrics.psram_free_kb, metrics.psram_used_kb);
    Serial.printf("  DRAM libre:  %u KB\n", metrics.dram_free_kb);

    Serial.println("\nTIEMPOS:");
    Serial.printf("  Captura:     %4lu ms\n", metrics.time_capture_ms);
    Serial.printf("  Normalizar:  %4lu ms\n", metrics.time_normalize_ms);
    Serial.printf("  MFCC:        %4lu ms\n", metrics.time_mfcc_ms);
    Serial.printf("  Inferencia:  %4lu ms\n", metrics.time_inference_ms);
    Serial.printf("  ─────────────────────\n");
    Serial.printf("  TOTAL:       %4lu ms\n", metrics.time_total_ms);

    Serial.println("\nAUDIO:");
    Serial.printf("  RMS: %.1f  |  Picos: [%d, %d]\n",
                  metrics.audio_rms, metrics.audio_peak_neg, metrics.audio_peak_pos);

    Serial.printf("\nRESULTADO: %s (%.1f%%)\n",
                  EMOTION_LABELS[metrics.emotion_index], metrics.confidence * 100);

    Serial.println("----------------------------------------------------------------");
}

void profiler_dump_csv() {
    if (!csvFilename) {
        Serial.println("[Profiler] ERROR: No hay archivo CSV");
        return;
    }

    // Cerrar archivo actual si está abierto
    if (csvFile) {
        csvFile.close();
    }

    // Abrir en modo lectura
    File readFile = LittleFS.open(csvFilename, "r");
    if (!readFile) {
        Serial.println("[Profiler] ERROR: No se pudo abrir CSV para lectura");
        return;
    }

    Serial.println("\n========== CSV START ==========");

    // Leer y enviar línea por línea
    while (readFile.available()) {
        String line = readFile.readStringUntil('\n');
        Serial.println(line);
    }

    Serial.println("========== CSV END ==========\n");

    readFile.close();

    // Reabrir en modo append
    csvFile = LittleFS.open(csvFilename, "a");
}

void profiler_reset_csv() {
    Serial.println("[Profiler] Borrando CSV...");

    // Cerrar archivo actual
    if (csvFile) {
        csvFile.close();
    }

    // Borrar archivo
    if (LittleFS.exists(csvFilename)) {
        LittleFS.remove(csvFilename);
    }

    // Recrear con header
    csvFile = LittleFS.open(csvFilename, "w");
    if (csvFile) {
        csvFile.println(
            "iteration,"
            "timestamp_ms,"
            "psram_free_kb,"
            "psram_used_kb,"
            "dram_free_kb,"
            "time_capture_ms,"
            "time_normalize_ms,"
            "time_mfcc_ms,"
            "time_inference_ms,"
            "time_total_ms,"
            "audio_rms,"
            "audio_peak_pos,"
            "audio_peak_neg,"
            "emotion_index,"
            "confidence"
        );
        csvFile.flush();
        csvFile.close();

        // Reabrir en modo append
        csvFile = LittleFS.open(csvFilename, "a");
        Serial.println("[Profiler] CSV reiniciado");
    }

    initialized = true;
}

int profiler_get_row_count() {
    if (!csvFilename) return 0;

    // Cerrar temporalmente
    if (csvFile) {
        csvFile.close();
    }

    File readFile = LittleFS.open(csvFilename, "r");
    if (!readFile) return 0;

    int count = -1;  // -1 para no contar el header
    while (readFile.available()) {
        readFile.readStringUntil('\n');
        count++;
    }
    readFile.close();

    // Reabrir en modo append
    csvFile = LittleFS.open(csvFilename, "a");

    return count > 0 ? count : 0;
}
