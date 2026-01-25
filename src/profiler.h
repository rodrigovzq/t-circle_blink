#ifndef PROFILER_H
#define PROFILER_H

#include <stdint.h>
#include <stddef.h>

// =============================================================================
// Profiler - Métricas de memoria y tiempo para análisis
// =============================================================================

// Datos de una iteración del pipeline
struct PipelineMetrics {
    // Identificación
    uint32_t iteration;
    unsigned long timestamp_ms;

    // Memoria (KB)
    uint32_t psram_free_kb;
    uint32_t psram_used_kb;
    uint32_t dram_free_kb;

    // Tiempos (ms)
    unsigned long time_capture_ms;
    unsigned long time_normalize_ms;
    unsigned long time_mfcc_ms;
    unsigned long time_inference_ms;
    unsigned long time_total_ms;

    // Audio
    float audio_rms;
    int16_t audio_peak_pos;
    int16_t audio_peak_neg;

    // Resultado
    int emotion_index;
    float confidence;
};

// Datos de memoria de inicialización (una sola vez)
struct InitMemoryProfile {
    // PSRAM antes de cualquier allocación
    uint32_t psram_initial_kb;

    // Tamaño de cada buffer (KB)
    float audio_buffer_kb;
    float mfcc_buffer_kb;
    float mfcc_internal_kb;  // vReal + vImag + mel + dct + hamming
    float model_buffer_kb;
    float tensor_arena_kb;

    // PSRAM después de todas las allocaciones
    uint32_t psram_after_init_kb;
    uint32_t total_allocated_kb;
};

// Inicializa el profiler y abre/crea el archivo CSV en LittleFS
// filename: nombre del archivo CSV (ej: "/profiling.csv")
// Retorna true si OK
bool profiler_init(const char* filename);

// Registra el perfil de memoria de inicialización
void profiler_log_init_memory(const InitMemoryProfile& profile);

// Registra una iteración del pipeline
void profiler_log_iteration(const PipelineMetrics& metrics);

// Cierra el archivo CSV (flush)
void profiler_close();

// Imprime resumen de memoria de inicialización
void profiler_print_init_memory(const InitMemoryProfile& profile);

// Imprime métricas de una iteración
void profiler_print_iteration(const PipelineMetrics& metrics);

// Vuelca el contenido del CSV al Serial (para exportar)
void profiler_dump_csv();

// Borra el CSV y empieza de nuevo
void profiler_reset_csv();

// Retorna el número de líneas en el CSV (excluyendo header)
int profiler_get_row_count();

#endif // PROFILER_H
